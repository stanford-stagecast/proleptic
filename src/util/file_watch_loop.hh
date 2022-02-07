#pragma once

#include <iostream>
#include <fstream>

using namespace std;

/* wrap WAV file with error/validity checks */
class FileWatchLoop
{
    std::ifstream ifs;

public:
    FileWatchLoop( const string & filename );

    std::string readfile( );

};