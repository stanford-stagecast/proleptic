#pragma once

#include <string>
#include <vector>

class Graph
{
  std::pair<double, double> image_size_, x_range_, y_range_;

  double x_user_to_image( double user_x ) const;
  double y_user_to_image( double user_y ) const;

  std::string svg_ {};

public:
  Graph( const std::pair<double, double> image_size,
         const std::pair<double, double> x_range,
         const std::pair<double, double> y_range,
         const std::string_view title,
         const std::string_view x_label,
         const std::string_view y_label );

  void graph( const std::vector<std::pair<float, float>>& data );

  void draw_identity_function( const std::string_view color, const unsigned int width );

  void finish();
  const std::string& svg() const { return svg_; }
};
