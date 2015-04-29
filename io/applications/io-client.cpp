// This file is part of comma, a generic and flexible library
// Copyright (c) 2011 The University of Sydney
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the University of Sydney nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
// GRANTED BY THIS LICENSE.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
// HOLDERS AND CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef WIN32
#include <stdlib.h>
#include <sys/ioctl.h>
#endif

#include <boost/asio/ip/udp.hpp>
#include <comma/application/command_line_options.h>
#include <comma/application/signal_flag.h>
#include <comma/io/stream.h>
#include <comma/io/select.h>

static const char* name() { return "io-client"; }

void usage( bool verbose = false )
{
    std::cerr << std::endl;
    std::cerr << "read from a socket and output to stdout" << std::endl;
    std::cerr << std::endl;
    std::cerr << "usage: " << name() << " <socket address>" << std::endl;
    std::cerr << std::endl;
    std::cerr << "supported sockets: tcp, udp, local (unix) socket, zmq local socket, zmq tcp" << std::endl;
    std::cerr << std::endl;
    std::cerr << "examples: " << std::endl;
    std::cerr << "    read from tcp port 12345 on localhost and output to stdout: " << std::endl;
    std::cerr << "    " << name() << " tcp:localhost:12345" << std::endl;
    std::cerr << std::endl;
    exit( 1 );
}

static const int buffer_size = 16384;

int main( int argc, char** argv )
{
    if( argc < 2 ) { usage(); }
    comma::command_line_options options( argc, argv, usage );
    const std::vector< std::string >& unnamed = options.unnamed( "", "" );
    if( unnamed.size() != 1 ) { std::cerr << name() << " : address is not given" << std::endl; return 1; }
    std::vector< std::string > input = comma::split( unnamed[0], ':' );
    comma::signal_flag is_shutdown;
    if( input[0] == "udp" )
    {
        if( input.size() != 2 ) { std::cerr << name() << " : expected udp:<port>, e.g. udp:12345, got" << unnamed[0] << std::endl; return 1; }
        unsigned short port = boost::lexical_cast< unsigned short >( input[1] );
        std::vector< char > packet( buffer_size );
        boost::asio::io_service service;
        boost::asio::ip::udp::socket socket( service );
        socket.open( boost::asio::ip::udp::v4() );
        boost::system::error_code error;
        socket.set_option( boost::asio::ip::udp::socket::broadcast( true ), error );
        if( error ) { std::cerr << name() << " : udp failed to set broadcast option on port " << port << std::endl; return 1; }
        socket.bind( boost::asio::ip::udp::endpoint( boost::asio::ip::udp::v4(), port ), error );
        if( error ) { std::cerr << name() << " : udp failed to bind port " << port << std::endl; return 1; }
#ifdef WIN32
        _setmode( _fileno( stdout ), _O_BINARY );
#endif
        while( !is_shutdown && std::cout.good() )
        {
            boost::system::error_code error;
            std::size_t size = socket.receive( boost::asio::buffer( packet ), 0, error );
            if( error || size == 0 ) { break; }
            std::cout.write( &packet[0], size );
            std::cout.flush();
        }
        return 0;
    }
    else
    {
#ifdef WIN32
        std::cerr << name() " : not implemented" << std::endl; return 1;
#else
        comma::io::istream istream( unnamed[0], comma::io::mode::binary, comma::io::mode::non_blocking );
        int fd = istream.fd();
        comma::io::select select;
        select.read().add( fd );
        std::vector< char > buffer( buffer_size );
        while( !is_shutdown && istream->good() && std::cout.good() )
        {
            select.check();
            if( select.read().ready( fd ) )
            {
                int count = istream.count();
                int size = std::min( count, buffer_size );
                istream->read( &buffer[0], size );
                std::cout.write( &buffer[0], size );
                std::cout.flush();
            }
        }
        return 0;
#endif
    }
}