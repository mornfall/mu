#pragma once
#include "writer.hpp"
#include "w_tex.hpp"
#include <vector>
#include <sstream>
#include <iostream>

/* Writer for ConTeXt-based PDF slides. */

namespace umd::doc
{

    struct w_slides : w_tex
    {
        w_slides( stream &out ) : w_tex( out ) { format = U"slides"; }

        bool table_rules;
        int table_rows;
        int math_negspace = 0;

        virtual void heading( std::u32string_view t, int level )
        {
            out.emit( heading_cmd( level ), "[title={", t, "}]" );
        }

        /* spans ; may be also called within mpost btex/etex */
        virtual void em_start() { out.emit( "{\\em{}" ); }
        virtual void em_stop()  { out.emit( "}" ); }
        virtual void tt_start() { out.emit( "{\\code{}" ); }
        virtual void tt_stop()  { out.emit( "}" ); }
        virtual void math_start() { out.emit( "\\math{" ); in_math = true; }
        virtual void math_stop() { out.emit( "}" ); in_math = false; }

        /* display math */
        virtual void eqn_start()
        {
            out.emit( "\\startformula\\startmathalignment\\noalign{\\blank[-1.5ex]}\n" );
            in_math = true;
            math_negspace = 2;
        }

        virtual void eqn_new_cell() { assert( in_math ); out.emit( "\\NC " ); }
        virtual void eqn_new_row()
        {
            assert( in_math );
            out.emit( "\\NR\\noalign{\\blank[-.8ex]}\n" );
            if ( math_negspace ) -- math_negspace;
        }

        virtual void eqn_stop()
        {
            if ( math_negspace )
                out.emit( "\\noalign{\\blank[-1.5ex]}" );
            out.emit( "\\stopmathalignment\\stopformula\n" );
            in_math = false;
        }

        /* lists */
        virtual void enum_start( int level )
        {
            out.emit( "\\startitemize[packed,",
                      level == 0 ? 'n' : level == 1 ? 'a' : 'r',
                      "][distance=-2pt]", "\n" );
        }

        virtual void enum_item() { out.emit( "\\item " ); }
        virtual void enum_stop() { out.emit( "\\stopitemize", "\n" ); }

        virtual void bullet_start( int level )
        {
            out.emit( "\\startitemize[packed,",
                      level == 0 ? 1 : level == 1 ? 5 : 4,
                      "][distance=-4pt]", "\n" );
        }

        virtual void bullet_item() { out.emit( "\\item " ); }
        virtual void bullet_stop() { out.emit( "\\stopitemize", "\n" ); }

        /* metapost figures */
        virtual void mpost_start()
        {
            out.emit( "\\blank[medium]\\startalignment[center]\\strut\\startMPcode", "\n" );
        }

        virtual void mpost_write( std::string_view sv )
        {
            out.emit( sv );
        }

        virtual void mpost_stop()
        {
            out.emit( "\\stopMPcode\\stopalignment\\blank[medium]", "\n" );
        }

        /* tables */
        virtual void table_start( columns ci, bool rules )
        {
            table_rules = rules;
            table_rows = 0;
            out.emit( "\\placetable[force,none]{}{\\blank[-1ex]",
                      "\\starttabulate[" );
            for ( auto c : ci )
                out.emit( "|", c );
            out.emit( "|]", rules ? "\\HL" : "", "\n" );
        }

        virtual void table_new_cell() { out.emit( "\\NC " ); }
        virtual void table_new_row()
        {
            out.emit( "\\NR", "\n" );
            if ( ++table_rows == 1 && table_rules )
                out.emit( "\\HL", "\n" );
        }

        virtual void table_stop()
        {
            out.emit( table_rules ? "\\HL\n" : "", "\\stoptabulate\\blank[-1ex]}", "\n" );
        }

        /* blocks */
        virtual void code_start( std::string type )
        {
            out.emit( "\\starttyping[margin=10pt,option=", type, "]\n" );
        }

        virtual void code_line( std::u32string_view l ) { out.emit( l, "\n" ); }
        virtual void code_stop() { out.emit( "\\stoptyping\n" ); }
        virtual void quote_start() { out.emit( "\\startblockquote\n" ); }
        virtual void quote_stop() { out.emit( "\\stopblockquote\n" );  }

        /* paging */
        virtual void pagebreak() { out.emit( "\\stopmakeup\\startmakeup[slide]", "\n" ); }
    };

}
