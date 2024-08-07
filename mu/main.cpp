#include "doc/util.hpp"
#include "doc/convert.hpp"
#include "doc/w_slides.hpp"
#include "doc/w_lnotes.hpp"
#include "doc/w_paper.hpp"
#include "doc/w_html.hpp"
#include <fstream>
#include <iostream>

using namespace umd;

struct w_doctype : doc::w_noop
{
    std::u32string type;

    virtual void meta( sv k, sv v )
    {
        if ( k == U"doctype" )
            type = v;
    }
};

template< typename writer, typename... args_t >
void convert( std::string outfn, std::u32string_view buf, args_t... args )
{
    std::unique_ptr< std::ofstream > file( outfn == "-" ? nullptr : new std::ofstream( outfn.c_str() ) );
    doc::stream out( file ? *file : std::cout );
    writer w( out, args... );
    doc::convert conv( buf, w );
    conv.run();
}

int doctype( std::u32string_view buf, std::u32string dt, std::string embed, std::string out )
{
    w_doctype wdt;

    if ( dt.empty() )
    {
        doc::convert conv( buf, wdt );
        conv.header();
        dt = wdt.type;
    }

    if      ( dt == U"slides" )   convert< doc::w_slides >( out, buf );
    else if ( dt == U"lnotes" )   convert< doc::w_lnotes >( out, buf );
    else if ( dt == U"workbook" ) convert< doc::w_context >( out, buf );
    else if ( dt == U"plain" )    convert< doc::w_context >( out, buf );
    else if ( dt == U"html" )     convert< doc::w_html >( out, buf, embed );
    else if ( dt == U"paper" )    convert< doc::w_paper >( out, buf );
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
    std::string embed, out = "-";

    for ( int i = 1; i < argc; ++i )
    {
        if ( argv[ i ] == std::string( "--html" ) )
            dt = U"html", fn = argv[ i + 1 ];
        if ( argv[ i ] == std::string( "--embed" ) )
            embed = argv[ i + 1 ], fn = argv[ i + 2 ];
        if ( argv[ i ] == std::string( "-o" ) )
            out = argv[ i + 1 ], fn = argv[ i + 2 ];
    }

    std::u32string buf;

    if ( fn )
        buf = read_file( fn );
    else
        buf = read_file( std::cin );

    return doctype( buf, dt, embed, out );
}
