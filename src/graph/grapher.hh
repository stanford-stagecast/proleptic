#pragma once

#include <Eigen/Dense>
#include <vector>

class Graph
{
  std::pair<double, double> image_size_, x_range_, y_range_;

  using OneFloatM = Eigen::Matrix<float, 1, 1>;

  double x_user_to_image( double user_x ) const;
  double y_user_to_image( double user_y ) const;

  std::string svg_ {};

public:
  Graph( const std::pair<double, double> image_size,
         const std::pair<double, double> x_range,
         const std::pair<double, double> y_range );

  void graph( const std::vector<std::pair<OneFloatM, OneFloatM>>& data );

  void finish();
  const std::string& svg() const { return svg_; }
};
