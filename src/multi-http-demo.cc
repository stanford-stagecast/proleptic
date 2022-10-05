#include "eventloop.hh"
#include "http_structures.hh"
#include "web_server.hh"

#include <iostream>

using namespace std;

void program_body()
{
  EventLoop events;

  WebServer web_server_ { events, 12345, []( const HTTPRequest&, HTTPResponse& response ) {
                           response.http_version = "HTTP/1.1";
                           response.status_code = "200";
                           response.reason_phrase = "OK";
                           response.body = "Hello, world.";
                           response.headers.content_length = response.body.size();
                         } };

  while ( events.wait_next_event( -1 ) != EventLoop::Result::Exit ) {}
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
