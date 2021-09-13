#pragma once
#include "writer.hpp"
#include "util.hpp"
#include <vector>
#include <sstream>
#include <iostream>
#include <map>

namespace umd::doc
{

    struct w_html : w_tex /* w_tex for math */
    {
        std::string _embed;

        w_html( stream &out, std::string embed = "" ) : w_tex( out ), _embed( embed )
        {
            _sections.resize( 7, 0 );
            _section_num.resize( 7 );
        }

        std::map< sv, sv > _meta;

        std::vector< int > _sections;
        std::vector< sv > _section_num;

        bool _table_rule = false;
        int _table_rows, _table_cells;
        std::vector< std::string > _table_class;

        bool _in_mpost = false;
        int _heading = 0; // currently open <hN> tag (must not be nested)

        void meta( sv key, sv value ) override { _meta[ key ] = value; }
        void meta_end() override
        {
            if ( _meta[ U"naked" ] == U"yes" )
                return out.emit( "<div>" );

            out.emit( "<!DOCTYPE html>" );
            out.emit( "<html lang=\"", _meta[ U"lang" ], "\"><head>" );
            out.emit( "<meta charset=\"UTF-8\">" );
            out.emit( "<title>", _meta[ U"title" ], "</title>" );
            auto css = to_utf8( _meta[ U"doctype" ] ) + ".css";

            if ( _embed.empty() )
            {
                out.emit( "<link rel=\"stylesheet\" href=\"", css ,"\">" );
                out.emit( "<link rel=\"stylesheet\" href=\"fonts.css\">" );
                out.emit( "<script src=\"highlight.js\"></script>" );
                out.emit( "<script src=\"toc.js\"></script>" );
            }
            else
            {
                out.emit( "<style>",  read_file( _embed + "/" + css ), "</style>" );
                out.emit( "<style>",  read_file( _embed + "/fonts.css" ), "</style>" );
                out.emit( "<script>", read_file( _embed + "/highlight.js" ), "</script>" );
                out.emit( "<script>", read_file( _embed + "/toc.js" ), "</script>" );
            }
            out.emit( "<script>hljs.initHighlightingOnLoad();</script>" );
            out.emit( "</head><body onload=\"makeTOC()\"><ol id=\"toc\"></ol><div>" );
        }

        void html( sv raw ) override { out.emit( raw ); }
        void end() override
        {
            if ( _meta[ U"naked" ] != U"yes" )
                out.emit( "</div></body></html>" );
            else
                out.emit( "</div>" );
        }

        void text( std::u32string_view t ) override
        {
            auto char_cb = [&]( auto flush, char32_t c )
            {
                switch ( c )
                {
                    case 0x0307:
                        if ( _in_math )
                            flush( 2 ), out.emit( "\\dot " ), flush();
                        break;
                    case U'&': flush(); out.emit( "&amp;" ); break;
                    case U'<': flush(); out.emit( "&lt;" ); break;
                    case U'>': flush(); out.emit( "&gt;" ); break;
                    default: ;
                }
            };

            if ( _in_math )
                w_tex::text( t );
            else
                process( t, char_cb, [&]( auto s ) { out.emit( s ); } );
        }

        void heading_start( int level, std::u32string_view num ) override
        {
            _heading = level;
            out.emit( "<h", level, ">" );

            for ( int i = 1; i < level; ++i )
                if ( _section_num[ i ].empty() )
                    out.emit( _sections[ i ], "." );
                else
                    out.emit( _section_num[ i ], "." );

            if ( num.empty() )
            {
                out.emit( ++ _sections[ level ] );
                _section_num[ level ] = U"";
            }
            else
                out.emit( _section_num[ level ] = num );

            for ( int i = level + 1; i < 6; ++i )
                _sections[ i ] = 0, _section_num[ i ] = U"";

            out.emit( " " );
        }

        void heading_stop() override
        {
            out.emit( "</h", _heading, ">" );
        }

        /* spans ; may be also called within mpost btex/etex */
        void em_start()   override { out.emit( _in_mpost ? "{\\em{}" : "<em>" ); }
        void em_stop()    override { out.emit( _in_mpost ? "}" : "</em>" ); }
        void tt_start()   override { out.emit( _in_mpost ? "{\\tt{}" : "<code>" ); }
        void tt_stop()    override { out.emit( _in_mpost ? "}" : "</code>" ); }

