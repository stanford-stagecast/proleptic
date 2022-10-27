/*#include "osc.hh"
#include <iostream>

using namespace std;

Oscillator::Oscillator(float period, float phase, time_t timestamp, float error)
  : ori_period( period )
  , ori_phase( phase )
  , ori_timestamp( timestamp )
  , error_bound( error )
{
  cout << "here" << endl;
}

float Oscillator::getPeriod() {return ori_period;}
float Oscillator::getPhase() {return ori_phase;}
time_t Oscillator::getTimestamp() {return ori_timestamp;}
float Oscillator::getError() {return error_bound;}

void Oscillator::update(float period, float phase, time_t timestamp)
{
  // period remains unchanged
  if (abs(period - ori_period) <= error_bound) {
    // update phase
    ori_phase = phase * (static_cast<long>(timestamp) - static_cast<long>(ori_timestamp));
  }
  if (abs(period * 2 - ori_period) <= error_bound) {
    // update phase
  }
  if (abs(period - ori_period * 2) <= error_bound) {
    // update phase
  }
}*/
