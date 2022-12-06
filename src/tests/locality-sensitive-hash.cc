#include "locality_sensitive_hash.hh"

#include "exception.hh"
#include "random.hh"

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

#define BANDS 16
#define ROWS 4
#define HASHES ( ROWS * BANDS )

using MH = MinHash<uint32_t, HASHES, 10007, 8192>;
using LSH = LocalitySensitiveHash<uint32_t, ROWS, BANDS, 10007, 8192>;

template<size_t N>
Eigen::Vector<uint32_t, BANDS> get_buckets( MH minhash, LSH lsh, Eigen::Vector<uint32_t, N> a )
{
  const Eigen::Vector<uint32_t, HASHES> a_sig = minhash.minhash<N>( a );
  const Eigen::Vector<uint32_t, BANDS> a_buckets = lsh.get_buckets( a_sig );
  return a_buckets;
}

template<size_t N>
bool would_match( MH minhash, LSH lsh, array<uint32_t, N> a, array<uint32_t, N> b )
{
  Eigen::Vector<uint32_t, N> am( a.data() );
  Eigen::Vector<uint32_t, N> bm( b.data() );

  const auto a_buckets = get_buckets<N>( minhash, lsh, am );
  const auto b_buckets = get_buckets<N>( minhash, lsh, bm );

  std::set<uint32_t> a_bucket_set( a_buckets.begin(), a_buckets.end() );
  std::set<uint32_t> b_bucket_set( b_buckets.begin(), b_buckets.end() );

  std::vector<uint32_t> intersection( BANDS * 2 );
  auto end = std::set_intersection(
    a_bucket_set.begin(), a_bucket_set.end(), b_bucket_set.begin(), b_bucket_set.end(), intersection.begin() );
  intersection.resize( end - intersection.begin() );

  return !intersection.empty();
}

void program_body()
{
  ios::sync_with_stdio( false );

  auto prng = get_random_engine();
  prng.seed( 20221201 );

  MinHash<uint32_t, HASHES, 10007, 8192> minhash( prng );
  LocalitySensitiveHash<uint32_t, ROWS, BANDS, 10007, 8192> lsh( prng );

#define should_match( N, ... ) check( would_match<N>( minhash, lsh, __VA_ARGS__ ) )
#define should_error( N, ... ) check( !would_match<N>( minhash, lsh, __VA_ARGS__ ) )

  should_match( 1, { 0 }, { 0 } );
  should_match( 2, { 0, 1 }, { 0, 1 } );
  should_match( 3, { 0, 1, 2 }, { 0, 1, 2 } );

  // Works for similar sets to a point, fails after that
  should_match( 4, { 0, 1, 2, 3 }, { 0, 1, 2, 3 } );
  should_match( 4, { 0, 1, 2, 3 }, { 0, 1, 2, 300 } );
  should_match( 4, { 0, 1, 2, 3 }, { 0, 1, 200, 300 } );

  should_error( 4, { 0, 1, 2, 3 }, { 0, 100, 200, 300 } );
  should_error( 4, { 0, 1, 2, 3 }, { 100, 200, 300, 400 } );
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
