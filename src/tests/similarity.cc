#include <Eigen/Dense>

#include "exception.hh"
#include "minhash.hh"
#include "random.hh"
#include "similarity.hh"

#include <assert.h>
#include <iostream>
#include <set>
#include <sstream>

using namespace std;

class check_failed : public runtime_error
{
public:
  check_failed( string test_name )
    : runtime_error( ( test_name ).c_str() )
  {}
};

// This is pretty much just assert with a custom exception type
#define check( EXPR )                                                                                              \
  {                                                                                                                \
    if ( !( EXPR ) ) {                                                                                             \
      stringstream ss;                                                                                             \
      ss << __FILE__ << ":" << __LINE__ << " (" << __FUNCTION__ << "): \n";                                        \
      ss << "\tCheck <" << #EXPR << "> failed.";                                                                   \
      throw check_failed( ss.str() );                                                                              \
    }                                                                                                              \
  }

#define check_eq( A, B )                                                                                           \
  {                                                                                                                \
    const auto x = ( A );                                                                                          \
    const auto y = ( B );                                                                                          \
    if ( x != y ) {                                                                                                \
      stringstream ss;                                                                                             \
      ss << "Check failed.\n";                                                                                     \
      ss << "\tlhs expr: " << #A << "\n";                                                                          \
      ss << "\trhs expr: " << #B << "\n";                                                                          \
      ss << "\tlhs: " << x << "\n";                                                                                \
      ss << "\trhs: " << y << "\n";                                                                                \
      ss << "\tfile: " << __FILE__ << "\n";                                                                        \
      ss << "\tline: " << __LINE__ << "\n";                                                                        \
      ss << "\tfunction: " << __FUNCTION__ << "\n";                                                                \
      throw check_failed( ss.str() );                                                                              \
    }                                                                                                              \
  }

#define check_close( A, B, T )                                                                                     \
  {                                                                                                                \
    const auto x = ( A );                                                                                          \
    const auto y = ( B );                                                                                          \
    if ( abs( x - y ) > T ) {                                                                                      \
      stringstream ss;                                                                                             \
      ss << "Check failed.\n";                                                                                     \
      ss << "\tlhs expr: " << #A << "\n";                                                                          \
      ss << "\trhs expr: " << #B << "\n";                                                                          \
      ss << "\tlhs: " << x << "\n";                                                                                \
      ss << "\trhs: " << y << "\n";                                                                                \
      ss << "\tdifference: " << x - y << "\n";                                                                     \
      ss << "\tthreshold: " << T << "\n";                                                                          \
      ss << "\tfile: " << __FILE__ << "\n";                                                                        \
      ss << "\tline: " << __LINE__ << "\n";                                                                        \
      ss << "\tfunction: " << __FUNCTION__ << "\n";                                                                \
      throw check_failed( ss.str() );                                                                              \
    }                                                                                                              \
  }

#define check_approx( A, B ) check_close( A, B, 0.05 )

static constexpr size_t N_HASHES = 1024;
using Hasher = MinHash<uint32_t, N_HASHES, 100003, 65536>;

template<size_t N>
float mh( const Hasher& hasher, const array<uint32_t, N>& a, const array<uint32_t, N>& b )
{
  Eigen::Vector<uint32_t, N> A( a.data() );
  Eigen::Vector<uint32_t, N> B( b.data() );
  return minhash_similarity<uint32_t, N_HASHES>( hasher.minhash<N>( A ), hasher.minhash<N>( B ) );
}

template<size_t N>
float ji( const array<uint32_t, N>& a, const array<uint32_t, N>& b )
{
  return jaccard_index<uint32_t, N>( a, b );
}

bool close( float x, float y, float threshold = 0.01 )
{
  return abs( x - y ) < threshold;
}

void program_body()
{
  ios::sync_with_stdio( false );

  // This is the baseline, so if this doesn't work something is very wrong...
  check_eq( ji<1>( { 0 }, { 0 } ), 1 );
  check_eq( ji<2>( { 0, 1 }, { 0, 1 } ), 1 );
  check_eq( ji<2>( { 0, 1 }, { 0, 2 } ), 1 / 3.f );
  check_eq( ji<8>( { 0, 1, 2, 3, 4, 5, 6, 7 }, { 0, 1, 2, 3, 4, 5, 6, 100 } ), 7 / 9.f );
  check_eq( ji<8>( { 0, 1, 2, 3, 4, 5, 6, 7 }, { 0, 1, 2, 3, 4, 5, 100, 101 } ), 6 / 10.f );
  check_eq( ji<8>( { 100, 101, 102, 103, 104, 105, 106, 107 }, { 0, 1, 2, 3, 4, 5, 6, 7 } ), 0 );
  check_eq( ji<3>( { 512, 256, 128 }, { 64, 128, 256 } ), 0.5 );

  auto prng = get_random_engine();
  prng.seed( 20221201 );

  Hasher hasher( prng );

#define compare( N, ... ) check_approx( mh<N>( hasher, __VA_ARGS__ ), ji<N>( __VA_ARGS__ ) )

  compare( 1, { 0 }, { 0 } );
  compare( 2, { 0, 1 }, { 0, 1 } );
  compare( 2, { 0, 1 }, { 0, 2 } );
  compare( 8, { 0, 1, 2, 3, 4, 5, 6, 7 }, { 0, 1, 2, 3, 4, 5, 6, 100 } );
  compare( 8, { 0, 1, 2, 3, 4, 5, 6, 7 }, { 0, 1, 2, 3, 4, 5, 100, 101 } );
  compare( 8, { 100, 101, 102, 103, 104, 105, 106, 107 }, { 0, 1, 2, 3, 4, 5, 6, 7 } );
  compare( 3, { 512, 256, 128 }, { 64, 128, 256 } );
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc != 1 ) {
    cerr << "Usage: " << argv[0] << "\n";
    return EXIT_FAILURE;
  }

  try {
    program_body();
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
