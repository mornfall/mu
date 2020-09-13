// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

#include "scene.hpp"
#include "reader.hpp"
#include "util.hpp"

#include <array>
#include <set>

namespace umd::pic::convert
{
    static inline reader::point diff( dir_t dir )
    {
        switch ( dir )
        {
            case north: return { 0, -1 };
            case south: return { 0, +1 };
            case east:  return { +1, 0 };
            case west:  return { -1, 0 };

            case north_east: return { +1, -1 };
            case north_west: return { -1, -1 };
            case south_east: return { +1, +1 };
            case south_west: return { -1, +1 };
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
        std::set< reader::point > processed;

        static constexpr double xpitch = 4.5, ypitch = 9;

        state( const reader::grid &g ) : grid( g ) {}

        void arrow( int x, int y ) { arrow( reader::point( x, y ) ); }
        void arrow( reader::point p )
        {
            arrow( p, grid[ p ].arrow_dir() );
        }

        void arrow( reader::point p, dir_t to_dir )
        {
            auto conv = []( reader::point p )
            {
                return pic::point( xpitch * p.x(), -ypitch * p.y() );
            };

            auto head   = grid[ p ].head();
            auto at_dir = grid[ p ].attach_dir( to_dir );
            auto to     = p + diff( to_dir );
            auto to_obj = objects.at( to );
            auto to_port = to_obj ? to_obj->port( opposite( to_dir ) ) : port( conv( p ), to_dir );
            std::vector< pic::point > points;
            bool dashed = false, curved = false;

            pic::object *from_obj = nullptr;

            while ( true )
            {
                auto next = p + diff( at_dir );
                from_obj = objects.at( next );

                if ( grid[ p ].dashed() )
                    dashed = true;
                if ( grid[ p ].rounded() )
                    curved = true;

                processed.insert( p );
                p = next;
                auto cell = grid[ next ];
                auto ndir = at_dir;

                if ( from_obj || cell.arrow() )
                    break;

                if ( !cell.attach_all() && !cell.arrow() )
                    ndir = cell.attach_dir( opposite( at_dir ) );
                if ( at_dir != ndir )
                    points.emplace_back( xpitch * next.x(), -ypitch * next.y() );
                at_dir = ndir;
            }

            auto from_port = from_obj ? from_obj->port( opposite( at_dir ) )
                                      : port( conv( p ), at_dir );
            auto &arrow = group.add< pic::arrow >( from_port, to_port );
            arrow._dashed = dashed;
            arrow._curved = curved;
            arrow._head   = head;
            std::copy( points.rbegin(), points.rend(), std::back_inserter( arrow._controls ) );
        }

        std::pair< reader::point, int > boundary( reader::point p, dir_t dir, int joins = 1,
                                                  bool jcw = true, bool *dashed = nullptr )
        {
            int joined = 0;
            bool first = true;
            for ( ; grid[ p ].attach( dir ); p = p + diff( dir ) )
            {
                if ( dashed && grid[ p ].dashed() )
                    *dashed = true;
                if ( first )
                {
                    first = false;
                    continue;
                }
                if ( ( !jcw && grid[ p ].attach( cw( dir ) ) ) ||
                     ( jcw && grid[ p ].attach( ccw( dir ) ) ) )
                    if ( ++joined == joins )
                        break;
            }
            return { p, joined };
        }

        using joins = std::array< int, 4 >;

        void box( reader::point p, joins mj = { 1, 1, 1, 1 } )
        {
            std::array< reader::point, 4 > c;
            joins j;
            bool dashed;

            auto nw = p;
            auto [ ne, jn ] = boundary( p, east,   mj[ 0 ], false, &dashed );
            auto [ sw, je ] = boundary( p, south,  mj[ 1 ], true,  &dashed );
            auto [ se, jw ] = boundary( ne, south, mj[ 2 ], false, &dashed );
            auto [ sx, js ] = boundary( sw, east,  mj[ 3 ], true,  &dashed );

            c[ corner_ne ] = ne; j[ 0 ] = jn;
            c[ corner_nw ] = nw; j[ 1 ] = je;
            c[ corner_se ] = se; j[ 2 ] = jw;
            c[ corner_sw ] = sw; j[ 3 ] = js;

            if ( se != sx )
            {
                auto idx = index_sort( mj );
                for ( auto i : idx )
                    if ( mj[ i ] == j[ i ] )
                    {
                        mj[ i ] ++;
                        return box( p, mj );
                        mj[ i ] --;
                    }

                return; /* not a box */
            }

            double w = ne.x() - nw.x();
            double h = sw.y() - nw.y();
            auto obj = &group.add< pic::box >( xpitch * (   p.x() + w / 2 ),
                                               ypitch * ( - p.y() - h / 2 ),
                                               xpitch * w, ypitch * h );
            for ( int i = 0; i < int( c.size() ); ++i )
                obj->set_rounded( i, grid[ c[ i ] ].rounded() );
            obj->set_dashed( dashed );

            std::u32string txt;
            int last_x = p.x(), last_y = 0;

            for ( int y = p.y(); y <= p.y() + h; ++y )
                for ( int x = p.x(); x <= p.x() + w; ++x )
                {
                    reader::point p( x, y );

                    if ( grid[ p ].shade() )
                        obj->set_shaded( grid[ p ].shade() );
                    else if ( grid[ p ].text() )
                    {
                        if ( y != last_y && !txt.empty() )
                            txt += U"\\break ";
                        if ( x != last_x + 1 && !txt.empty() )
                            txt += ' ';
                        txt += grid[ p ].character();
                        last_x = x, last_y = y;
                    }

                    objects[ reader::point( x, y ) ] = obj;
                    processed.emplace( x, y );
                }

            if ( !txt.empty() )
                group.add< pic::label >( xpitch * (   p.x() + w / 2 ),
                                         ypitch * ( - p.y() - 1.08 * h / 2 ), txt );

            for ( auto p = ne; p != se; p = p + reader::point( 0, 1 ) )
                if ( grid[ p ].attach( east ) )
                    box( p );

            for ( auto p = sw; p != se; p = p + reader::point( 1, 0 ) )
                if ( grid[ p ].attach( south ) )
                    box( p );
        }

        void object( int x, int y ) { object( reader::point( x, y ) ); }
        void object( reader::point p )
        {
            auto c = grid.at( p );
            if ( objects.at( p ) ) return; /* already taken up by an object */

            if ( c.node() )
                objects[ p ] = &group.add< pic::node >( xpitch * p.x(), -ypitch * p.y(), 2 );

            if ( c.attach( south ) && c.attach( east ) )
                box( p );
        }

        void line( int x, int y ) { line( reader::point( x, y ) ); }
        void line( reader::point p )
        {
            auto c = grid.at( p );

            for ( auto dir : all_dirs )
                if ( c.attach( dir ) && objects.count( p + diff( dir ) ) )
                {
                    arrow( p, dir );
                    break;
                }
        }

        void label( int x, int y ) { label( reader::point( x, y ) ); }
        void label( reader::point p )
        {
            if ( objects.at( p ) )
                return;

            auto origin = p;
            std::u32string txt;

            int spaces = 0;
            for ( int i = 0; ; ++i, p = p + reader::point( 1, 0 ) )
            {
                if ( grid.at( p ).empty() || grid.at( p ).attach() )
                    ++ spaces;
                else
                    spaces = 0;

                if ( spaces == 2 )
                    break;
                else
                    txt += grid.at( p ).character();
            }

            txt.pop_back();
            auto w = p.x() - origin.x() - 1;
            auto obj = &group.add< pic::text >( xpitch * ( origin.x() + w / 2.0 - 0.5 ),
                                                ypitch * ( - p.y() ), txt );
            while ( p != origin ) /* fixme off by one */
            {
                objects[ p ] = obj;
                processed.insert( p );
                p = p + reader::point( -1, 0 );
            }
        }
    };

    static inline group scene( const reader::grid &grid )
    {
        state s( grid );
        auto seen = [&]( int x, int y ) { return s.processed.count( reader::point( x, y ) ); };

        for ( auto [ x, y, c ] : grid )
            if ( c.attach() )
                s.object( x, y );

        for ( auto [ x, y, c ] : grid )
            if ( c.arrow() && !seen( x, y ) )
                s.arrow( x, y );

        for ( auto [ x, y, c ] : grid )
            if ( c.attach() && !seen( x, y ) )
                s.line( x, y );

        for ( auto [ x, y, c ] : grid )
            if ( c.text() && !seen( x, y ) )
                s.label( x, y );

        return s.group;
    }
}
