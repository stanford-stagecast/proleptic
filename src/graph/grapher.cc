#include "grapher.hh"

#include <vector>

using namespace std;

Graph::Graph( const pair<double, double> image_size,
              const pair<double, double> x_range,
              const pair<double, double> y_range )
  : image_size_( image_size )
  , x_range_( x_range )
  , y_range_( y_range )
  , cairo_( image_size.first, image_size.second )
  , diamond_( [&]() {
    cairo_new_path( cairo_ );
    cairo_move_to( cairo_, 0, 0 );
    cairo_rel_move_to( cairo_, 0, -5 );
    cairo_rel_line_to( cairo_, 5, 5 );
    cairo_rel_line_to( cairo_, -5, 5 );
    cairo_rel_line_to( cairo_, -5, -5 );
    cairo_close_path( cairo_ );
    return Cairo::Path( cairo_ );
  }() )
{
  cairo_identity_matrix( cairo_ );
  cairo_set_source_rgba( cairo_, 0.5, 0.2, 0.5, 1 );
}

double Graph::x_user_to_image( double user_x ) const
{
  const double user_domain_length = x_range_.second - x_range_.first;
  const double position_in_graph = ( user_x - x_range_.first ) / user_domain_length;
  return position_in_graph * image_size_.first;
}

double Graph::y_user_to_image( double user_y ) const
{
  const double user_range_length = y_range_.second - y_range_.first;
  const double position_in_graph = ( user_y - y_range_.first ) / user_range_length;
  return image_size_.second - position_in_graph * image_size_.second;
}

void Graph::graph( const vector<pair<OneFloatM, OneFloatM>>& points )
{
  for ( const auto& pt : points ) {
    cairo_identity_matrix( cairo_ );
    cairo_translate( cairo_, x_user_to_image( pt.first( 0, 0 ) ), y_user_to_image( pt.second( 0, 0 ) ) );
    cairo_append_path( cairo_, diamond_ );
    cairo_fill( cairo_ );
  }
}
