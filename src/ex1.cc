#include <charconv>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "dnn_types.hh"
#include "grapher.hh"
#include "mmap.hh"
#include "network.hh"
#include "parser.hh"
#include "random.hh"
#include "sampler.hh"
#include "serdes.hh"

using namespace std;

static constexpr float note_timing_variation = 0.05; /* jitter = stddev of +/- 5% of the beat */

void program_body( const string& filename, const string& iterations_s )
{
  ios::sync_with_stdio( false );

  auto mynetwork_ptr = make_unique<DNN>();
  DNN& mynetwork = *mynetwork_ptr;
  const size_t iterations = stoi( iterations_s );

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

  using MySampler = Sampler<64, DNN, float>;

  MySampler::Output outputs;

  MySampler::sample( iterations, mynetwork, input_generator, output_transformer, outputs );

  cerr << "total outputs: " << outputs.size() << "\n";

  Graph graph { { 640, 480 },  { 0, 270 },      { 0, 270 }, "Scatter plot for BPM prediction",
                "bpm (truth)", "bpm (inferred)" };

  graph.draw_identity_function( "black", 3 );
  graph.draw_points( outputs );
  graph.draw_identity_function( "white", 1 );

  graph.finish();
  cout << graph.svg();
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc != 3 ) {
    cerr << "Usage: " << argv[0] << " filename iterations\n";
    return EXIT_FAILURE;
  }

  try {
    program_body( argv[1], argv[2] );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
