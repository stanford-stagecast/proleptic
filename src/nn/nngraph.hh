#include <array>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "backprop.hh"
#include "inference.hh"
#include "network.hh"

static constexpr std::pair<float, float> init_range = { std::numeric_limits<double>::max(), 0 };

class AutoCDF
{
  static constexpr unsigned int num_cdf_points = 1000;

  std::vector<std::pair<float, float>> cdf_ {};

  std::pair<float, float> xrange_ { init_range }, yrange_ { init_range };

  std::string title_, xlabel_;

public:
  AutoCDF( const std::string_view title, const std::string_view xlabel );

  std::string graph( const unsigned int width, const unsigned int height, std::vector<float>& values );
};

template<NetworkT Network>
class NetworkGraph
{
  std::vector<std::vector<float>> weight_values_, bias_values_, activation_values_, weight_gradient_values_,
    bias_gradient_values_;
  std::vector<AutoCDF> weight_cdfs_ {}, bias_cdfs_ {}, activation_cdfs_ {}, weight_gradient_cdfs_ {},
    bias_gradient_cdfs_ {};
  std::vector<std::string> weight_svgs_, bias_svgs_;

  std::pair<float, float> io_xrange_ { init_range }, io_yrange_ { init_range };

public:
  NetworkGraph();
  void initialize( const Network& net );

  void reset_activations();

  template<int batch_size>
  void add_activations( const NetworkInference<Network, batch_size>& activations );

  void reset_gradients();

  template<int batch_size>
  void add_gradients( const NetworkBackPropagation<Network, batch_size>& activations );

  std::string io_graph( const std::vector<std::pair<float, float>>& target_vs_actual,
                        const std::string_view title,
                        const std::string_view quantity );

  std::vector<std::string> layer_graphs();
};
