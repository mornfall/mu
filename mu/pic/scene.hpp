#pragma once
#include "types.hpp"
#include "writer.hpp"

#include <memory>
#include <vector>
#include <codecvt>

namespace umd::pic
{
    struct element
    {
        virtual void emit( writer &w ) const = 0;
    };

    writer &operator<<( writer &w, const element &obj )
    {
        obj.emit( w );
        return w;
    }

    struct point : element
    {
        float x, y;

        point( float x, float y ) : x( x ), y( y ) {}

        point operator-( point o ) const { return point( x - o.x, y - o.y ); }
        point operator+( point o ) const { return point( x + o.x, y + o.y ); }

        void emit( writer &w ) const override
        {
            w << "(" << x << ", " << y << ")";
        }
    };

    struct dir : element
    {
        int angle;

        dir( int a ) : angle( a ) {}
        dir operator-() const { return dir( angle + 180 ); }

        void emit( writer &w ) const override
        {
            w << "{dir " << angle << "}";
        }
    };

    struct port : element
    {
        point _position;
        dir _dir;

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

        arrow( port_out f, port_in t ) : _from( f ), _to( t ) {}

        void emit( writer &o ) const override
        {
            o << "drawarrow "<< _from << ".." << _to << " withcolor fg;" << std::endl;
        }
    };

    struct label : element
    {
        point _position;
        std::u32string _text;

        label( point pos, std::u32string s ) : _position( pos ), _text( s ) {}
        label( int x, int y, std::u32string s ) : _position( x, y ), _text( s ) {}

        void emit( writer &o ) const override
        {
            std::wstring_convert< std::codecvt_utf8< char32_t >, char32_t > conv;
            o << "label( btex \\strut{}";
            o.emit_tex( _text );
            o << "\\strut etex, " << _position << ");\n";
        }
    };

    struct object : element
    {
        virtual port port( dir_t p ) const = 0;
        port_out out( dir_t p ) const { return port( p ); }
        port_in in( dir_t p ) const { return port( p ); }
    };

    struct node : object
    {
        int _radius = 5;
        point _position;

        node( point p, int r = 5 ) : _position( p ), _radius( r ) {}
        node( int x, int y, int r = 5 ) : _position( x, y ), _radius( r ) {}

        pic::port port( dir_t p ) const override
        {
            switch ( p )
            {
                case north: return pic::port( _position + point( 0, _radius + 1 ), 90 );
                case south: return pic::port( _position - point( 0, _radius + 1 ), -90 );
                case east:  return pic::port( _position + point( _radius + 1, 0 ), 0 );
                case west:  return pic::port( _position - point( _radius + 1, 0 ), 180 );
            }
        }

        void emit( writer &o ) const override
        {
            o << "fill fullcircle scaled " << 2 * _radius << " shifted " << _position
              << " withcolor fg;\n";
        }
    };

    struct box : object
    {
        point _position;
        float _w, _h;
        std::bitset< 4 > _rounded;

        box( point p, int w, int h ) : _position( p ), _w( w ), _h( h ) {}
        box( int x, int y, int w, int h ) : _position( x, y ), _w( w ), _h( h ) {}

        void set_rounded( int c, bool r ) { _rounded[ c ] = r; }

        pic::port port( dir_t p ) const override
        {
            switch ( p )
            {
                case north: return pic::port( _position + point( 0, +_h / 2 + 1.5 ), 90 );
                case south: return pic::port( _position + point( 0, -_h / 2 - 1.5 ), -90 );
                case east: return  pic::port( _position + point( +_w / 2 + 1.5, 0 ), 0 );
                case west: return  pic::port( _position + point( -_w / 2 - 1.5, 0 ), 180 );
            }
        }

        void emit( writer &o ) const override
        {
            point nw = _position + point( -_w/2,  _h/2 ),
                  ne = _position + point(  _w/2,  _h/2 ),
                  se = _position + point(  _w/2, -_h/2 ),
                  sw = _position + point( -_w/2, -_h/2 );
            auto round_x = [&]( int p ) { return point( _rounded[ p ] ? 8 : 0, 0 ); };
            auto round_y = [&]( int p ) { return point( 0, _rounded[ p ] ? 8 : 0 ); };

            o << "draw " << nw + round_x( 0 ) << " -- "
              << ne - round_x( 1 ) << " .. controls " << ne << " .. " << ne - round_y( 1 ) << " -- "
              << se + round_y( 2 ) << " .. controls " << se << " .. " << se - round_x( 2 ) << " -- "
              << sw + round_x( 3 ) << " .. controls " << sw << " .. " << sw + round_y( 3 ) << " -- "
              << nw - round_y( 0 ) << " .. controls " << nw << " .. " << " cycle withcolor fg;\n";
        }
    };

    using element_ptr = std::shared_ptr< element >;
    using object_ptr = std::shared_ptr< object >;

    struct group : element
    {
        std::vector< element_ptr > _objects;

        template< typename T, typename... Args >
        T &add( Args && ... args )
        {
            auto ptr = std::make_shared< T >( std::forward< Args >( args )... );
            _objects.push_back( ptr );
            return *ptr;
        }

        void emit( writer &o ) const override
        {
            for ( auto obj : _objects )
                obj->emit( o );
        }
    };

}
