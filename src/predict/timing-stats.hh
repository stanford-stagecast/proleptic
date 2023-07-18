#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>

#include "timer.hh"

struct TimingStats
{
  uint64_t max_ns {};   // max time taken to process an event
  uint64_t total_ns {}; // total processing time (to calc avg time)
  uint64_t count {};    // total events processed

  std::optional<uint64_t> start_time {};

  void start_timer()
  {
    if ( start_time.has_value() ) {
      throw std::runtime_error( "timer started when already running" );
    }
    start_time = Timer::timestamp_ns(); // start the timer
  }

  void stop_timer()
  {
    if ( not start_time.has_value() ) {
      throw std::runtime_error( "timer stopped when not running" );
    }
    const uint64_t time_elapsed = Timer::timestamp_ns() - start_time.value();
    max_ns = std::max( time_elapsed, max_ns );
    total_ns += time_elapsed;
    count++;
    start_time.reset();
  }
};
