#pragma once
#include <string_view>
#include <locale>
#include <codecvt>
#include <fstream>

static inline std::string to_utf8( std::u32string_view w )
{
    std::wstring_convert< std::codecvt_utf8< char32_t >, char32_t > conv;
    return conv.to_bytes( w.begin(), w.end() );
}

static inline std::u32string from_utf8( std::string_view v )
{
    std::wstring_convert< std::codecvt_utf8< char32_t >, char32_t > conv;
    return conv.from_bytes( v.begin(), v.end() );
}

static inline std::u32string read_file( std::istream &in )
{
    std::string buffer;

    if ( in.tellg() == -1 )
        std::getline( in, buffer, '\0' ); /* FIXME binary data */
    else
    {
        in.seekg( 0, std::ios::end );
        size_t length = in.tellg();
        in.seekg( 0, std::ios::beg );
        buffer.resize( length );
        in.read( &buffer[ 0 ], length );
    }

    return from_utf8( buffer );
}

static inline std::u32string read_file( std::string path )
{
    std::ifstream ifs( path );
    return read_file( ifs );
}

template< typename SV >
bool starts_with( SV s, decltype( s ) t )
{
    return s.size() >= t.size() && s.compare( 0, t.size(), t ) == 0;
}
