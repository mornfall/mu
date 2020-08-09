#include "common.hpp"
#include "doc/convert.hpp"
#include "doc/w_slides.hpp"
#include "doc/w_lnotes.hpp"
#include "doc/w_paper.hpp"
#include "doc/w_html.hpp"
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

int doctype( std::u32string_view buf, std::u32string dt )
{
    w_doctype wdt;

    if ( dt.empty() )
    {
        doc::convert conv( buf, wdt );
        conv.header();
        dt = wdt.type;
    }

    if      ( dt == U"slides" ) convert< doc::w_slides >( buf );
    else if ( dt == U"lnotes" ) convert< doc::w_lnotes >( buf );
    else if ( dt == U"html" )   convert< doc::w_html >( buf );
    else if ( dt == U"paper" )  convert< doc::w_paper >( buf );
    else
    {
        std::cerr << "unknown document type " << to_utf8( dt ) << std::endl;
        return 1;
    }

    return 0;
}

int main( int argc, const char **argv )
{
    if ( argc < 2 )
        return std::cerr << "need at least one (file) argument" << std::endl, 1;

    std::u32string dt;
    const char *fn = argv[ 1 ];

    if ( argv[ 1 ] == std::string( "--html" ) )
        dt = U"html", fn = argv[ 2 ];

    std::ifstream f( fn );
    auto buf = read_file( f );
    return doctype( buf, dt );
}
