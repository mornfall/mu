// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

#pragma once
#include "types.hpp"
#include "writer.hpp"
#include <brick-string>

#include <memory>
#include <vector>
#include <bitset>
#include <codecvt>
#include <locale>
#include <cmath>
#include <cassert>

namespace umd::pic
{
    struct element
    {
        virtual void emit( writer &w ) const = 0;
        virtual void fill( writer & ) const {}
        virtual ~element() {}
    };

    inline writer &operator<<( writer &w, const element &obj )
    {
        obj.emit( w );
        return w;
    }

    inline writer &operator<<( writer &w, std::string_view sv )
    {
        w.emit_mpost( sv );
        return w;
    }

    struct point : element
    {
        float x, y;

        point( float x, float y ) : x( x ), y( y ) {}

        point operator-( point o ) const { return point( x - o.x, y - o.y ); }
        point operator+( point o ) const { return point( x + o.x, y + o.y ); }
        point operator*( float s ) const { return point( x * s, y * s ); }
        point operator/( float s ) const { return point( x / s, y / s ); }

        void emit( writer &w ) const override
        {
            w << "(" << std::to_string( x ) << ", " << std::to_string( y ) << ")";
        }
    };

    struct dir : element
    {
        int angle;

        dir( int a ) : angle( a ) {}
        dir operator-() const { return dir( angle + 180 ); }

        void emit( writer &w ) const override
        {
            w << "{dir " << std::to_string( angle ) << "}";
        }
    };

    inline point dir_to_vec( dir d )
    {
        auto rad = 4 * std::atan( 1 ) * float( d.angle ) / 180;
        return point( std::cos( rad ), std::sin( rad ) );
    }

    struct port : element
    {
        point _position;
        dir _dir;

        point position() const { return _position; }
        dir direction() const { return _dir; }
        port( point pos, dir dir ) : _position( pos ), _dir( dir ) {}
        void emit( writer & ) const override { assert( 0 ); }
    };

    struct port_in : port
    {
        port_in( port p ) : port( p ) {}

        void emit( writer &o ) const override
        {
            o << -_dir << _position;
        }
    };

    struct port_out : port
    {
        port_out( port p ) : port( p ) {}

        void emit( writer &o ) const override
        {
            o << _position << _dir;
        }
    };

    struct arrow : element
    {
        port_out _from;
        port_in _to;
        bool _dashed = false, _curved = false, _head = true;
        int _shade = 0;
        std::vector< point > _controls;

        arrow( port_out f, port_in t ) : _from( f ), _to( t ) {}

        void emit_curved( writer &o ) const
        {
            std::vector< point > through;

            if ( !_controls.empty() )
                through.emplace_back( ( _from.position() + _controls.front() ) / 2 );

            for ( auto i = _controls.begin();
                  i != _controls.end() && std::next( i ) != _controls.end(); ++i )
                through.emplace_back( ( *i + *std::next( i ) ) / 2 );

            o << _from.position() << " .. ";

            for ( int i = 0; i < int( _controls.size() ); ++i )
                o << through[ i ] << " .. controls " << _controls[ i ] << " .. ";

            if ( !_controls.empty() )
                o << ( _controls.back() + _to.position() ) / 2 << " .. ";
            o << _to.position();
        }

        void emit_angled( writer &o ) const
        {
            o << _from.position() << " -- ";
            for ( auto c : _controls )
                o << c << " -- ";
            o << _to.position();
        }

        void emit( writer &o ) const override
        {
            o << ( _head ? "drawarrow " : "draw " );
            if ( _curved )
                emit_curved( o );
            else
                emit_angled( o );
            o << ( _dashed ? " dashed dotted" : "" )
              << " withcolor fg_" << std::string( 1, "abcde"[ _shade ] ) << ";\n";
        }
    };

    struct label : virtual element
    {
        point _position;
        std::u32string _text;

        label( point pos, std::u32string s ) : _position( pos ), _text( s ) {}
        label( double x, double y, std::u32string s ) : _position( x, y ), _text( s ) {}

        void emit( writer &o ) const override
        {
            std::wstring_convert< std::codecvt_utf8< char32_t >, char32_t > conv;
            o << "label( btex \\strut{}";
            o.emit_tex( _text );
            o << "\\strut etex, " << _position << ");\n";
        }
    };

    struct object : virtual element
    {
        virtual pic::port port( dir_t p ) const = 0;
        port_out out( dir_t p ) const { return port( p ); }
        port_in in( dir_t p ) const { return port( p ); }
    };

    struct node : object
    {
        point _position;
        int _radius = 5;

        node( point p, int r = 5 ) : _position( p ), _radius( r ) {}
        node( double x, double y, int r = 5 ) : _position( x, y ), _radius( r ) {}

