#pragma once

namespace ad
{
    enum dir_t { north = 0, east = 1, south = 2, west = 3 };

    dir_t opposite( dir_t dir )
    {
        return dir_t( ( dir + 2 ) % 4 );
    }
}
