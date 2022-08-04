#include "cdf.hh"
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

static constexpr size_t batch_size = 64;
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

template<class T>
struct GetWeights
{
  static constexpr auto& get( const T& net ) { return net.first_layer().weights(); }
};

template<class T>
struct GetBiases
{
  static constexpr auto& get( const T& net ) { return net.first_layer().biases(); }
};

template<class T>
struct GetActivations
{
  static constexpr auto& get( const T& act ) { return act.first_layer(); }
};

template<template<typename> typename Getter, class T>
void flatten( const T& obj, vector<vector<float>>& vec, const unsigned int layer_no = 0 )
{
  while ( layer_no >= vec.size() ) {
    vec.emplace_back();
  }

  auto& this_vec = vec.at( layer_no );

  const auto& value_matrix = Getter<T>::get( obj );

  for ( unsigned int i = 0; i < value_matrix.size(); ++i ) {
    const float value = *( value_matrix.data() + i );
    this_vec.emplace_back( value );
  }

  if constexpr ( T::is_last_layer ) {
    return;
  } else {
    flatten<Getter>( obj.rest(), vec, layer_no + 1 );
  }
}

void clear_all( vector<vector<float>>& vec )
{
  for ( auto& x : vec ) {
    x.clear();
  }
}

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

  vector<vector<float>> flat_weights, flat_biases, flat_activations;
  vector<pair<float, float>> weights_cdf, biases_cdf, activations_cdf;
  flatten<GetWeights>( mynetwork, flat_weights );
  flatten<GetBiases>( mynetwork, flat_biases );

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

  DNN::M_input<batch_size> input_batch;

  DNN::M_input<1> single_input;
  DNN::M_output<1> single_output;

  DNN::Activations<batch_size> activations;

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

  vector<pair<float, float>> weight_range( mynetwork.num_layers );
  vector<pair<float, float>> bias_range( mynetwork.num_layers );
  vector<pair<float, float>> activation_range( mynetwork.num_layers );

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

          clear_all( flat_activations );

          for ( size_t batch = 0; batch < num_batches; ++batch ) {
            // step 1: generate one batch of input
            for ( size_t elem = 0; elem < batch_size; ++elem ) {
              input_generator( single_input, single_output );
              input_batch.row( elem ) = single_input;
            }

            // step 2: apply the network to the batch
            mynetwork.apply( input_batch, activations );

            // step 3: grab the activations
            flatten<GetActivations>( activations, flat_activations );
          }

          for ( unsigned int layer_no = 0; layer_no < mynetwork.num_layers; ++layer_no ) {
            weights_cdf.clear();
            biases_cdf.clear();
            activations_cdf.clear();
            make_cdf( flat_weights.at( layer_no ), 1000, weights_cdf );
            make_cdf( flat_biases.at( layer_no ), 1000, biases_cdf );
            make_cdf( flat_activations.at( layer_no ), 1000, activations_cdf );

            {
              auto& domain = weight_range.at( layer_no );
              if ( weights_cdf.empty() ) {
                abort();
              }
              domain = { min( domain.first, weights_cdf.front().first ),
                         max( domain.second, weights_cdf.back().first ) };

              Graph graph { { 640, 480 }, domain,
                            { 0, 1 },     "CDF of weights @ layer " + to_string( layer_no ),
                            "weight",     "cumulative fraction" };
              graph.draw_line( weights_cdf );
              graph.finish();
              the_response.body.append( graph.svg() );
            }

            {
              auto& domain = bias_range.at( layer_no );

              if ( biases_cdf.empty() ) {
                abort();
              }

              domain
                = { min( domain.first, biases_cdf.front().first ), max( domain.second, biases_cdf.back().first ) };

              Graph graph { { 640, 480 }, domain,
                            { 0, 1 },     "CDF of biases @ layer " + to_string( layer_no ),
                            "bias",       "cumulative fraction" };
              graph.draw_line( biases_cdf );
              graph.finish();
              the_response.body.append( graph.svg() );
            }

            {
              auto& domain = activation_range.at( layer_no );
              domain = { min( domain.first, activations_cdf.front().first ),
                         max( domain.second, activations_cdf.back().first ) };

              Graph graph { { 640, 480 }, domain,
                            { 0, 1 },     "CDF of activations @ layer " + to_string( layer_no ),
                            "activation", "cumulative fraction" };
              graph.draw_line( activations_cdf );
              graph.finish();
              the_response.body.append( graph.svg() );
            }

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
