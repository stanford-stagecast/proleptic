#include <array>
#include <limits>
#include <string>
#include <utility>
#include <vector>

class AutoCDF
{
  static constexpr unsigned int num_cdf_points = 1000;
  static constexpr std::pair<float, float> init_range = { std::numeric_limits<double>::max(), 0 };

  std::vector<float> values_ {};
  std::vector<std::pair<float, float>> cdf_ {};

  std::pair<float, float> xrange_ { init_range }, yrange_ { init_range };

  std::string title_, xlabel_;

public:
  AutoCDF( const std::string_view title, const std::string_view xlabel );

  std::string graph( const unsigned int width, const unsigned int height );

  std::vector<float>& values() { return values_; }
};

template<class Network>
class NetworkGraph
{
  std::vector<AutoCDF> weights_, biases_;
  std::vector<std::string> weight_svgs_, bias_svgs_;

public:
  NetworkGraph();
  void initialize( const Network& net );

  std::string graph();
};
