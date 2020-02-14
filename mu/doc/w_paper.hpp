#pragma once
#include "w_tex.hpp"
#include <vector>
#include <sstream>
#include <iostream>

/* Writer for LaTeX-based scientific papers (LNCS, Elsevier, ACM, IEEE). */

namespace umd::doc
{

    struct w_paper : w_tex
    {
        w_paper( stream &out ) : w_tex( out ) {}

        bool table_rules;
        int table_rows;
        bool in_math = false;
        bool first_cell = true;

        virtual void heading( std::u32string_view t, int level )
        {
            out.emit( heading_cmd( level ), "{", t, "}" );
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
            out.emit( "\\begin{align}\n" );
            in_math = true;
        }

        virtual void eqn_new_cell()
        {
            if ( first_cell )
                first_cell = false;
            else
                out.emit( " & " );
            assert( in_math );
        }

        virtual void eqn_new_row()
        {
            out.emit( " \\\\\n" );
            first_cell = true;
            assert( in_math );
        }

        virtual void eqn_stop()
        {
            out.emit( "\\end{align}\n" );
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
        virtual void enum_stop( bool ) { out.emit( "\\stopitemize", "\n" ); }

        virtual void bullet_start( int level )
        {
            out.emit( "\\startitemize[packed,",
                      level == 0 ? 1 : level == 1 ? 5 : 4,
                      "][distance=-4pt]", "\n" );
        }

        virtual void bullet_item() { out.emit( "\\item " ); }
        virtual void bullet_stop( bool ) { out.emit( "\\stopitemize", "\n" ); }

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
            out.emit( "\\begin{longtable}[]{@{}" );
            for ( auto c : ci )
                out.emit( c );
            out.emit( "@{}}", rules ? "\\toprule" : "", "\n" );
        }

        virtual void table_new_cell() { out.emit( " & " ); }
        virtual void table_new_row()
        {
            out.emit( "\\tabularnewline", "\n" );
            if ( ++table_rows == 1 && table_rules )
                out.emit( "\\midrule", "\n" );
        }

        virtual void table_stop()
        {
            out.emit( table_rules ? "\\bottomrule\n" : "", "\\end{longtable}", "\n" );
        }

        /* blocks */
        virtual void code_start( std::u32string )
        {
            out.emit( "\\begin{verbatim}\n" ); /* TODO syntax */
        }

        virtual void code_line( std::u32string_view l ) { out.emit( l, "\n" ); }
        virtual void code_stop() { out.emit( "\\end{verbatim}\n" ); }
        virtual void quote_start() { out.emit( "\\begin{quote}\n" ); }
        virtual void quote_stop() { out.emit( "\\end{quote}\n" );  }

        /* paging */
        virtual void pagebreak() { out.emit( "\\newpage", "\n" ); }
    };

}
