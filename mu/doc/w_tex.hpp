#pragma once
#include "writer.hpp"
#include <vector>
#include <sstream>
#include <cassert>

/* Base class for TeX-based writers. */

namespace umd::doc
{

    struct w_tex : w_noop
    {
        stream &out;
        w_tex( stream &out ) : out( out ) {}

        bool _in_math = false;
        sv format, doctype;

        std::string heading_cmd( int l )
        {
            std::stringstream s;
            while ( l > 1 )
                --l, s << "sub";
            s << "section";
            return s.str();
        }

        virtual void meta( sv key, sv value )
        {
            if ( key == U"format" )
                format = value;
            if ( key == U"doctype" )
                doctype = value;

            out.emit( "\\def\\mm", key, "{", value, "}\n" );
        }

        virtual void meta_end()
        {
            if ( format.empty() )
                format = doctype;

            out.emit( "\\input{prelude-", format, ".tex}\n" );
        }

        virtual void text( std::u32string_view t )
        {
            bool in_sub = false, in_sup = false;
            sv sub = U"₀₁₂₃₄₅₆₇₈₉________ᵢⱼₖ_ₘₙ___________₌₋₊",
               sup = U"⁰¹²³⁴⁵⁶⁷⁸⁹ᵃᵇᶜᵈᵉᶠᵍʰⁱʲᵏˡᵐⁿᵒᵖʳˢᵗᵘᵛʷˣʸᶻ⁼⁻⁺",
               nom = U"0123456789abcdefghijklmnoprstuvwxyz=-+";

            auto char_cb = [&]( auto flush, char32_t c )
            {
                auto subi = c == U'_' ? sub.npos : sub.find( c );
                auto supi = sup.find( c );

                if ( ( in_sup && supi == sup.npos ) || ( in_sub && subi == sub.npos ) )
                    flush( 1 ), out.emit( _in_math ? "}" : "}$" );

                if ( !in_sub && subi != sub.npos )
                    flush(), out.emit( _in_math ? "_{" : "$_{" ), in_sub = true;

                if ( !in_sup && supi != sup.npos )
                    flush(), out.emit( _in_math ? "^{" : "$^{" ), in_sup = true;

                if ( in_sub || in_sup )
                    flush( 1 );

                if ( ( in_sub && subi != sub.npos ) || ( in_sup && supi != sup.npos ) )
                    out.emit( nom.substr( in_sup ? supi : subi, 1 ) );

                if ( subi == sub.npos ) in_sub = false;
                if ( supi == sup.npos ) in_sup = false;

                switch ( c )
                {
                    case 0x0307:
                        if ( _in_math )
                            flush( 2 ), out.emit( "\\dot " ), flush();
                        break;
                    case U'&': flush(); out.emit( "\\&" ); break;
                    case U'%': flush(); out.emit( "\\%" ); break;
                    case U'$': flush(); out.emit( "\\$" ); break;
                    case U'#': flush(); out.emit( "\\#" ); break;
                    case U'|': flush(); out.emit( "\\|" ); break;
                    case U'_': if ( !_in_math ) flush(), out.emit( "\\_" ); break;
                    case U'[': if ( !_in_math ) flush(), out.emit( "\\[" ); break;
                    case U']': if ( !_in_math ) flush(), out.emit( "\\]" ); break;
                    case U'{':
                        flush();
                        if ( _in_math )
                            out.emit( "\\left\\{" );
                        else
                            out.emit( "\\textbraceleft{}");
                        break;
                    case U'}':
                        flush();
                        if ( _in_math )
                            out.emit( "\\right\\}" );
                        else
                            out.emit( "\\textbraceright{}" );
                        break;
                    case U'~': flush(); out.emit( "\\textasciitilde{}" ); break;
                    case U'^': if ( !_in_math ) flush(), out.emit( "\\textasciicircum{}" ); break;
                    case U'\\': if ( !_in_math ) flush(), out.emit( "\\textbackslash{}" ); break;
                    default: ;
                }
            };

            process( t, char_cb, [&]( auto s )
            {
                if ( in_sub || in_sup )
                {
                    assert( s.size() == 1 );
                    out.emit( _in_math ? "}" : "}$" );
                    in_sub = in_sup = false;
                }
                else
                    out.emit( s );
            } );

            if ( in_sub || in_sup )
                out.emit( _in_math ? "}" : "}$" );
        }

        /* spans ; may be also called within mpost btex/etex */
        virtual void em_start() { out.emit( "{\\em{}" ); }
        virtual void em_stop()  { out.emit( "}" ); }
        virtual void tt_start() { out.emit( "\\code{" ); }
        virtual void tt_stop()  { out.emit( "}" ); }
        virtual void bf_start() { out.emit( "{\\bf\\realbf{}" ); }
        virtual void bf_stop()  { out.emit( "}" ); }

        virtual void small_start() { out.emit( "{\\ss{}\\smaller" ); }
        virtual void small_stop() { out.emit( "}" ); }
        virtual void math_start() { out.emit( "\\math{" ); _in_math = true; }
        virtual void math_stop() { out.emit( "}" ); _in_math = false; }

        virtual void enum_item() { out.emit( "\\item{}" ); }
        virtual void bullet_item() { out.emit( "\\item{}" ); }

        virtual void code_line( std::u32string_view l ) { out.emit( l, "\n" ); }
        virtual void paragraph() { out.emit( "\n" ); }
        virtual void tab() { out.emit( "\\strut\\hfill\\strut{}" ); }
    };

}
