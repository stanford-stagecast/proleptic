#pragma once

#include <Eigen/Dense>
#include <cassert>
#include <cstdio>

template<typename T, int rows, int cols>
void pp_internal( const Eigen::Matrix<T, rows, cols>& m )
{
  printf( "  [" );

  for ( size_t row = 0; row < rows; row++ ) {
    for ( size_t col = 0; col < cols; col++ ) {
      printf( "\t%.2f", m( row, col ) );
    }

    if ( row == rows - 1 ) {
      printf( "\t]" );
    };

    printf( "\n" );
  }
}

#define pp( X )                                                                                                    \
  do {                                                                                                             \
    printf( "%s =\n", #X );                                                                                        \
    pp_internal( X );                                                                                              \
    printf( "\n" );                                                                                                \
  } while ( 0 );
