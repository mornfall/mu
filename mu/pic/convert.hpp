#include "scene.hpp"
#include "reader.hpp"

namespace umd::pic::convert
{
    reader::point diff( dir_t dir )
    {
        switch ( dir )
        {
            case north: return { 0, -1 };
            case south: return { 0, +1 };
            case east: return  { +1, 0 };
            case west: return  { -1, 0 };
        }
    }

    struct object_map : std::map< reader::point, pic::object * >
    {
        // object_ptr at( int x, int y ) const { return at( point( x, y ) ); }

        pic::object *at( reader::point p ) const
        {
            if ( auto i = find( p ) ; i != end() )
                return i->second;
            else
                return nullptr;
        }
    };

    struct state
    {
        object_map objects;
        pic::group group;
        const reader::grid &grid;

        state( const reader::grid &g ) : grid( g ) {}

        void arrow( int x, int y ) { arrow( reader::point( x, y ) ); }
        void arrow( reader::point p )
        {
            auto to_dir = grid[ p ].arrow_dir();
            auto at_dir = grid[ p ].attach_dir();
            std::cerr << "requesting object at " << p + diff( to_dir ) << std::endl;
            auto to_obj = objects.at( p + diff( to_dir ) );
            assert( to_obj );
            auto to_port = to_obj->port( at_dir );

            pic::object *from_obj;

            while ( true )
            {
                auto next = p + diff( at_dir );
                from_obj = objects.at( next );

                p = next;

                if ( from_obj )
                    break;

                at_dir = grid[ next ].attach_dir( opposite( at_dir ) );
            }

            auto from_port = from_obj->port( opposite( at_dir ) );
            group.add< pic::arrow >( from_port, to_port );
        }

        reader::point line( reader::point p, dir_t dir )
        {
            for ( ; grid[ p ].attach( dir ); p = p + diff( dir ) );
            return p;
        }

        void box( reader::point p )
        {
            auto nw = p,
                 ne = line( p, east ),
                 sw = line( p, south ),
                 se1 = line( ne, south ),
                 se2 = line( sw, east );

            std::cerr << "checking box at " << p << std::endl;
            if ( se1 != se2 )
                return; /* not a box */

            int w = ne.x() - nw.x();
            int h = sw.y() - nw.y();
            std::cerr << "found box of size " << w << ", " << h << std::endl;
            auto obj = &group.add< pic::box >( 6 * p.x() + 3 * w, -10 * p.y() - 5 * h, 6 * w, 10 * h );

            std::cerr << std::endl;
            for ( int x = p.x(); x <= p.x() + w; ++x )
                for ( int y = p.y(); y <= p.y() + h; ++y )
                {
                    std::cerr << reader::point( x, y ) << " ";
                    objects[ reader::point( x, y ) ] = obj;
                }
            std::cerr << std::endl;
        }

        void object( int x, int y ) { object( reader::point( x, y ) ); }
        void object( reader::point p )
        {
            std::cerr << "checking object at " << p << std::endl;
            auto c = grid.at( p );
            if ( objects.at( p ) ) return; /* already taken up by an object */

            if ( c.attach( south ) && c.attach( east ) )
                box( p );
        }
    };

    group scene( const reader::grid &grid )
    {
        state s( grid );

        for ( auto [ x, y, c ] : grid )
        {
            if ( c.attach() )
                s.object( x, y );
        }

        for ( auto [ x, y, c ] : grid )
            if ( c.arrow() )
                s.arrow( x, y );

        return s.group;
    }
}
