#include "exception.hh"
#include "osc.hh"

#include <assert.h>
#include <cmath>
#include <iostream>

using namespace std;

void test1()
{
  float period = 60.0;
  float phase = 0.1;
  float error = pow( 10, -3 );
  time_t timestamp;
  time( &timestamp );

  auto osc = Oscillator( period, phase, timestamp, error );
  osc.update( 60, 2, timestamp + 1 );

  assert( osc.getPeriod() == 60 );
  assert( osc.getPhase() == 2 );
}

void test2()
{
  float period = 60.0;
  float phase = 0.1;
  float error = pow( 10, -3 );
  time_t timestamp;
  time( &timestamp );

  auto osc = Oscillator( period, phase, timestamp, error );
  osc.update( 60.001, 2, timestamp + 1 );

  assert( osc.getPeriod() == 60 );
  assert( osc.getPhase() == 2 );
}

void test3()
{
  float period = 60.0;
  float phase = 0.1;
  float error = pow( 10, -3 );
  time_t timestamp;
  time( &timestamp );

  auto osc = Oscillator( period, phase, timestamp, error );
  osc.update( 61, 2, timestamp + 1 );

  assert( osc.getPeriod() == 60 );
  assert( osc.getPhase() == 0.1 );
}

void test4()
{
  float period = 60.0;
  float phase = 0.1;
  float error = pow( 10, -4 );
  time_t timestamp;
  time( &timestamp );

  auto osc = Oscillator( period, phase, timestamp, error );
  osc.update( 30, 2, timestamp + 1 );
  assert( osc.getPeriod() == 60 );
  assert( osc.getPhase() == 1 );
}

void test5()
{
  float period = 60.0;
  float phase = 0.1;
  float error = pow( 10, -4 );
  time_t timestamp;
  time( &timestamp );

  auto osc = Oscillator( period, phase, timestamp, error );
  osc.update( 31, 2, timestamp + 1 );
  assert( osc.getPeriod() == 60 );
  assert( osc.getPhase() == 0.1 );
}

void test6()
{
  float period = 60.0;
  float phase = 0;
  float error = pow( 10, -3 );
  time_t timestamp;
  time( &timestamp );

  auto osc = Oscillator( period, phase, timestamp, error );
  osc.update( 120, 0.1, timestamp + 1 );

  assert( osc.getPeriod() == 60 );
  assert( osc.getPhase() == 0.2 );
}

void test7()
{
  float period = 60.0;
  float phase = 0;
  float error = pow( 10, -3 );
  time_t timestamp;
  time( &timestamp );

  auto osc = Oscillator( period, phase, timestamp, error );
  osc.update( 121, 0.1, timestamp + 1 );

  assert( osc.getPeriod() == 60 );
  assert( osc.getPhase() == 0 );
}

void test8()
{
  float period = 60.0;
  float phase = 2 * pi();
  float error = pow( 10, -3 );
  time_t timestamp;
  time( &timestamp );

  auto osc = Oscillator( period, phase, timestamp, error );
  osc.update( 120, pi() / 2, timestamp + 1 );

  assert( osc.getPeriod() == 60 );
  assert( osc.getPhase() == pi() );
}

void test9()
{
  float period = 60.0;
  float phase = 0.1;
  float error = pow( 10, -3 );
  time_t timestamp;
  time( &timestamp );

  auto osc = Oscillator( period, phase, timestamp, error );
  osc.update( 60.000001, 0, timestamp + 1 );

  assert( osc.getPeriod() == 60 );
  assert( osc.getPhase() == 0.1 );
}

void test10()
{
  float period = 60.0;
  float phase = 0.1;
  float error = pow( 10, -3 );
  time_t timestamp;
  time( &timestamp );

  auto osc = Oscillator( period, phase, timestamp, error );
  osc.update( 60.000001, 0, timestamp );

  assert( osc.getPeriod() == 60 );
  assert( osc.getPhase() == 0.1 );
}

void program_body()
{
  cout << "Testing begins!" << endl;
  test1();
  cout << "Test 1 passed." << endl;
  test2();
  cout << "Test 2 passed." << endl;
  test3();
  cout << "Test 3 passed." << endl;
  test4();
  cout << "Test 4 passed." << endl;
  test5();
  cout << "Test 5 passed." << endl;
  test6();
  cout << "Test 6 passed." << endl;
  test7();
  cout << "Test 7 passed." << endl;
  test8();
  cout << "Test 8 passed." << endl;
  test9();
  cout << "Test 9 passed." << endl;
  test10();
  cout << "Test 10 passed." << endl;
  cout << "All Tests Passed! :)" << endl;
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
