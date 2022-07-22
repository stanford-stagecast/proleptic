#include <charconv>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "network.hh"
#include "pp.hh"

using namespace std;

using DNN = Network<float, 1, 16, 16, 1, 30, 2560, 1>;

void split_on_char( const string_view str, const char ch_to_find, vector<string_view>& ret )
{
  ret.clear();

  bool in_double_quoted_string = false;
  unsigned int field_start = 0;
  for ( unsigned int i = 0; i < str.size(); i++ ) {
    const char ch = str[i];
    if ( ch == '"' ) {
      in_double_quoted_string = !in_double_quoted_string;
    } else if ( in_double_quoted_string ) {
      continue;
    } else if ( ch == ch_to_find ) {
      ret.emplace_back( str.substr( field_start, i - field_start ) );
      field_start = i + 1;
    }
  }

  ret.emplace_back( str.substr( field_start ) );
}

float to_float( string_view str )
{
  float ret = -1;
  const auto [ptr, ignore] = from_chars( str.data(), str.data() + str.size(), ret );
  if ( ptr != str.data() + str.size() ) {
    str.remove_prefix( ptr - str.data() );
    throw runtime_error( "could not parse as integer: " + string( str ) );
  }

  return ret;
}

template<size_t N>
void load_layer( DNN& mynetwork, ifstream& file )
{
  auto& the_layer = mynetwork.get_layer<N>();
  string line;
  vector<string_view> tokens;

  // load weights
  for ( int row = 0; row < the_layer.weights().rows(); row++ ) {
    for ( int col = 0; col < the_layer.weights().cols(); col++ ) {
      getline( file, line );
      if ( not file.good() ) {
        throw runtime_error( "file not good" );
      }

      split_on_char( line, ' ', tokens );
      if ( tokens.at( 0 ) != "W" ) {
        throw runtime_error( "invalid: missing W" );
      }

      if ( tokens.at( 1 ) != to_string( row ) ) {
        throw runtime_error( "invalid: row mismatch " + string( tokens.at( 1 ) ) + " vs. " + to_string( row ) );
      }

      if ( tokens.at( 2 ) != to_string( col ) ) {
        throw runtime_error( "invalid: col mismatch " + string( tokens.at( 2 ) ) + " vs. " + to_string( col ) );
      }

      the_layer.weights()( row, col ) = to_float( tokens.at( 3 ) );
    }
  }

  // load biases
  for ( int col = 0; col < the_layer.biases().cols(); col++ ) {
    getline( file, line );
    if ( not file.good() ) {
      throw runtime_error( "file not good" );
    }

    split_on_char( line, ' ', tokens );
    if ( tokens.at( 0 ) != "B" ) {
      throw runtime_error( "invalid: missing B" );
    }

    if ( tokens.at( 1 ) != to_string( col ) ) {
      throw runtime_error( "invalid: col mismatch " + string( tokens.at( 1 ) ) + " vs. " + to_string( col ) );
    }

    the_layer.biases()( 0, col ) = to_float( tokens.at( 2 ) );
  }
}

void load_weights_and_biases( DNN& mynetwork, const string& filename )
{
  ifstream file { filename };
  if ( not file.good() ) {
    throw runtime_error( "can't open filename" );
  }

  load_layer<0>( mynetwork, file );
  load_layer<1>( mynetwork, file );
  load_layer<2>( mynetwork, file );
  load_layer<3>( mynetwork, file );
  load_layer<4>( mynetwork, file );
}

void program_body( const string& filename )
{
  auto mynetwork_ptr = make_unique<DNN>();
  DNN& mynetwork = *mynetwork_ptr;

  DNN::M_input my_input;

  my_input << 0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 5.5, 6, 6.5, 7, 7.5;

  cout << "input: " << my_input << "\n";

  DNN::M_output my_output;

  load_weights_and_biases( mynetwork, filename );

  cout << "Number of layers: " << mynetwork.num_layers << "\n";

  mynetwork.apply( my_input, my_output );

  cout << my_output( 0, 0 ) << "\n";
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
