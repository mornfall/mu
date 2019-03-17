#include "doc/convert.hpp"
#include "doc/w_slides.hpp"
#include <fstream>
#include <iostream>

using namespace umd;

std::u32string read_file( std::ifstream &in )
{
    in.seekg( 0, std::ios::end );
    size_t length = in.tellg();
    in.seekg( 0, std::ios::beg );

    std::string buffer;
    buffer.resize( length );
    in.read( &buffer[ 0 ], length );

    std::wstring_convert< std::codecvt_utf8< char32_t >, char32_t > conv;
    return conv.from_bytes( buffer );
}

int main( int argc, const char **argv )
{
    std::cout << "\\input{prelude.tex}\\startmakeup[slide]" << std::endl;

    if ( argc != 2 )
        return std::cerr << "need one (file) argument" << std::endl, 1;

    std::ifstream f( argv[1] );
    auto buf = read_file( f );
    doc::stream out( std::cout );
    doc::w_slides swr( out );
    doc::convert s( buf, swr );
    s.run();

    s.end_list( -1 );
    std::cout << "\\stopmakeup\\stoptext" << std::endl;
}
