#include "simplenn.hh"
#include "dnn_types.hh"
#include "inference.hh"
#include "mmap.hh"
#include "serdes.hh"

using namespace std;

// This is a hack so the client code doesn't have to include any headers except "simplenn.hh" (especially
// since the actual type of "DNN" might change).
struct NetworkData
{
  DNN network {};
};

SimpleNN::SimpleNN( const string& filename )
{
  data_ = make_unique<NetworkData>();
  DNN& nn = data_->network;

  {
    ReadOnlyFile dnn_on_disk { filename };
    Parser parser { dnn_on_disk };
    parser.object( nn );
  }
}

float SimpleNN::predict_next_timestamp( const std::array<float, 16>& past_timestamps )
{
  using Infer = NetworkInference<DNN, 1>;
  using Input = typename Infer::Input;
  Input input( past_timestamps.data() );
  Infer infer;
  infer.apply( data_->network, input );
  float tempo = infer.output()( 0 );
  return past_timestamps[0] - ( 60.0 / tempo );
}
