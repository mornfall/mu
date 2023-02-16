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

        void heading_start( int level, std::u32string_view num ) override
        {
            close_sections( level );
            if ( level == 3 )
            {
                out.emit( "\\blank[big]" );
                out.buffer( "heading" );
            }
            open_section( level, num );
        }

        void heading_stop() override
        {
            w_slides::heading_stop();
            out.resume();
        }

        void nest_start() override
        {
            ASSERT_LEQ( nest_level, 1 );
            out.emit( "\\placefloat[slideflt][rightmargin][]{}{\n" );
            nest_level ++;
        }

        void nest_end() override
        {
            out.emit( "}\n" );
            nest_level --;
            out.flush( "heading" );
        }

        void pagebreak() override { out.emit( "\\page", "\n" ); }
    };

}
