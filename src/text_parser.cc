#include <Eigen/Dense>
#include <fstream>
#include <iostream>
#include <regex>

using namespace std;
using namespace Eigen;

using T = float;

bool isNumber( const string& str )
{
  for ( char c : str ) {
    if ( isdigit( c ) == 0 )
      return false;
  }
  return true;
}

int program_body( const string& filename )
{
  ifstream file( filename );
  string line;
  int curr_layer = -1;
  bool is_weight = true;
  int cnt = 0;
  regex regex( ", " );
  while ( getline( file, line ) ) {
    // empty line -> update params
    if ( line.empty() )
      continue;
    // layer num -> get input size and output size
    else if ( isNumber( line ) ) {
      if ( curr_layer != -1 ) {
      }
      curr_layer = stoi( line );
    }
    // contains "weights" -> next line(s) will be param for weight
    else if ( strstr( line.c_str(), "weights" ) ) {
      is_weight = true;
      cnt = 0;
    }
    // contains "biases" -> next line will be param for biase
    else if ( strstr( line.c_str(), "biases" ) ) {
      is_weight = false;
    } else {
      vector<string> params( sregex_token_iterator( line.begin(), line.end(), regex, -1 ),
                             sregex_token_iterator() );
      // get rid of square brackets
      auto s = &( params[0] );
      ( *s ).erase( remove( ( *s ).begin(), ( *s ).end(), '[' ), ( *s ).end() );
      s = &( params[params.size() - 1] );
      s->erase( remove( s->begin(), s->end(), ']' ), s->end() );

      for ( int i = 0; i < (int)params.size(); i++ ) {
        float p = stof( params[i] );
        if ( is_weight ) {
          cout << "W " << cnt << " " << i << " " << p << "\n";
        } else {
          cout << "B " << i << " " << p << "\n";
        }
      }
      cnt++;
    }
  }
  // Last layer bug fix below:

  file.close();
  return 0;
}

int main( int argc, char* argv[] )
{
  if ( argc < 0 ) {
    abort();
  }

  if ( argc != 2 ) {
    cerr << "Usage: " << argv[0] << " filename\n";
    return EXIT_FAILURE;
  }

  try {
    program_body( argv[1] );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
