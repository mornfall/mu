#pragma once
#include "writer.hpp"
#include "w_tex.hpp"
#include "util.hpp"
#include <vector>
#include <sstream>
#include <iostream>
#include <map>
#include <utility>
#include <brick-string>

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

        bool _in_foothead = false, _in_footnote = false;
        std::u32string _foothead, _footnote;

        int _last_footnote_num = 0;
        std::vector< std::pair< int, std::u32string > > _outstanding_footnotes;

        bool _in_div = false;
        bool _table_rule = false;
        int _table_rows, _table_cells;
        std::vector< std::string > _table_class;

        bool _in_mpost = false;
        int _heading = 0; // currently open <hN> tag (must not be nested)

        sv fgcolor() const
        {
            if ( auto i = _meta.find( U"fgcolor" ); i != _meta.end() )
                return i->second;
            else
                return U"black";
        }

        void meta( sv key, sv value ) override { _meta[ key ] = value; }
        void meta_end() override
        {
            if ( _meta[ U"naked" ] == U"yes" )
                return;

            out.emit( "<!DOCTYPE html>" );
            out.emit( "<html lang=\"", _meta[ U"lang" ], "\"><head>" );
            out.emit( "<meta charset=\"UTF-8\"><!-- ‹really› -->" );
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

        void html( sv raw ) override
        {
            if ( _in_foothead )
                _foothead += raw;
            else if ( _in_footnote )
                _footnote += raw;
            else
                out.emit( raw );
        }

        void end() override
        {
            place_footnotes();

            if ( _meta[ U"naked" ] != U"yes" )
                out.emit( "</div></body></html>" );
        }

        void text( std::u32string_view t ) override { return text( t, true ); }

        void text( std::u32string_view t, bool allow_div )
        {
            auto char_cb = [&]( auto flush, char32_t c )
            {
                switch ( c )
                {
                    case 0x0307:
                        if ( _in_math )
                            flush( 2 ), html( U"\\dot " ), flush();
                        break;
                    case U'&': flush(); html( U"&amp;" ); break;
                    case U'<': flush(); html( U"&lt;" ); break;
                    case U'>': flush(); html( U"&gt;" ); break;
                    default: ;
                }
            };

            if ( !_in_footnote && !_in_foothead && ( _in_math || _in_mpost ) )
                w_tex::text( t );
            else
            {
                if ( allow_div )
                    ensure_div();

                process( t, char_cb, [&]( auto s ) { html( s ); } );
            }
        }

        void heading_start( int level, std::u32string_view num ) override
        {
            place_footnotes();

            paragraph();
            _heading = level;
            out.emit( "<h", level, ">" );

            for ( int i = 1; i < level; ++i )
                if ( _section_num[ i ].empty() )
                    out.emit( _sections[ i ], "." );
                else
                    out.emit( _section_num[ i ], "." );

            if ( num.empty() )
            {
                if ( _meta[ U"toc" ] == U"yes" )
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
            out.emit( "</h", _heading, "> " );
            _heading = 0;
        }

        /* spans ; may be also called within mpost btex/etex */
        void em_start()   override { html( _in_mpost ? U"{\\em{}" : U"<em>" ); }
        void em_stop()    override { html( _in_mpost ? U"}" : U"</em>" ); }
        void tt_start()   override { html( _in_mpost ? U"{\\tt{}" : U"<code>" ); }
        void tt_stop()    override { html( _in_mpost ? U"}" : U"</code>" ); }

        /* generate svgtex-compatible markup */
        void eqn_start( int n, std::string ) override
        {
            _in_math = true;

            out.emit( "<tex>\\startTEXpage\\startcolor[", fgcolor(),"]"
                      "\\startformula[packed]\\startmathalignment[n=", n, "]" );
        }

        void eqn_new_cell() override { out.emit( "\\NC " ); }
        void eqn_new_row() override { out.emit( "\\NR " ); }
        void eqn_stop() override
        {
            _in_math = false;
            out.emit( "\\stopmathalignment\\stopformula\\stopcolor\\stopTEXpage</tex>" );
        }

        void math_start() override
        {
            _in_math = true;

            if ( _in_mpost )
                html( U"\\math{" );
            else
            {
                html( U"<tex>\\setupMPinstance[mathfun][textcolor=" );
                html( fgcolor() );
                html( U"]\n\\startMPpage[instance=mathfun]\npicture p; p := btex \\math{" );
            }
        }

        void math_stop()  override
        {
            _in_math = false;
            if ( _in_mpost )
                html( U"}" );
            else
                html( U"} etex; write decimal ypart llcorner p to \"yshift.txt\";"
                      " draw p;\n\\stopMPpage</tex>" );
        }

        void mpost_start() override
        {
            html( U"<div class=\"center\"><tex>\\setupMPinstance[metafun][textcolor=" );
            html( fgcolor() );
            html( U"]\\startMPpage\ncolor fg; fg := " );
            html( fgcolor() );
            html( U"; picture dotted; dotted := dashpattern( on 1 off 1.5 ); " );

            _in_mpost = true;
        }

        void mpost_stop()  override { html( U"\\stopMPpage</tex></div>" ); _in_mpost = false; }
        void mpost_write( std::string_view s ) override { html( from_utf8( s ) ); }

        void table_start( columns ci, bool even = false ) override
        {
            _table_rule = false;
            _table_rows = _table_cells = 0;
            ensure_div();
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
            if ( _li_close.top() ) html( U"</li>" );
            html( U"<li>" );
            _li_close.top() = true;
            _li_count.top() ++;
        }

        void list_start()
        {
            ensure_div();
            _li_close.push( false );
            _li_count.push( 0 );
        }

        void list_stop()
        {
            if ( _li_close.top() ) html( U"</li>" );
            _li_close.pop();
            _li_count.pop();
        }

        void enum_start( int, int start ) override
        {
            ensure_div();
            out.emit( "<ol start=\"", start, "\">" );
            list_start();
        }

        void enum_item()             override { list_item(); }
        void enum_stop( bool )       override { list_stop(); out.emit( "</ol>" ); }

        void bullet_start( int )     override { ensure_div(); html( U"<ul>" ); list_start(); }
        void bullet_item()           override { list_item(); }
        void bullet_stop( bool )     override { list_stop(); html( U"</ul>" ); }

        void code_start( sv t ) override { out.emit( "<pre><code class=\"", t, "\">" ); }
        void code_line( sv l )  override { text( l, false ); out.emit( "\n" ); }
        void code_stop()        override { out.emit( "</code></pre>\n" ); }
        void quote_start()      override { out.emit( "<blockquote>\n" ); }
        void quote_stop()       override { out.emit( "</blockquote>\n" ); }

        void small_start() override {}
        void small_stop()  override {}

        void footnote_head() override { _in_foothead = true; _foothead.clear(); }
        void footnote_start() override
        {
            _in_foothead = false;
            _in_footnote = true;
            _footnote.clear();
        }

        void linebreak() override { html( U"<br>" ); }

        void footnote_stop() override
        {
            auto get_url = []( sv s )
            {
                if ( !starts_with( s, U"<code>" ) || !brq::ends_with( s, U"</code>" ) )
                    return sv();

                s.remove_prefix( 6 );
                s.remove_suffix( 7 );

                if ( starts_with( s, U"http://"  ) ||
                     starts_with( s, U"https://" ) ||
                     starts_with( s, U"./"       ) )
                    return s;
                else
                    return sv();
            };

            ensure_div();

            if ( auto url = get_url( _footnote ); !url.empty() )
            {
                out.emit( "<a href=\"" );
                out.emit( url );
                out.emit( "\">" );
                out.emit( _foothead );
                out.emit( "</a>" );
            }
            else
            {
                // FIXME When we encounter footnote superscript without any
                // accompanying emphasis block, some of the preceding text might
                // have been assumed to be foothead.
                out.emit( _foothead );

                int num  = ++_last_footnote_num;

                footnote_anchor( "foothead", "footnote", num );
                _outstanding_footnotes.emplace_back( num, _footnote );
            }

            _in_footnote = false;
        }

        void footnote_anchor( std::string_view id, std::string_view href, int num )
        {
            out.emit( "<a class=\"anchor\" id=\"" );
            out.emit( id );
            out.emit( num );
            out.emit( "\" href=\"#" );
            out.emit( href );
            out.emit( num );
            out.emit( "\"><sup>" );
            out.emit( num );
            out.emit( "</sup></a>" );
        }

        void place_footnotes()
        {
            if ( _outstanding_footnotes.empty() )
                return;

            paragraph();
            out.emit( "<div class=\"par footnotes\">" );

            for ( const auto &[ num, footnote ] : _outstanding_footnotes )
            {
                footnote_anchor( "footnote", "foothead", num );

                ensure_div();
                out.emit( " " );
                out.emit( footnote );
                paragraph();
            }

            out.emit( "</div>" );

            _outstanding_footnotes.clear();
        }

        void paragraph() override
        {
            html( U"<!-- paragraph -->\n" );
            if ( _in_div )
                html( U"</div>\n" );
            _in_div = false;
        }

        void ensure_div()
        {
            if ( !_in_div && !_heading && !_in_mpost )
            {
                html( U"<div class=\"par\">" );
                _in_div = true;
            }
        }
    };

}
