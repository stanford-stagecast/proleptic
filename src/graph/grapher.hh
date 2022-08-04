#pragma once

#include <string>
#include <vector>

class Graph
{
  std::pair<double, double> image_size_, x_range_, y_range_;

  double x_user_to_image( const double user_x ) const;
  double y_user_to_image( const double user_y ) const;

  std::string svg_ {};

  void begin_points();
  void begin_line();

  void add_vertices( const std::vector<std::pair<float, float>>& data );
  void add_x_tic( const double tic, const std::string_view label );
  void add_y_tic( const double tic, const std::string_view label );

public:
  Graph( const std::pair<double, double> image_size,
         const std::pair<double, double> x_range,
         const std::pair<double, double> y_range,
         const std::string_view title,
         const std::string_view x_label,
         const std::string_view y_label );

  void draw_points( const std::vector<std::pair<float, float>>& data );
  void draw_line( const std::vector<std::pair<float, float>>& data );

  void draw_identity_function( const std::string_view color, const unsigned int width );

  void finish();
  const std::string& svg() const { return svg_; }
};
