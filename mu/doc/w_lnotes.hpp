#pragma once
#include "writer.hpp"
#include "w_tex.hpp"
#include <vector>
#include <sstream>
#include <iostream>

/* Writer for ConTeXt-based PDF lecture notes (slides interspersed with free
 * text). */

namespace umd::doc
{
    struct w_lnotes : w_slides
    {
        w_lnotes( stream &out ) : w_slides( out ) { format = U"lnotes"; }
        int nest_level = 0;
        bool in_cols = false;

        void meta_end() override
        {
            w_slides::meta_end();
            out.emit( "\\ifnotes" );
        }

        void heading( std::u32string_view t, int level ) override
        {
            close_sections( level );
            if ( level == 1 && in_cols )
                out.emit( "\\stopsectioncolumns" );
            out.emit( "\\fi" );
            open_section( t, level );
            out.emit( "\\ifnotes" );
            if ( level == 1 )
                in_cols = true, out.emit( "\\startsectioncolumns\n" );
        }

        void nest_start() override
        {
            out.emit( "\\fi\\startslide", "\n" );
            nest_level ++;
        }

        void nest_end() override
        {
            out.emit( "\\stopslide\\ifnotes\n" );
            nest_level --;
        }

        void pagebreak() override { out.emit( "\\page", "\n" ); }
        void end() override { out.emit( "\\stopsectioncolumns\\fi\\stoptext", "\n" ); }
    };

}
