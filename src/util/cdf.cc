#include <algorithm>
#include <cmath>

#include "cdf.hh"

using namespace std;

void make_cdf( vector<float>& values, const unsigned int num_samples, vector<pair<float, float>>& cdf )
{
  sort( values.begin(), values.end() );
  const double bin_size = 1.0 / double( num_samples );
  for ( double cum_fraction = 0.0; cum_fraction < 1.0; cum_fraction += bin_size ) {
    const size_t index = lrint( floor( values.size() * cum_fraction ) );
    const double actual_fraction = double( index ) / double( values.size() - 1 );
    cdf.emplace_back( values.at( index ), actual_fraction );
  }

  if ( not values.empty() ) {
    cdf.emplace_back( values.back(), 1.0 );
  }
}
