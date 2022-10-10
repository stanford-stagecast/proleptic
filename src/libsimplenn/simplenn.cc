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
  DNN_timestamp network {};
};

SimpleNN::SimpleNN( const string& filename )
{
  data_ = make_unique<NetworkData>();
  DNN_timestamp& nn = data_->network;

  {
    ReadOnlyFile dnn_on_disk { filename };
    Parser parser { dnn_on_disk };
    parser.object( nn );
  }
}

float SimpleNN::predict_next_timestamp( const std::array<float, 16>& past_timestamps )
{
  using Infer = NetworkInference<DNN_timestamp, 1>;
  using Input = typename Infer::Input;
  Input input( past_timestamps.data() );
  Infer infer;
  infer.apply( data_->network, input );
  float timestamp = infer.output()( 1 );
  return timestamp;
}

SimpleNN::~SimpleNN() = default;
