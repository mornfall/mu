#pragma once
#include <array>

namespace umd::pic
{
    template< typename T, size_t n >
    std::array< int, n > index_sort( const std::array< T, n > &t )
    {
        std::array< int, n > idx;
        for ( int i = 0; i < n; ++i )
            idx[ i ] = i;

        std::sort( idx.begin(), idx.end(), [&]( auto a, auto b ) { return t[ a ] > t[ b ]; } );
        return idx;
    }
}
