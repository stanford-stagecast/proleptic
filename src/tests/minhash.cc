#include "minhash.hh"

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

void program_body()
{
  ios::sync_with_stdio( false );

  auto prng = get_random_engine();
  prng.seed( 20221201 );

  MinHash<uint32_t, 512, 10007, 8192> hasher( prng );

  // Hashing: same input => same hash
  check( hasher.hash( 1 ) != hasher.hash( 2 ) );
  check( hasher.hash( 4 ) != hasher.hash( 8 ) );
  check( hasher.hash( 12 ) == hasher.hash( 12 ) );
  check( hasher.hash( 0 ) == hasher.hash( 0 ) );

  // Hashing multiple: same input => same hashes (order-dependent)
  check( hasher.hash<2>( { 0, 1 } ) == hasher.hash<2>( { 0, 1 } ) );
  check( hasher.hash<2>( { 0, 1 } ) != hasher.hash<2>( { 1, 2 } ) );
  check( hasher.hash<2>( { 100, 200 } ) == hasher.hash<2>( { 100, 200 } ) );
  check( hasher.hash<2>( { 100, 200 } ) != hasher.hash<2>( { 200, 100 } ) );

  // MinHashing: same input => same output (order-independent)
  check( hasher.minhash<2>( { 0, 1 } ) == hasher.minhash<2>( { 0, 1 } ) );
  check( hasher.minhash<2>( { 1, 0 } ) == hasher.minhash<2>( { 0, 1 } ) );
  check( hasher.minhash<3>( { 1, 2, 3 } ) == hasher.minhash<3>( { 1, 2, 3 } ) );
  check( hasher.minhash<3>( { 1, 2, 3 } ) == hasher.minhash<3>( { 3, 2, 1 } ) );
  check( hasher.minhash<3>( { 1, 2, 3 } ) != hasher.minhash<3>( { 4, 5, 6 } ) );
  check( hasher.minhash<3>( { 1, 2, 3 } ) != hasher.minhash<3>( { 4, 5, 6 } ) );

  // Does not work for similar but unequal sets (need LSH for that)
  check( hasher.minhash<4>( { 1, 2, 3, 4 } ) == hasher.minhash<4>( { 1, 2, 3, 4 } ) );
  check( hasher.minhash<4>( { 1, 2, 3, 4 } ) != hasher.minhash<4>( { 1, 2, 3, 400 } ) );
  check( hasher.minhash<4>( { 1, 2, 3, 4 } ) != hasher.minhash<4>( { 1, 2, 300, 400 } ) );
  check( hasher.minhash<4>( { 1, 2, 3, 4 } ) != hasher.minhash<4>( { 1, 200, 300, 400 } ) );
  check( hasher.minhash<4>( { 1, 2, 3, 4 } ) != hasher.minhash<4>( { 100, 200, 300, 400 } ) );
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