        /* generate svgtex-compatible markup */
        void eqn_start( int n, std::string ) override
        {
            _in_math = true;
            out.emit( "<tex>\\startTEXpage\\startformula[packed]\\startmathalignment[n=", n, "]" );
        }

        void eqn_new_cell() override { out.emit( "\\NC" ); }
        void eqn_new_row() override { out.emit( "\\NR" ); }
        void eqn_stop() override
        {
            _in_math = false;
            out.emit( "\\stopmathalignment\\stopformula\\stopTEXpage</tex>" );
        }

        void math_start() override
        {
            _in_math = true;
            if ( _in_mpost )
                out.emit( "\\math{" );
            else
                out.emit( "<tex>\\startMPpage[instance=mathfun]\npicture p; p := btex \\math{" );
        }

        void math_stop()  override
        {
            _in_math = false;
            if ( _in_mpost )
                out.emit( "}" );
            else
                out.emit( "} etex; write decimal ypart llcorner p to \"yshift.txt\";",
                          " draw p;\n\\stopMPpage</tex>" );
        }

        void mpost_start() override
        {
            out.emit( "<div class=\"center\"><tex>\\startMPpage\n",
                      "color fg; fg := black; picture dotted; dotted := dashpattern( on 1 off 1.5 ); " );
            _in_mpost = true;
        }

        void mpost_stop()  override { out.emit( "\\stopMPpage</tex></div>" ); _in_mpost = false; }
        void mpost_write( std::string_view s ) override { out.emit( s ); }

        void table_start( columns ci, bool even = false ) override
        {
            _table_rule = false;
            _table_rows = _table_cells = 0;
            out.emit( "<table class=\"", even ? "even" : "", "\">\n" );
            auto setup = []( char c )
            {
                switch ( c )
                {
                    case 'l': return "left";
                    case 'c': return "center";
                    case 'r': return "right";
                    case '|': return "vrule center";
                    case ']': return "vrule right";
                    case '[': return "vrule left";
                    default: return "";
                }
            };

            _table_class.clear();
            for ( auto c : ci )
                _table_class.push_back( setup( c ) );
        }

        void table_new_cell( int span ) override
        {
            if ( ++ _table_cells > 1 )
                out.emit( "</td> " );
            out.emit( "<td colspan=\"", span, "\" class=\"", _table_class[ _table_cells - 1 ],
                      _table_rule ? " hrule" : "", "\">" );
        }

        void table_new_row( bool rule = false ) override
        {
            if ( ++ _table_rows > 1 )
                out.emit( "</td></tr>\n" );
            out.emit( "<tr>" );
            _table_rule = rule;
            _table_cells = 0;
        }

        void table_stop() override
        {
            out.emit( "</td></tr></table>\n" );
        }

        std::stack< bool > _li_close;
        std::stack< int > _li_count;

        void list_item()
        {
            if ( _li_close.top() ) out.emit( "</li>" );
            out.emit( "<li>" );
            _li_close.top() = true;
            _li_count.top() ++;
        }

        void list_start()
        {
            _li_close.push( false );
            _li_count.push( 0 );
        }

        void list_stop()
        {
            if ( _li_close.top() ) out.emit( "</li>" );
            _li_close.pop();
            _li_count.pop();
        }

        void enum_start( int, int start ) override
        {
            out.emit( "<ol start=\"", start, "\">" );
            list_start();
        }

        void enum_item()             override { list_item(); }
        void enum_stop( bool )       override { list_stop(); out.emit( "</ol>" ); }
        void bullet_start( int )     override { out.emit( "<ul>" ); list_start(); }
        void bullet_item()           override { list_item(); }
        void bullet_stop( bool )     override { list_stop(); out.emit( "</ul>" ); }

        void code_start( sv t ) override { out.emit( "<pre><code class=\"", t, "\">" ); }
        void code_line( sv l )  override { text( l ); out.emit( "\n" ); }
        void code_stop()        override { out.emit( "</code></pre>\n" ); }
        void quote_start()      override { out.emit( "<blockquote>\n" ); }
        void quote_stop()       override { out.emit( "</blockquote>\n" );  }

        void small_start() override {}
        void small_stop()  override {}

        void paragraph() override { out.emit( "</div><div>\n" ); }
    };

}
