#include "cdf.hh"
#include "dnn_types.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "grapher.hh"
#include "http_server.hh"
#include "inference.hh"
#include "mmap.hh"
#include "network.hh"
#include "nngraph.hh"
#include "parser.hh"
#include "random.hh"
#include "serdes.hh"
#include "socket.hh"

#include <iostream>

using namespace std;

static constexpr size_t mebi = 1024 * 1024;
static constexpr float note_timing_variation = 0.05; /* jitter = stddev of +/- 5% of the beat */

static constexpr size_t num_batches = 8;

static const string html_page = R"html(<!doctype html>
<html>
<body>
  <div id='image'></div>

  <script>
    image_element = document.getElementById('image');
    async function load_graph() {
	image_element.innerHTML = await fetch('/input-output.svg').then(response => response.text());
    };

    window.onload = function(e) { setInterval(load_graph, 100) };
  </script>
</body>
</html>

)html";

void program_body( const string& filename )
{
  ios::sync_with_stdio( false );

  auto mynetwork_ptr = make_unique<DNN_timestamp>();
  DNN_timestamp& mynetwork = *mynetwork_ptr;

  // parse DNN_timestamp from file
  {
    ReadOnlyFile dnn_on_disk { filename };
    Parser parser { dnn_on_disk };
    parser.object( mynetwork );
  }

  cerr << "Number of layers: " << mynetwork.num_layers << "\n";

  cerr << "cp-1\n";
  NetworkGraph<DNN_timestamp> graph;
  cerr << "cp0\n";
  graph.initialize( mynetwork );
  cerr << "cp1\n";

  auto prng = get_random_engine();
  auto tempo_distribution = uniform_real_distribution<float>( 30, 240 ); // beats per minute
  auto noise_distribution = normal_distribution<float>( 0, note_timing_variation );

  using SingleInput = NetworkInference<DNN_timestamp, 1>::Input;
  using SingleOutput = NetworkInference<DNN_timestamp, 1>::Output;

  const auto input_generator = [&]( SingleInput& sample_input, SingleOutput& target_output ) {
    const float tempo = tempo_distribution( prng );
    target_output( 0, 0 ) = tempo;

    const float seconds_per_beat = 60.0 / tempo;

    auto offset_distribution = uniform_real_distribution<float>( 0, seconds_per_beat );
    const float offset = offset_distribution( prng );

    for ( unsigned int note_num = 0; note_num < 16; note_num++ ) {
      sample_input( note_num ) = offset + seconds_per_beat * note_num * ( 1 + noise_distribution( prng ) );
    }

    // QZ: added
    target_output( 0, 1 ) = sample_input( 0 ) - seconds_per_beat;
  };

  // const auto output_transformer = []( const auto& singleton, float& out ) { out = singleton( 0, 0 ); };
  const auto output_transformer = []( const auto& singleton, float& out ) { out = singleton( 0, 1 ); };

  NetworkInference<DNN_timestamp, BATCH_SIZE> inference;
  NetworkInference<DNN_timestamp, BATCH_SIZE>::Input input_batch;
  SingleInput single_input;
  SingleOutput single_output;

  vector<pair<float, float>> target_actual_inference;

  /* listen for connections */
  TCPSocket server_socket {};
  cerr << "cp2\n";
  server_socket.set_reuseaddr();

  try {
    server_socket.bind( { "0", 12354 } );
  } catch ( ... ) {
    server_socket.bind( { "0", 0 } );
  }

  server_socket.listen();
  cerr << "cp3\n";
  cerr << "Listening on port " << server_socket.local_address().port() << "\n";

  EventLoop events;

  auto connection = server_socket.accept();
  cerr << "Connection received from " << connection.peer_address().to_string() << "\n";

  RingBuffer client_buffer { mebi }, server_buffer { mebi };

  HTTPServer http_server;
  HTTPRequest request;

  events.add_rule(
    "Read bytes from client",
    connection,
    Direction::In,
    [&] { client_buffer.push_from_fd( connection ); },
    [&] { return not client_buffer.writable_region().empty(); } );

  const pair<float, float> init { numeric_limits<float>::max(), 0 };
  vector<pair<float, float>> weight_range( mynetwork.num_layers, init );
  vector<pair<float, float>> bias_range( mynetwork.num_layers, init );
  vector<pair<float, float>> activation_range( mynetwork.num_layers, init );

  events.add_rule(
    "Parse bytes from client buffer",
    [&] {
      if ( http_server.read( client_buffer, request ) ) {
        HTTPResponse the_response;
        the_response.http_version = "HTTP/1.1";
        the_response.status_code = "200";
        the_response.reason_phrase = "OK";

        if ( request.request_target == "/" ) {
          the_response.headers.content_type = "text/html";
          the_response.body = html_page;
        } else {
          the_response.headers.content_type = "image/svg+xml";

          target_actual_inference.clear();
          graph.reset_activations();

          for ( size_t batch = 0; batch < num_batches; ++batch ) {
            // step 1: generate one batch of input
            for ( size_t elem = 0; elem < BATCH_SIZE; ++elem ) {
              input_generator( single_input, single_output );
              input_batch.row( elem ) = single_input;
              target_actual_inference.emplace_back();
              output_transformer( single_output, target_actual_inference.at( batch * BATCH_SIZE + elem ).first );
            }

            // step 2: apply the network to the batch
            inference.apply( mynetwork, input_batch );

            // step 3: grab the actual inference outputs
            for ( unsigned int elem = 0; elem < BATCH_SIZE; ++elem ) {
              output_transformer( inference.output().row( elem ),
                                  target_actual_inference.at( batch * BATCH_SIZE + elem ).second );
            }

            // step 4: grab all the activations
            graph.add_activations( inference );
          }

          the_response.body.append( graph.io_graph( target_actual_inference, "Target vs. Inferred", "seconds" ) );
          the_response.body.append( "<p>" );

          auto layer_graphs = graph.layer_graphs();
          for ( const auto& layer : layer_graphs ) {
            the_response.body.append( layer );
            the_response.body.append( "<p>" );
          }
        }
        the_response.headers.content_length = the_response.body.size();
        http_server.push_response( move( the_response ) );
      }
    },
    [&] { return not client_buffer.readable_region().empty(); } );

  events.add_rule(
    "Serialize bytes from server into server buffer",
    [&] { http_server.write( server_buffer ); },
    [&] { return not http_server.responses_empty() and not server_buffer.writable_region().empty(); } );

  events.add_rule(
    "Write bytes to client",
    connection,
    Direction::Out,
    [&] { server_buffer.pop_to_fd( connection ); },
    [&] { return not server_buffer.readable_region().empty(); } );

  while ( events.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  cerr << "Exiting...\n";
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc != 2 ) {
    cerr << "Usage: " << argv[0] << " filename\n";
    return EXIT_FAILURE;
  }

  try {
    program_body( argv[1] );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
