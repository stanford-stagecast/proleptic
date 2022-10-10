#include "stats_printer.hh"
#include "exception.hh"

using namespace std;
using namespace std::chrono;

StatsPrinterTask::StatsPrinterTask( std::shared_ptr<EventLoop> loop )
  : loop_( loop )
  , standard_output_( CheckSystemCall( "dup STDERR_FILENO", dup( STDERR_FILENO ) ) )
  , bootup_time( steady_clock::now() )
  , next_stats_print( bootup_time + stats_print_interval )
{
  loop_->add_rule(
    "generate+print statistics",
    [&] {
      ss_.str( {} );
      ss_.clear();

      const auto now = steady_clock::now();

      ss_ << "=============== Runtime Statistics at t = ";
      ss_ << duration_cast<milliseconds>( now - bootup_time ).count();
      ss_ << " ms ===============\n\n";

      for ( const auto& obj : objects_ ) {
        if ( obj ) {
          obj->summary( ss_ );
          obj->reset_summary();
          ss_ << "\n";
        }
      }

      loop_->summary( ss_ );
      ss_ << "\n";
      global_timer().summary( ss_ );
      ss_ << "\n";

      /* calculate new time */
      next_stats_print = now + stats_print_interval;

      /* reset for next time */
      loop_->reset_summary();

      /* print out */
      const auto& str = ss_.str();
      if ( output_rb_.writable_region().size() >= str.size() ) {
        output_rb_.push_from_const_str( str );
      }
      output_rb_.pop_to_fd( standard_output_ );
    },
    [&] { return steady_clock::now() > next_stats_print; } );

  loop_->add_rule(
    "print statistics",
    standard_output_,
    Direction::Out,
    [&] { output_rb_.pop_to_fd( standard_output_ ); },
    [&] { return output_rb_.bytes_stored() > 0; } );
}

unsigned int StatsPrinterTask::wait_time_ms() const
{
  const auto now = steady_clock::now();
  if ( now > next_stats_print ) {
    return 0;
  } else {
    return duration_cast<milliseconds>( next_stats_print - now ).count();
  }
}
