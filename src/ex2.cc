#include "eventloop.hh"
#include "exception.hh"
#include "http_server.hh"
#include "socket.hh"

#include <iostream>

using namespace std;

static constexpr size_t mebi = 1024 * 1024;

int main()
{
  FileDescriptor output { CheckSystemCall( "dup", dup( STDOUT_FILENO ) ) };
  output.set_blocking( false );

  TCPSocket server_socket {};
  server_socket.set_reuseaddr();

  try {
    server_socket.bind( { "0", 9090 } );
  } catch ( ... ) {
    server_socket.bind( { "0", 0 } );
  }

  server_socket.listen();
  cerr << "Listening on port " << server_socket.local_address().port() << "\n";

  EventLoop events;

  auto connection = server_socket.accept();
  cerr << "Connection received from " << connection.peer_address().to_string() << "\n";

  RingBuffer client_buffer { mebi }, server_buffer { mebi };

  HTTPServer http_server;
  HTTPRequest request_in_progress;

  events.add_rule(
    "Read bytes from client",
    connection,
    Direction::In,
    [&] { client_buffer.push_from_fd( connection ); },
    [&] { return not client_buffer.writable_region().empty(); } );

  const string response_str = "<span style='color:blue;'>Hello, everybody. Time for lunch!</span>";

  events.add_rule(
    "Parse bytes from client buffer",
    [&] {
      if ( http_server.read( client_buffer, request_in_progress ) ) {
        cerr << "Got request: " << request_in_progress.request_target << "\n";
        HTTPResponse the_response;
        the_response.http_version = "HTTP/1.1";
        the_response.status_code = "200";
        the_response.reason_phrase = "OK";
        the_response.headers.content_type = "text/html";
        the_response.headers.content_length = response_str.size();
        the_response.body = response_str;
        http_server.push_response( move( the_response ) );
      }
    },
    [&] { return not client_buffer.readable_region().empty(); } );

  events.add_rule(
    "Serialize bytes from server into server buffer",
    [&] { http_server.write( server_buffer ); },
    [&] { return not http_server.responses_empty() and not server_buffer.writable_region().empty(); } );

  events.add_rule(
    "Write bytes to client",
    connection,
    Direction::Out,
    [&] { server_buffer.pop_to_fd( connection ); },
    [&] { return not server_buffer.readable_region().empty(); } );

  while ( events.wait_next_event( -1 ) != EventLoop::Result::Exit ) {}

  cerr << "Exiting...\n";

  return 0;
}
