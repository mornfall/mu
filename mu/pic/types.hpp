#pragma once

namespace umd::pic
{
    enum dir_t { north = 0, east = 1, south = 2, west = 3 };

    dir_t opposite( dir_t dir )
    {
        return dir_t( ( dir + 2 ) % 4 );
    }

    dir_t cw( dir_t dir )
    {
        return dir_t( ( dir + 1 ) % 4 );
    }

    dir_t ccw( dir_t dir )
    {
        return dir_t( ( dir + 3 ) % 4 );
    }
}
