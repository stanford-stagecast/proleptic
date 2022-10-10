#pragma once

#include <chrono>
#include <memory>
#include <sstream>
#include <unistd.h>

#include "eventloop.hh"
#include "file_descriptor.hh"
#include "ring_buffer.hh"
#include "summarize.hh"

class StatsPrinterTask
{
  std::shared_ptr<EventLoop> loop_;
  std::vector<std::shared_ptr<Summarizable>> objects_ {};

  FileDescriptor standard_output_;
  RingBuffer output_rb_ { 65536 };

  using time_point = decltype( std::chrono::steady_clock::now() );

  time_point bootup_time;
  time_point next_stats_print;

  static constexpr auto stats_print_interval = std::chrono::milliseconds( 2000 );

  std::ostringstream ss_ {};

public:
  StatsPrinterTask( std::shared_ptr<EventLoop> loop );

  unsigned int wait_time_ms() const;

  template<class T>
  void add( std::shared_ptr<T> obj )
  {
    objects_.push_back( std::static_pointer_cast<Summarizable>( obj ) );
  }
};
