#include <iostream>
#include <fstream>
#include <cstdint>
#include <math.h>

using namespace std;

void generate_data() {

  std::fstream file_l;
  std::fstream file_r;
  file_l.open("audio_data_l.bin", std::ios::app | std::ios::binary);
  file_r.open("audio_data_r.bin", std::ios::app | std::ios::binary);

  

  for (int i = 0; i < 100000; i++) {
    double time = i / double( 48000 );
    float left_aud = 0.99 * sin( 2 * M_PI * 440 * time );
    float right_aud = 0.99 * sin( 2 * M_PI * 440 * time );
    file_l.write(reinterpret_cast<const char*>(&left_aud), 16384);
    file_r.write(reinterpret_cast<const char*>(&right_aud), 16384);
  }

  file_l.close();
  file_r.close();
}



int main()
{
  generate_data();
}
