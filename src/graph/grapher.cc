#include "grapher.hh"

#include <charconv>
#include <cmath>
#include <stdexcept>
#include <vector>

using namespace std;

static constexpr double reserved_area_size = 64;

static unsigned int find_tic_spacing( const double extent )
{
  unsigned int tic_interval = 1;

  while ( true ) {
    if ( extent / tic_interval <= 15 ) {
      return tic_interval;
    }

    if ( extent / ( tic_interval * 2 ) <= 15 ) {
      return tic_interval * 2;
    }

    if ( extent / ( tic_interval * 5 ) <= 15 ) {
      return tic_interval * 5;
    }

    tic_interval *= 10;
  }
}

Graph::Graph( const pair<double, double> image_size,
              const pair<double, double> x_range,
              const pair<double, double> y_range,
              const string_view title,
              const string_view x_label,
              const string_view y_label )
  : image_size_( image_size )
  , x_range_( x_range )
  , y_range_( y_range )
{
  svg_.append( "<svg xmlns='http://www.w3.org/2000/svg' width='" + to_string( image_size.first ) + "' " );
  svg_.append( "height='" + to_string( image_size.second ) + "' viewbox='0 0 " );
  svg_.append( to_string( image_size.first ) + " " + to_string( image_size.second ) + "'>\n" );

  // https://developer.mozilla.org/en-US/docs/Web/SVG/Element/marker
  svg_.append( R"xml(
    <defs>
    <marker id="dot" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="10" markerHeight="10">
      <polygon points="5,0 10,5 5,10 0,5" fill="darkblue" fill-opacity="0.25" />
    </marker>
  </defs>
)xml" );

  /* make axes */
  svg_.append( "<polyline fill='none' stroke='#606060' stroke-width='1px' points='" );
  svg_.append( to_string( x_user_to_image( x_range_.first ) ) + ","
               + to_string( y_user_to_image( y_range_.first ) ) );
  svg_.append( " " );
  svg_.append( to_string( x_user_to_image( x_range_.second ) ) + ","
               + to_string( y_user_to_image( y_range.first ) ) );
  svg_.append( "' />" );

  svg_.append( "<polyline fill='none' stroke='#606060' stroke-width='1px' points='" );
  svg_.append( to_string( x_user_to_image( x_range_.first ) ) + ","
               + to_string( y_user_to_image( y_range_.first ) ) );
  svg_.append( " " );
  svg_.append( to_string( x_user_to_image( x_range_.first ) ) + ","
               + to_string( y_user_to_image( y_range_.second ) ) );
  svg_.append( "' />" );

  /* make tic marks and labels */
  {
    const unsigned int x_spacing = find_tic_spacing( x_range_.second - x_range_.first );
    auto x_lower_limit = lrint( floor( x_range_.first ) );
    x_lower_limit -= x_lower_limit % x_spacing;

    if ( x_lower_limit < x_range_.first ) {
      x_lower_limit += x_spacing;
    }

    auto tic = x_lower_limit;
    while ( tic <= x_range_.second ) {
      svg_.append( "<polyline fill='none' stroke='#606060' stroke-width='1px' points='"
                   + to_string( x_user_to_image( tic ) ) + "," + to_string( y_user_to_image( y_range_.first ) + 5 )
                   + " " + to_string( x_user_to_image( tic ) ) + ","
                   + to_string( y_user_to_image( y_range_.first ) ) + "' />" );

      svg_.append( "<text fill='#606060' font-size='14' x='" + to_string( x_user_to_image( tic ) ) + "' y='"
                   + to_string( y_user_to_image( y_range_.first ) + 8 )
                   + "' dominant-baseline='hanging' text-anchor='middle'>" + to_string( tic ) + "</text>" );
      tic += x_spacing;
    }
  }

  {
    const unsigned int y_spacing = find_tic_spacing( y_range_.second - y_range_.first );
    auto y_lower_limit = lrint( floor( y_range_.first ) );
    y_lower_limit -= y_lower_limit % y_spacing;

    if ( y_lower_limit < y_range_.first ) {
      y_lower_limit += y_spacing;
    }

    auto tic = y_lower_limit;
    while ( tic <= y_range_.second ) {
      svg_.append( "<polyline fill='none' stroke='#606060' stroke-width='1px' points='"
                   + to_string( x_user_to_image( x_range_.first ) - 5 ) + "," + to_string( y_user_to_image( tic ) )
                   + " " + to_string( x_user_to_image( x_range_.first ) ) + ","
                   + to_string( y_user_to_image( tic ) ) + "' />" );

      svg_.append( "<text fill='#606060' font-size='14' y='" + to_string( y_user_to_image( tic ) ) + "' x='"
                   + to_string( x_user_to_image( x_range_.first ) - 8 )
                   + "' dominant-baseline='middle' text-anchor='end'>" + to_string( tic ) + "</text>" );
      tic += y_spacing;
    }
  }

  svg_.append( "<text text-anchor='middle' dominant-baseline='hanging' font-size='14' y='" + to_string( 0 )
               + "' x= '" + to_string( image_size_.first / 2 ) + "' > " );
  svg_.append( title );
  svg_.append( "</text>" );

  svg_.append( "<text text-anchor='middle' dominant-baseline='auto' x='"
               + to_string( x_user_to_image( ( x_range_.first + x_range_.second ) / 2 ) ) + "' y='"
               + to_string( image_size_.second - 10 ) + "' fill='#606060' font-size='14'>" );
  svg_.append( x_label );
  svg_.append( "</text>" );

  svg_.append( "<text text-anchor='middle' transform='translate(8, "
               + to_string( y_user_to_image( ( y_range_.first + y_range_.second ) / 2 ) )
               + ") rotate(270)' dominant-baseline='middle' fill='#606060' font-size='14'>" );
  svg_.append( y_label );
  svg_.append( "</text>" );
}

