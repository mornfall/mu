#include "pic/scene.hpp"
#include "pic/reader.hpp"
#include "pic/convert.hpp"

#include <fstream>
#include <codecvt>

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

pic::group test_scene()
{
    pic::point origin( 0, 0 ), p1( 50, 20 ), p2( 40, -30 );
    pic::group g;

    auto &n1 = g.add< pic::node >( origin );
    auto &n2 = g.add< pic::node >( p1 );
    auto &box = g.add< pic::box >( p2, 30, 15 );

    g.add< pic::arrow >( n1.out( pic::east ), n2.in( pic::west ) );
    g.add< pic::arrow >( box.out( pic::north ), n2.in( pic::south ) );

    return g;
}

int main( int argc, const char **argv )
{
    std::cout << "color fg; fg := black;" << std::endl;
    std::cout << "beginfig(0)" << std::endl;
    std::cout << "pickup pencircle scaled .3mm;" << std::endl;

    if ( argc == 2 )
    {
        std::ifstream f( argv[1] );
        auto buf = read_file( f );
        auto grid = pic::reader::read_grid( buf );

        auto scene = pic::convert::scene( grid );
        scene.emit( std::cout );
    }
    else
        test_scene().emit( std::cout );

    std::cout << "endfig" << std::endl;
    std::cout << "end" << std::endl;
}
