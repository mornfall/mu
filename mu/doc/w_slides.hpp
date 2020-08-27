#pragma once
#include "writer.hpp"
#include "w_tex.hpp"
#include <vector>
#include <sstream>
#include <iostream>
#include <cassert>

/* Writer for ConTeXt-based PDF slides. */

namespace umd::doc
{

    struct w_slides : w_tex
    {
        w_slides( stream &out ) : w_tex( out ) { format = U"slides"; }

        std::stack< int > section_level;

        bool table_rules;
        int table_rows, table_cells;
        int math_negspace = 0;

        void close_sections( int level )
        {
            while ( !section_level.empty() && section_level.top() >= level )
            {
                out.emit( "\\stop", heading_cmd( section_level.top() ), "\n" );
                section_level.pop();
            }
        }

        void open_section( std::u32string_view t, int level )
        {
            out.emit( "\\start", heading_cmd( level ), "[title={", t, "}]" );
            section_level.push( level );
        }

        virtual void heading( std::u32string_view t, int level )
        {
            close_sections( level );
            open_section( t, level );
        }

        /* display math */
        virtual void eqn_start( int n, std::string astr )
        {
            out.emit( "\\startformula\\startmathalignment[n=", n, ",align={" );
            for ( char c : astr )
                switch ( c )
                {
                    case 'c': out.emit( "middle," ); break;
                    case 'l': out.emit( "left," ); break;
                    case 'r': out.emit( "right," ); break;
                }
            out.emit( "}]\n" );
            out.emit( "\\noalign{\\blank[-1.5ex]}\n" );
            _in_math = true;
            math_negspace = 2;
        }

        virtual void eqn_new_cell() { assert( _in_math ); out.emit( "\\NC " ); }
        virtual void eqn_new_row()
        {
            assert( _in_math );
            out.emit( "\\NR\\noalign{\\blank[-.3ex]}\n" );
            if ( math_negspace ) -- math_negspace;
        }

        virtual void eqn_stop()
        {
            if ( math_negspace )
                out.emit( "\\noalign{\\blank[-1.5ex]}" );
            out.emit( "\\stopmathalignment\\stopformula\n" );
            _in_math = false;
        }

        /* lists */
        virtual void enum_start( int level, int first )
        {
            out.emit( "\\startitemize[packed,",
                      level == 0 ? 'n' : level == 1 ? 'a' : 'r',
                      "][distance=-2pt,start=", first, "]", "\n" );
        }

        virtual void enum_stop( bool xspace )
        {
            out.emit( "\\stopitemize", xspace ? "\\vskip3pt\n" : "\n" );
        }

        virtual void bullet_start( int level )
        {
            out.emit( "\\startitemize[packed,",
                      level == 0 ? 1 : level == 1 ? 5 : 4,
                      "][distance=-4pt]", "\n" );
        }

        virtual void bullet_item() { out.emit( "\\item " ); }
        virtual void bullet_stop( bool xspace )
        {
            out.emit( "\\stopitemize", xspace ? "\\vskip3pt\n" : "\n" );
        }

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
            table_rows = table_cells = 0;
            out.emit( "\\placetable[force,none]{}{\\blank[-1ex]\n" );
            int i = 0;
            auto setup = []( char c )
            {
                switch ( c )
                {
                    case 'c': return "align={center}";
                    case 'r': return "align={flushright}";
                    case '|': return "leftframe=on,align={center}";
                    case ']': return "leftframe=on,align={flushright}";
                    case '[': return "leftframe=on,align={flushleft}";
                    default: return "";
                }
            };

            out.emit( "\\setupTABLE[frame=off]\n" );

            for ( auto c : ci )
                out.emit( "\\setupTABLE[c][", std::to_string( ++i ), "][", setup( c ), "]\n" );

            out.emit( "\\bTABLE[toffset=-1pt,boffset=-1pt,loffset=2pt,roffset=2pt]\\bTR\n" );
            out.emit( rules ? "\\HL" : "", "\n" );
        }

        virtual void table_new_cell( int span )
        {
            if ( ++table_cells > 1 )
                out.emit( "\\eTD " );
            out.emit( "\\bTD[nc=", std::to_string( span ), span > 1 ? ",align={center}" : "", "]" );
        }

        virtual void table_new_row()
        {
            out.emit( "\\eTD\\eTR\\bTR", "\n" );
            table_cells = 0;
        }

        virtual void table_stop()
        {
            out.emit( "\\eTD\\eTR" );
            out.emit( "\\eTABLE\\blank[-1ex]}", "\n" );
        }

        /* blocks */
        virtual void code_start( sv type )
        {
            out.emit( "\\starttyping[margin=10pt,option=", type, "]\n" );
        }

        virtual void code_line( sv l ) { out.emit( l, "\n" ); }
        virtual void code_stop() { out.emit( "\\stoptyping\n" ); }
        virtual void quote_start() { out.emit( "\\startblockquote\n" ); }
        virtual void quote_stop() { out.emit( "\\stopblockquote\n" );  }

        /* paging */
        virtual void pagebreak() { out.emit( "\\stopmakeup\\startmakeup[slide]", "\n" ); }
        virtual void hrule( char32_t )
        {
            out.emit( "\\vskip-8pt\\startalignment[center]\\startMPcode\n",
                      "pickup pencircle scaled .2pt;"
                      " draw (0, 0) -- (\\the\\textwidth, 0) dashed evenly withcolor (.3, .3, .3);"
                      " \\stopMPcode\\stopalignment");
        }
        virtual void end()
        {
            out.emit( "\\stopmakeup\\stoptext", "\n" );
        }
    };

}
