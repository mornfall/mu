#pragma once
#include "writer.hpp"
#include "w_context.hpp"
#include <vector>
#include <sstream>
#include <iostream>

namespace umd::doc
{
    struct w_slides : w_context
    {
        int last_head = 0;
        bool in_slide = false;

        w_slides( stream &out ) : w_context( out )
        {
            doctype = U"slides";
        }

        void meta_end() override
        {
            w_context::meta_end();
            out.buffer( "not_slides" );
        }

        void heading_start( int l, sv num ) override
        {
            last_head = l;
            slide_start();
            w_context::heading_start( l, num );
        }

        void heading_stop() override
        {
            w_context::heading_stop();
            if ( last_head < 3 )
                slide_end();
        }

        void mpost_start() override { slide_start(); w_context::mpost_start(); }
        void mpost_stop() override  { w_context::mpost_stop(); slide_end(); }
        void nest_start() override  { slide_start(); w_context::nest_start(); }
        void nest_end() override    { w_context::nest_end(); slide_end(); }

        void table_start( columns ci, bool even = false ) override
        {
            slide_start();
            w_context::table_start( ci, even );
            out.emit( "\\switchtobodyfont[6pt]\n" );
        }

        void table_stop() override
        {
            w_context::table_stop();
            slide_end();
        }

        void slide_start()
        {
            if ( !in_slide )
            {
                out.resume();
                out.emit( "\\startslide\n" );
                in_slide = true;
            }
        }

        void slide_end()
        {
            ASSERT( in_slide );
            out.emit( "\\stopslide\n" );
            out.buffer( "not_slides" );
            in_slide = false;
        }
    };
}
