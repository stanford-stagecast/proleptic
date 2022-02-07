#include "file_watch_loop.hh"

#include <string>

#include <chrono>
#include <thread>

FileWatchLoop::FileWatchLoop( const string & filename )
: ifs( filename )
{
    
}


string FileWatchLoop::readfile( )
{

    if (ifs.is_open())
    {
        std::string line;
        std::string output_line;

        while (std::getline(ifs, line)) output_line = line;
        if (ifs.eof())
            ifs.clear();
        return output_line;
    }
    return "";
}

