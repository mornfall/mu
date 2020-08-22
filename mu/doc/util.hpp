#pragma once
#include <string_view>
#include <locale>
#include <codecvt>
#include <fstream>

static inline std::u32string read_file( std::ifstream &in )
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

static inline std::u32string read_file( std::string path )
{
    std::ifstream ifs( path );
    return read_file( ifs );
}

static inline std::string to_utf8( std::u32string_view w )
{
    std::wstring_convert< std::codecvt_utf8< char32_t >, char32_t > conv;
    return conv.to_bytes( w.begin(), w.end() );
}

template< typename SV >
bool starts_with( SV s, decltype( s ) t )
{
    return s.size() >= t.size() && s.compare( 0, t.size(), t ) == 0;
}
