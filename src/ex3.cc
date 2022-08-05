#include "cdf.hh"
#include "dnn_types.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "grapher.hh"
#include "http_server.hh"
#include "mmap.hh"
#include "network.hh"
#include "nngraph.hh"
#include "parser.hh"
#include "random.hh"
#include "sampler.hh"
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

  auto mynetwork_ptr = make_unique<DNN>();
  DNN& mynetwork = *mynetwork_ptr;

  // parse DNN from file
  {
    ReadOnlyFile dnn_on_disk { filename };
    Parser parser { dnn_on_disk };
    parser.object( mynetwork );
  }

  cerr << "Number of layers: " << mynetwork.num_layers << "\n";

  NetworkGraph<DNN> graph;
  graph.initialize( mynetwork );

  auto prng = get_random_engine();
  auto tempo_distribution = uniform_real_distribution<float>( 30, 240 ); // beats per minute
  auto noise_distribution = normal_distribution<float>( 0, note_timing_variation );

  const auto input_generator = [&]( DNN::M_input<1>& sample_input, DNN::M_output<1>& target_output ) {
    const float tempo = tempo_distribution( prng );
    target_output( 0, 0 ) = tempo;

    const float seconds_per_beat = 60.0 / tempo;

    auto offset_distribution = uniform_real_distribution<float>( 0, seconds_per_beat );
    const float offset = offset_distribution( prng );

    for ( unsigned int note_num = 0; note_num < 16; note_num++ ) {
      sample_input( note_num ) = offset + seconds_per_beat * note_num * ( 1 + noise_distribution( prng ) );
    }
  };

  DNN::M_input<BATCH_SIZE> input_batch;

  DNN::M_input<1> single_input;
  DNN::M_output<1> single_output;

  DNN::Activations<BATCH_SIZE> activations;

  /* listen for connections */
  TCPSocket server_socket {};
  server_socket.set_reuseaddr();

  try {
    server_socket.bind( { "0", 9090 } );
  } catch ( ... ) {
    server_socket.bind( { "0", 0 } );
  }

  server_socket.listen();
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

          graph.reset_activations();
          for ( size_t batch = 0; batch < num_batches; ++batch ) {
            // step 1: generate one batch of input
            for ( size_t elem = 0; elem < BATCH_SIZE; ++elem ) {
              input_generator( single_input, single_output );
              input_batch.row( elem ) = single_input;
            }

            // step 2: apply the network to the batch
            mynetwork.apply( input_batch, activations );

            // step 3: grab the activations
            graph.add_activations( activations );
          }
          the_response.body = graph.graph();
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
