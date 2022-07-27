#include "dnn_types.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "grapher.hh"
#include "http_server.hh"
#include "mmap.hh"
#include "network.hh"
#include "parser.hh"
#include "random.hh"
#include "sampler.hh"
#include "serdes.hh"
#include "socket.hh"

#include <iostream>

using namespace std;

static constexpr size_t mebi = 1024 * 1024;
static constexpr float note_timing_variation = 0.05; /* jitter = stddev of +/- 5% of the beat */

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

  const auto output_transformer = []( const auto& singleton, float& out ) { out = singleton( 0, 0 ); };

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

  using MySampler = Sampler<64, DNN, float>;
  MySampler::Output outputs;

  unsigned int frame_no = 0;

  events.add_rule(
    "Parse bytes from client buffer",
    [&] {
      if ( http_server.read( client_buffer, request ) ) {
        cerr << "Got request: " << request.request_target << "\n";
        HTTPResponse the_response;
        the_response.http_version = "HTTP/1.1";
        the_response.status_code = "200";
        the_response.reason_phrase = "OK";

        if ( request.request_target == "/" ) {
          the_response.headers.content_type = "text/html";
          the_response.headers.content_length = html_page.size();
          the_response.body = html_page;
        } else {
          the_response.headers.content_type = "image/svg+xml";
          MySampler::sample( 8, mynetwork, input_generator, output_transformer, outputs );
          const string info = "frame=" + to_string( frame_no++ ) + " layers=" + to_string( DNN::num_layers )
                              + " params=" + to_string( DNN::num_params )
                              + " points=" + to_string( outputs.size() );
          Graph graph { { 640, 480 }, { 0, 270 }, { 0, 270 }, info, "bpm (true)", "bpm (inferred)" };
          graph.graph( outputs );
          graph.finish();
          the_response.headers.content_length = graph.svg().size();
          the_response.body = move( graph.svg() );
        }
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
