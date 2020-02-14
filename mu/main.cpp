#include "common.hpp"
#include "doc/convert.hpp"
#include "doc/w_slides.hpp"
#include "doc/w_lnotes.hpp"
#include "doc/w_paper.hpp"
#include <fstream>
#include <iostream>

using namespace umd;

std::string to_utf8( std::u32string w )
{
    std::wstring_convert< std::codecvt_utf8< char32_t >, char32_t > conv;
    return conv.to_bytes( w );
}

struct w_doctype : doc::w_noop
{
    std::u32string type;

    virtual void meta( sv k, sv v )
    {
        if ( k == U"doctype" )
            type = v;
    }
};

template< typename writer >
void convert( std::u32string_view buf )
{
    doc::stream out( std::cout );
    writer w( out );
    doc::convert conv( buf, w );
    conv.run();
}

int doctype( std::u32string_view buf )
{
    w_doctype dt;
    doc::convert conv( buf, dt );
    conv.header();
    if      ( dt.type == U"slides" ) convert< doc::w_slides >( buf );
    else if ( dt.type == U"lnotes" ) convert< doc::w_lnotes >( buf );
    else if ( dt.type == U"paper" )  convert< doc::w_paper >( buf );
    else
    {
        std::cerr << "unknown document type " << to_utf8( dt.type ) << std::endl;
        return 1;
    }

    return 0;
}

int main( int argc, const char **argv )
{
    if ( argc != 2 )
        return std::cerr << "need one (file) argument" << std::endl, 1;

    std::ifstream f( argv[1] );
    auto buf = read_file( f );
    return doctype( buf );
}