        pic::port port( dir_t p ) const override
        {
            auto mk = [&]( int x, int y, int deg )
            {
                double rad = _radius + 1, xr = x * rad, yr = y * rad;
                double len = std::sqrt( xr * xr + yr * yr );
                return pic::port( _position + point( xr, yr ), deg );
            };

            switch ( p )
            {
                case north: return mk(  0,  1,  90 );
                case south: return mk(  0, -1, -90 );
                case east:  return mk(  1,  0,   0 );
                case west:  return mk( -1,  0, 180 );
                case north_east: return mk(  1,  1,  -45 );
                case north_west: return mk( -1,  1, -135 );
                case south_east: return mk(  1, -1,   45 );
                case south_west: return mk( -1, -1,  135 );
                default: assert( 0 && "unexpected direction in node::port()" );
            }
        }

        void emit( writer &o ) const override
        {
            o << "fill fullcircle scaled " << std::to_string( 2 * _radius )
              << " shifted " << _position << " withcolor fg;\n";
        }
    };

    struct text : object, label
    {
        using label::label;
        pic::port port( dir_t ) const override { abort(); }
        void emit( writer &o ) const override { label::emit( o ); }
    };

    struct box : object
    {
        point _position;
        float _w, _h;
        std::bitset< 4 > _rounded;
        std::bitset< 4 > _dashed, _hidden;
        int _shaded = 0;

        box( point p, float w, float h ) : _position( p ), _w( w ), _h( h ) {}
        box( float x, float y, float w, float h ) : _position( x, y ), _w( w ), _h( h ) {}

        void set_shaded( int s ) { _shaded = s; }
        void set_rounded( int c, bool r ) { _rounded[ c ] = r; }
        void set_dashed( int s, bool d ) { _dashed[ s ] = d; }
        void set_visible( int s, bool d ) { _hidden[ s ] = !d; }

        float width() const { return _w; }
        float height() const { return _h; }

        pic::port port( dir_t p ) const override
        {
            switch ( p )
            {
                case north: return pic::port( _position + point( 0, +_h / 2 + 1.5 ), 90 );
                case south: return pic::port( _position + point( 0, -_h / 2 - 1.5 ), -90 );
                case east: return  pic::port( _position + point( +_w / 2 + 1.5, 0 ), 0 );
                case west: return  pic::port( _position + point( -_w / 2 - 1.5, 0 ), 180 );
                default: __builtin_trap();
            }
        }

        writer &path( writer &o, bool outline ) const
        {
            auto round_x = [&]( int p ) { return point( _rounded[ p ] ? 8 : 0, 0 ); };
            auto round_y = [&]( int p ) { return point( 0, _rounded[ p ] ? 8 : 0 ); };

            point nw = _position + point( -_w/2,  _h/2 ),
                  ne = _position + point(  _w/2,  _h/2 ),
                  se = _position + point(  _w/2, -_h/2 ),
                  sw = _position + point( -_w/2, -_h/2 );

            auto corner = []( writer &w, auto t ) -> writer &
            {
                auto [ a, b, c ] = t;
                return w << a << ".. controls" << b << ".." << c;
            };

            auto line = [&]( int i, auto f, auto t )
            {
                if ( !_hidden[ i ] )
                {
                    o << "draw ";
                    corner( o, f ) << " -- " << std::get< 0 >( t );
                    if ( _dashed[ i ] )
                        o << " dashed dotted";
                    o << " withcolor fg;";
                }
            };

            std::tuple c_nw{ nw - round_y( 0 ), nw, nw + round_x( 0 ) },
                       c_ne{ ne - round_x( 1 ), ne, ne - round_y( 1 ) },
                       c_se{ se + round_y( 2 ), se, se - round_x( 2 ) },
                       c_sw{ sw + round_x( 3 ), sw, sw + round_y( 3 ) };

            if ( outline )
            {
                line( 0, c_nw, c_ne );
                line( 1, c_ne, c_se );
                line( 2, c_se, c_sw );
                line( 3, c_sw, c_nw );
            }
            else
            {
                corner( o, c_nw ) << " -- ";
                corner( o, c_ne ) << " -- ";
                corner( o, c_se ) << " -- ";
                corner( o, c_sw ) << " -- cycle";
            }

            return o;
        }

        void emit( writer &o ) const override
        {
            path( o, true );
        }

        void fill( writer &o ) const override
        {
            auto col = 1 - _shaded * 1.0 / 4;

            if ( _shaded )
            {
                o << "fill ";
                path( o, false );
                o << " withcolor shade_" << std::string( 1, "abcd"[ _shaded - 1 ] ) << ";\n";
            }
        }
    };

    using element_ptr = std::shared_ptr< element >;
    using object_ptr = std::shared_ptr< object >;

    struct group : element
    {
        std::vector< element_ptr > _objects;

        template< typename T, typename... Args >
        auto add( Args && ... args )
        {
            auto ptr = std::make_shared< T >( std::forward< Args >( args )... );
            _objects.push_back( ptr );
            return ptr;
        }

        void emit( writer &o ) const override
        {
            for ( auto obj : _objects )
                obj->fill( o );
            for ( auto obj : _objects )
                obj->emit( o );
        }
    };

}
