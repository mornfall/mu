#pragma once
#include "writer.hpp"
#include "w_tex.hpp"
#include <vector>
#include <sstream>
#include <iostream>
#include <cassert>
#include <brick-assert>

namespace umd::doc
{
    struct w_context : w_tex
    {
        w_context( stream &out ) : w_tex( out ) { doctype = U"context"; }

        std::stack< int > section_level;

        bool table_rules;
        int table_rows, table_cells;

        void close_sections( int level )
        {
            while ( !section_level.empty() && section_level.top() >= level )
            {
                out.emit( "\\stop", heading_cmd( section_level.top() ), "\n" );
                section_level.pop();
            }
        }

        void open_section( int level, std::u32string_view num )
        {
            if ( !num.empty() )
                out.emit( "\\setupheadnumber[", heading_cmd( level ), "][-1]\n" );
            out.emit( "\\start", heading_cmd( level ), "[" );
            if ( !num.empty() )
                out.emit( "ownnumber={", num, "}," );
            out.emit( "title={" );
            section_level.push( level );
        }

        void heading_start( int level, sv num, sv ref ) override
        {
            close_sections( level );
            open_section( level, num );
        }

        void heading_stop() override
        {
            out.emit( "}]" );
        }

        /* display math */
        void eqn_start( int n, std::string astr ) override
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
            _in_math = true;
        }

        void eqn_new_cell() override { assert( _in_math ); out.emit( "\\NC " ); }
        void eqn_new_row() override
        {
            assert( _in_math );
            out.emit( "\\NR\n" );
        }

        void eqn_stop() override
        {
            out.emit( "\\stopmathalignment\\stopformula\n" );
            _in_math = false;
        }

        /* lists */
        void enum_start( int level, int first, bool alpha = false ) override
        {
            const char *type;
            switch ( level )
            {
                case 0: type = alpha ? "A" : "n"; break;
                case 1: type = "a"; break;
                case 2: type = "r"; break;
                default: NOT_IMPLEMENTED();
            }

            out.emit( "\\startitemize[packed,", type, "][start=", first, "]", "\n" );
        }

        void enum_stop() override
        {
            out.emit( "\\stopitemize\n" );
        }

        void bullet_start( int level ) override
        {
            out.emit( "\\startitemize[packed,",
                      level == 0 ? 1 : level == 1 ? 5 : 4,
                      "]", "\n" );
        }

        void bullet_item() override { out.emit( "\\item{}" ); }
        void bullet_stop() override
        {
            out.emit( "\\stopitemize\n" );
        }

        /* metapost figures */
        void mpost_start() override
        {
            out.emit( "\\beforepicture\\startMPcode", "\n" );
        }

        void mpost_write( std::string_view sv ) override
        {
            out.emit( sv );
        }

        void mpost_stop() override
        {
            out.emit( "\\stopMPcode\\afterpicture", "\n" );
        }

        /* tables */
        void table_start( columns ci, bool even = false ) override
        {
            table_rules = false;
            table_rows = table_cells = 0;
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

            out.emit( "\\setupTABLE[frame=off" );
            if ( even )
                out.emit( ",width=", 1.0 / ci.size(), "\\textwidth" );
            out.emit( "]\n" );

            for ( auto c : ci )
                out.emit( "\\setupTABLE[c][", std::to_string( ++i ), "][", setup( c ), "]\n" );

            out.emit( "\\beforetable\\bTABLE[boffset=0pt,loffset=\\tablemargin,roffset=\\tablemargin]",
                      table_rules ? "[bottomframe=on]" : "" );
        }

        void table_new_cell( int span ) override
        {
            if ( ++table_cells > 1 )
                out.emit( "\\eTD " );
            out.emit( "\\bTD[nc=", std::to_string( span ), span > 1 ? ",align={center}" : "",
                      table_rules ? ",topframe=on" : ",toffset=0pt", "]" );
        }

        void table_new_row( bool rule = false ) override
        {
            if ( table_cells > 0 )
                out.emit( "\\eTD" );
            if ( ++table_rows > 1 )
                out.emit( "\\eTR" );
            out.emit( "\\bTR\n" );
            table_rules = rule;
            table_cells = 0;
        }

        void table_stop() override
        {
            out.emit( "\\eTD\\eTR" );
            out.emit( "\\eTABLE\\aftertable", "\n" );
        }

        /* blocks */
        void code_start( sv type ) override
        {
            out.emit( "\\starttyping[margin=\\codemargin,option=", type, "]\n" );
        }

        void code_line( sv l ) override { out.emit( l, "\n" ); }
        void code_stop() override { out.emit( "\\stoptyping\n" ); }
        void quote_start() override { out.emit( "\\startblockquote\n" ); }
        void quote_stop() override { out.emit( "\\stopblockquote\n" );  }

        void footnote_start() override { out.emit( "\\footnote{" ); }
        void footnote_stop() override { out.emit( "}" ); }

        void small_start() override { out.emit( "\\switchtobodyfont[sans,\\codesize]" ); }
        void small_stop() override { out.emit( "\\switchtobodyfont[\\bodysize]" ); }

        /* paging */
        void pagebreak() override { out.emit( "\\pagebreak\n" ); }
        void hrule( char32_t ) override
        {
            out.emit( "\\startalignment[center]\\startMPcode\n",
                      "pickup pencircle scaled .2pt;"
                      " draw (0, 0) -- (\\the\\textwidth, 0) dashed evenly withcolor (.3, .3, .3);"
                      " \\stopMPcode\\stopalignment");
        }
        void end() override
        {
            out.emit( "\\stopmakeup\\stoptext", "\n" );
        }
    };

}
