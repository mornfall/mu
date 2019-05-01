#include "common.hpp"
#include "doc/convert.hpp"
#include "doc/w_paper.hpp"
#include <fstream>
#include <iostream>

using namespace umd;

int main( int argc, const char **argv )
{
    if ( argc != 2 )
        return std::cerr << "need one (file) argument" << std::endl, 1;

    std::ifstream f( argv[1] );
    auto buf = read_file( f );
    doc::stream out( std::cout );
    doc::w_paper swr( out );
    doc::convert s( buf, swr );
    s.run();

    s.end_list( -1 );
    std::cout << "\\end{document}" << std::endl;
}
