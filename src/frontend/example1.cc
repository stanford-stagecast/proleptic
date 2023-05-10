#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace std;

struct midi_event
{
  int timestamp;
  // int event_type;
  int note;
  int velocity;
};

void print_1d_int( vector<int> to_print )
{
  cout << "[";

  for ( int i = 0; i < to_print.size(); i++ ) {
    cout << to_print[i] << " ";
  }

  cout << "]\n";
}

void print_midi_event( vector<midi_event> to_print )
{
  cout << "[";

  for ( int i = 0; i < to_print.size(); i++ ) {
    cout << "[" << to_print[i].timestamp << " " << to_print[i].note << "] ";
  }

  cout << "]\n";
}

void print_2d_int( vector<vector<float>> to_print )
{
  cout << "[";

  for ( int i = 0; i < to_print.size(); i++ ) {
    cout << "[";
    for ( int j = 0; j < to_print[i].size(); j++ ) {
      cout << to_print[i][j] << " ";
    }
    cout << "]\n";
  }

  cout << "\n";
}

int main( int argc, char* argv[] )
{
  return EXIT_SUCCESS;
}
