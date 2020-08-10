#pragma once
#include "writer.hpp"
#include <vector>
#include <sstream>
#include <iostream>
#include <map>

/* Base class for TeX-based writers. */

namespace umd::doc
{

    struct w_html : w_noop // writer
    {
        stream &out;
        w_html( stream &out ) : out( out )
        {
            _sections.resize( 7, 0 );
            _section_num.resize( 7 );
        }

        bool _in_math = false;
        std::map< sv, sv > _meta;

        std::vector< int > _sections;
        std::vector< sv > _section_num;

        int _heading = 0; // currently open <hN> tag (must not be nested)

        void meta( sv key, sv value ) override { _meta[ key ] = value; }
        void meta_end() override
        {
            out.emit( "<!DOCTYPE html>" );
            out.emit( "<html><head>" );
            out.emit( "<meta charset=\"UTF-8\">" );
            out.emit( "<title>", _meta[ U"title" ], "</title>" );
            out.emit( "<link rel=\"stylesheet\" href=\"", _meta[ U"doctype" ] ,".css\">" );
            out.emit( "<script src=\"highlight.min.js\"></script>" );
            out.emit( "<script>hljs.initHighlightingOnLoad();</script>" );
            out.emit( "</head><body>" );
        }

        void end() override { out.emit( "</body></html>" ); }

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
        void em_start()   override { out.emit( "<em>" ); }
        void em_stop()    override { out.emit( "</em>" ); }
        void tt_start()   override { out.emit( "<code>" ); }
        void tt_stop()    override { out.emit( "</code>" ); }
        void math_start() override { out.emit( "(math)" ); _in_math = true; }
        void math_stop()  override { out.emit( "(/math)" ); _in_math = false; }

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
    };

}
