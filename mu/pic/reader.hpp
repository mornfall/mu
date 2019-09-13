// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

#pragma once
#include "types.hpp"

#include <map>
#include <bitset>

namespace umd::pic::reader
{
    struct point : std::pair< int, int >
    {
        using std::pair< int, int >::pair;
        int &x() { return first; }
        int &y() { return second; }

        friend std::ostream &operator<<( std::ostream &o, point p )
        {
            return o << "[" << p.x() << ", " << p.y() << "]";
        }

        point operator+( point o ) { return point( x() + o.x(), y() + o.y() ); }
    };

    struct cell
    {
        char32_t _char = U' ';
        std::bitset< 4 > _attach;
        std::bitset< 4 > _arrow;
        bool _rounded = false;

        bool text() const { return ( _char < 0x2500 || _char > 0x2580 ) && !attach() && !arrow() && !empty(); }
        bool empty() const { return _char == U' '; }
        bool node() const { return _char == U'●'; }
        bool rounded() const { return _rounded; }
        bool attach( dir_t dir ) const { return _attach[ dir ]; }
        bool attach() const { return _attach.any(); }

        char32_t character() const { return _char; }
        bool attach_all() const { return _attach.count() == 4; }
        dir_t attach_dir() const { return dir( _attach ); }
        dir_t attach_dir( dir_t except ) const
        {
            auto mod = _attach;
            mod[ except ] = false;
            return dir( mod );
        }

        bool arrow( dir_t dir ) const { return _arrow[ dir ]; }
        bool arrow() const { return _arrow.any(); }
        dir_t arrow_dir() const { return dir( _arrow ); }

        dir_t dir( std::bitset< 4 > bs ) const
        {
            assert( bs.count() == 1 );
            for ( dir_t dir = north; dir <= west; dir = dir_t( dir + 1 ) )
                if ( bs[ dir ] )
                    return dir;
            assert( 0 );
        }

        cell &set_attach( dir_t d ) { _attach[ d ] = true; return *this; }
        cell &set_arrow( dir_t d )  { _arrow[ d ] = true; return *this; }
        cell &set_char( uint32_t c ) { _char = c; return *this; }
        cell &set_rounded( bool b ) { _rounded = b; return *this; }

        cell( uint32_t c ) : _char( c )
        {
            switch ( c )
            {
                case U'◀': set_attach( east ); set_arrow( west ); break;
                case U'▶': set_attach( west ); set_arrow( east ); break;
                case U'▲': set_attach( south ); set_arrow( north ); break;
                case U'▼': set_attach( north ); set_arrow( south ); break;

                case U'│': set_attach( north ); set_attach( south ); break;
                case U'─': set_attach( east ); set_attach( west ); break;

                case U'╭': set_rounded( true );
                case U'┌': set_attach( east ); set_attach( south ); break;
                case U'╮': set_rounded( true );
                case U'┐': set_attach( west ); set_attach( south ); break;
                case U'╰': set_rounded( true );
                case U'└': set_attach( east ); set_attach( north ); break;
                case U'╯': set_rounded( true );
                case U'┘': set_attach( west ); set_attach( north ); break;

                case U'┤': set_attach( west ); set_attach( north ); set_attach( south ); break;
                case U'├': set_attach( east ); set_attach( north ); set_attach( south ); break;
                case U'┬': set_attach( east ); set_attach( west );  set_attach( south ); break;
                case U'┴': set_attach( east ); set_attach( west );  set_attach( north ); break;

                case U'┼': case U'●':
                    set_attach( north ); set_attach( south );
                    set_attach( east ); set_attach( west );
                    break;

                default: ;
            }
        }

        cell() = default;
    };

    struct grid
    {
        int _x = 0, _y = 0;
        std::vector< std::tuple< int, int, cell > > _iterable;
        std::vector< std::vector< cell > > _indexable;

        grid() { _indexable.emplace_back(); }

        void add( uint32_t c )
        {
            if ( c == U'\n' )
            {
                _indexable.emplace_back();
                ++ _y;
                _x = 0;
            }
            else
            {
                _iterable.emplace_back( _x++, _y, c );
                _indexable.back().emplace_back( c );
            }
        }

        cell at( int x, int y ) const
        {
            if ( x < 0 || y < 0 ) return cell();
            if ( y >= _indexable.size() ) return cell();
            auto &row = _indexable[ y ];
            if ( x >= row.size() ) return cell();
            return row[ x ];
        }

        cell at( point p ) const
        {
            return at( p.x(), p.y() );
        }

        cell operator[]( point p ) const { return at( p ); }

        auto begin() const { return _iterable.begin(); }
        auto end() const { return _iterable.end(); }
    };

    static inline grid read_grid( std::u32string_view ptr )
    {
        grid g;
        while ( !ptr.empty() )
            g.add( ptr[0] ), ptr.remove_prefix( 1 );
        return g;
    }
}
