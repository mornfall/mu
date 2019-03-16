#pragma once
#include "types.hpp"
#include <memory>
#include <vector>
#include <iostream>

namespace umd::pic
{
    struct element
    {
        virtual void emit( std::ostream &o ) const = 0;
    };

    std::ostream &operator<<( std::ostream &o, const element &obj )
    {
        obj.emit( o );
        return o;
    }

    struct point : element
    {
        float x, y;

        point( float x, float y ) : x( x ), y( y ) {}

        point operator-( point o ) const { return point( x - o.x, y - o.y ); }
        point operator+( point o ) const { return point( x + o.x, y + o.y ); }

        void emit( std::ostream &o ) const override
        {
            o << "(" << x << ", " << y << ")";
        }
    };

    struct dir : element
    {
        int angle;

        dir( int a ) : angle( a ) {}
        dir operator-() const { return dir( angle + 180 ); }

        void emit( std::ostream &o ) const override
        {
            o << "{dir " << angle << "}";
        }
    };

    struct port : element
    {
        point _position;
        dir _dir;

        port( point pos, dir dir ) : _position( pos ), _dir( dir ) {}
        void emit( std::ostream & ) const override { assert( 0 ); }
    };

    struct port_in : port
    {
        port_in( port p ) : port( p ) {}
        void emit( std::ostream &o ) const override
        {
            o << -_dir << _position;
        }
    };

    struct port_out : port
    {
        port_out( port p ) : port( p ) {}
        void emit( std::ostream &o ) const override
        {
            o << _position << _dir;
        }
    };

    struct arrow : element
    {
        port_out _from;
        port_in _to;

        arrow( port_out f, port_in t ) : _from( f ), _to( t ) {}

        void emit( std::ostream &o ) const override
        {
            o << "drawarrow "<< _from << ".." << _to << " withcolor fg;" << std::endl;
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

        node( point p ) : _position( p ) {}
        node( int x, int y ) : _position( x, y ) {}

        ad::port port( dir_t p ) const override
        {
            switch ( p )
            {
                case north: return pic::port( _position + point( 0, _radius + 1 ), 90 );
                case south: return pic::port( _position - point( 0, _radius + 1 ), -90 );
                case east:  return pic::port( _position + point( _radius + 1, 0 ), 0 );
                case west:  return pic::port( _position - point( _radius + 1, 0 ), 180 );
            }
        }

        void emit( std::ostream &o ) const override
        {
            o << "fill fullcircle scaled " << 2 * _radius << " shifted " << _position
              << " withcolor fg;"
              << std::endl;
        }
    };

    struct box : object
    {
        point _position;
        float _w, _h;

        box( point p, int w, int h ) : _position( p ), _w( w ), _h( h ) {}
        box( int x, int y, int w, int h ) : _position( x, y ), _w( w ), _h( h ) {}

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

        void emit( std::ostream &o ) const override
        {
            o << "draw unitsquare xscaled " << _w << " yscaled " << _h << " shifted ("
              << _position << " + (" << -_w / 2 << ", " << -_h / 2 << ")) withcolor fg;" << std::endl;
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

        void emit( std::ostream &o ) const override
        {
            for ( auto obj : _objects )
                obj->emit( o );
        }
    };

}
