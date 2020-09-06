#pragma once
#include <initializer_list>
#include <cassert>

namespace umd::pic
{
    enum dir_t { north = 0, east = 1, south = 2, west = 3,
                 north_east, north_west, south_west, south_east };
    constexpr const int corner_nw = 0, corner_ne = 1, corner_se = 2, corner_sw = 3;

    constexpr const auto all_dirs = { north, east, south, west,
                                      north_east, north_west, south_west, south_east };

    static inline dir_t opposite( dir_t dir )
    {
        switch ( dir )
        {
            case north: return south;
            case south: return north;
            case east: return west;
            case west: return east;
            case north_east: return south_west;
            case north_west: return south_east;
            case south_east: return north_west;
            case south_west: return north_east;
            default: assert( 0 && "bad direction in opposite()" );
        }
    }

    static inline dir_t cw( dir_t dir )
    {
        return dir_t( ( dir + 1 ) % 4 );
    }

    static inline dir_t ccw( dir_t dir )
    {
        return dir_t( ( dir + 3 ) % 4 );
    }
}
