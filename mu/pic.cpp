#include "common.hpp"

#include "pic/scene.hpp"
#include "pic/reader.hpp"
#include "pic/convert.hpp"
#include "doc/writer.hpp"

#include <fstream>
#include <iostream>

using namespace umd;

struct writer : pic::writer, doc::stream
{
    writer( std::ostream &o ) : doc::stream( o ) {}
    void emit_mpost( std::string_view s ) { emit( s ); }
    void emit_tex( std::u32string_view s ) { emit( s ); }
};

int main( int, const char **argv )
{
    std::cout << "color fg; fg := black;" << std::endl;
    std::cout << "beginfig(1)" << std::endl;
    std::cout << "pickup pencircle scaled .3mm;" << std::endl;

    std::ifstream f( argv[1] );
    auto buf = read_file( f );
    auto grid = pic::reader::read_grid( buf );

    auto scene = pic::convert::scene( grid );
    writer w( std::cout );
    scene.emit( w );

    std::cout << "endfig" << std::endl;
    std::cout << "end" << std::endl;
}
