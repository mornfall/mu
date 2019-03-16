#include "scene.hpp"
#include "reader.hpp"
#include "convert.hpp"

#include <fstream>
#include <codecvt>

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

ad::group test_scene()
{
    ad::point origin( 0, 0 ), p1( 50, 20 ), p2( 40, -30 );
    ad::group g;

    auto &n1 = g.add< ad::node >( origin );
    auto &n2 = g.add< ad::node >( p1 );
    auto &box = g.add< ad::box >( p2, 30, 15 );

    g.add< ad::arrow >( n1.out( ad::east ), n2.in( ad::west ) );
    g.add< ad::arrow >( box.out( ad::north ), n2.in( ad::south ) );

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
        auto grid = ad::reader::read_grid( buf );

        auto scene = ad::convert::scene( grid );
        scene.emit( std::cout );
    }
    else
        test_scene().emit( std::cout );

    std::cout << "endfig" << std::endl;
    std::cout << "end" << std::endl;
}
