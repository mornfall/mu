#pragma once

namespace umd::pic
{
    enum dir_t { north = 0, east = 1, south = 2, west = 3,
                north_east, north_west, south_west, south_east };
    constexpr const int corner_nw = 0, corner_ne = 1, corner_se = 2, corner_sw = 3;

    static inline dir_t opposite( dir_t dir )
    {
        return dir_t( ( dir + 2 ) % 4 );
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
