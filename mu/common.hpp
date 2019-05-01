#pragma once
#include <string_view>
#include <locale>
#include <codecvt>
#include <fstream>

static std::u32string read_file( std::ifstream &in )
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
