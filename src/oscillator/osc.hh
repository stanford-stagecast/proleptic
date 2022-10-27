#pragma once

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>

constexpr float pi()
{
  return std::atan( 1 ) * 4;
}

class Oscillator
{
  float ori_period;
  float ori_phase;
  time_t ori_timestamp;
  float error_bound;

public:
  Oscillator( float period, float phase, time_t timestamp, float error )
    : ori_period( period )
    , ori_phase( phase )
    , ori_timestamp( timestamp )
    , error_bound( error )
  {}

  float getPeriod() { return ori_period; }
  float getPhase() { return ori_phase; }
  time_t getTimestamp() { return ori_timestamp; }
  float getError() { return error_bound; }

  void update( float period, float phase, time_t timestamp )
  {
    float new_period;
    float new_phase;
    // period not multiple
    if ( abs( period - ori_period ) <= error_bound ) {
      new_period = ori_period;
      new_phase = phase;
    }
    // multiple
    else if ( abs( period * 2 - ori_period ) <= error_bound ) {
      new_period = ori_period;
      auto numerator = phase / ( 2 * pi() ) * period;
      new_phase = numerator / ori_period * 2 * pi();
      if ( ori_phase > pi() )
        new_phase += pi();
    } else if ( abs( period - ori_period * 2 ) <= error_bound ) {
      new_period = ori_period;
      if ( phase > pi() )
        phase -= pi();
      auto numerator = phase / ( 2 * pi() ) * period;
      new_phase = numerator / ori_period * 2 * pi();
    } else {
      std::cerr << "Not Valid Input Period. Not Updating..." << std::endl;
      return;
    }

    // adjust with timestamp
    // not valid timestamp
    if ( timestamp <= ori_timestamp ) {
      std::cerr << "Not Valid Input Timestamp. Not Updating..." << std::endl;
      return;
    }
    // not valid input phase
    else if ( ( new_phase < ori_phase ) && ( period < new_period + error_bound ) ) {
      std::cerr << "Not Valid Input Phase. Not Updating..." << std::endl;
      return;
    }

    // making phase in range [0,2p]
    if ( new_phase < 0 )
      new_phase += ( 2 * pi() );
    else if ( new_phase > 2 * pi() )
      new_phase -= ( 2 * pi() );

    // update field varaibles
    ori_period = new_period;
    ori_phase = new_phase;
    ori_timestamp = timestamp;
  }
};
