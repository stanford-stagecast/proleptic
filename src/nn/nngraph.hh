#include <array>
#include <limits>
#include <string>
#include <utility>
#include <vector>

class AutoCDF
{
  static constexpr unsigned int num_cdf_points = 1000;
  static constexpr std::pair<float, float> init_range = { std::numeric_limits<double>::max(), 0 };

  std::vector<std::pair<float, float>> cdf_ {};

  std::pair<float, float> xrange_ { init_range }, yrange_ { init_range };

  std::string title_, xlabel_;

public:
  AutoCDF( const std::string_view title, const std::string_view xlabel );

  std::string graph( const unsigned int width, const unsigned int height, std::vector<float>& values );
};

template<class Network>
class NetworkGraph
{
  std::vector<std::vector<float>> weight_values_, bias_values_, activation_values_;
  std::vector<AutoCDF> weight_cdfs_ {}, bias_cdfs_ {}, activation_cdfs_ {};
  std::vector<std::string> weight_svgs_, bias_svgs_;

public:
  NetworkGraph();
  void initialize( const Network& net );

  void reset_activations();

  template<int T_batch_size>
  void add_activations( const typename Network::template Activations<T_batch_size>& activations );

  std::string graph();
};
