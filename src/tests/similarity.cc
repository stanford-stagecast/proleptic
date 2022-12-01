#include "similarity.hh"
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

void test_jaccard_index()
{
  // This is the baseline, so if this doesn't work something is very wrong...
  check( jaccard_index<int>( { 0 }, { 0 } ) == 1 );
  check( jaccard_index<int>( { 0, 1 }, { 0, 1 } ) == 1 );
  check( jaccard_index<int>( { 0 }, { 0, 1 } ) == 0.5 );
  check( jaccard_index<int>( { 0, 1 }, { 0 } ) == 0.5 );
  check( jaccard_index<int>( { 0, 1, 2, 3, 4, 5, 6, 7 }, { 0 } ) == 0.125 );
  check( jaccard_index<int>( { 0 }, { 0, 1, 2, 3, 4, 5, 6, 7 } ) == 0.125 );
  check( jaccard_index<int>( { 512 }, { 0, 1, 2, 3, 4, 5, 6, 7 } ) == 0 );
  check( jaccard_index<int>( { 512, 256, 128 }, { 64, 128, 256 } ) == 0.5 );
  check( jaccard_index<int>( { 512, 256, 128 }, { 128, 256 } ) == 2.f / 3.f );
}

void program_body()
{
  ios::sync_with_stdio( false );
  test_jaccard_index();
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