void Graph::finish()
{
  svg_.append( "\n</svg>\n" );
}

void Graph::draw_identity_function( const string_view color, const unsigned int width )
{
  svg_.append( "<polyline fill='none' stroke='" );
  svg_.append( color );
  svg_.append( "' stroke-width='" + to_string( width ) + "px' points='" );
  svg_.append( to_string( x_user_to_image( x_range_.first ) ) + ","
               + to_string( y_user_to_image( x_range_.first ) ) );
  svg_.append( " " );
  svg_.append( to_string( x_user_to_image( x_range_.second ) ) + ","
               + to_string( y_user_to_image( x_range_.second ) ) );
  svg_.append( "' />" );
}

double Graph::x_user_to_image( double user_x ) const
{
  const double user_domain_length = x_range_.second - x_range_.first;
  const double position_in_graph = ( user_x - x_range_.first ) / user_domain_length;
  return reserved_area_size + position_in_graph * ( image_size_.first - reserved_area_size );
}

double Graph::y_user_to_image( double user_y ) const
{
  const double user_range_length = y_range_.second - y_range_.first;
  const double position_in_graph = ( user_y - y_range_.first ) / user_range_length;
  return ( image_size_.second - reserved_area_size ) * ( 1 - position_in_graph );
}

void Graph::begin_points()
{
  svg_.append( "<polyline fill='none' stroke='none' marker-start='url(#dot)' marker-mid='url(#dot)' "
               "marker-end='url(#dot)' points='" );
}

void Graph::begin_line()
{
  svg_.append( "<polyline fill='none' stroke='darkblue' points='" );
}

void Graph::draw_points( const vector<pair<float, float>>& points )
{
  begin_points();
  add_vertices( points );
}

void Graph::draw_line( const vector<pair<float, float>>& points )
{
  begin_line();
  add_vertices( points );
}

void Graph::add_vertices( const vector<pair<float, float>>& points )
{
  const size_t current_size = svg_.size();
  svg_.resize( svg_.size() + points.size() * 20 );

  char* next_point = svg_.data() + current_size;
  char* const last_byte = svg_.data() + svg_.size();

  const chars_format fmt { chars_format::fixed };

  for ( const auto& pt : points ) {
    const auto resx = to_chars( next_point, last_byte, x_user_to_image( pt.first ), fmt, 2 );
    if ( resx.ec != errc() ) {
      throw runtime_error( "to_chars failure" );
    }
    *resx.ptr = ',';
    next_point = resx.ptr + 1;

    const auto resy = to_chars( next_point, last_byte, y_user_to_image( pt.second ), fmt, 2 );
    if ( resy.ec != errc() ) {
      throw runtime_error( "to_chars failure" );
    }
    *resy.ptr = ' ';
    next_point = resy.ptr + 1;
  }

  svg_.resize( next_point - svg_.data() );

  svg_.append( "'/>\n" );
}
