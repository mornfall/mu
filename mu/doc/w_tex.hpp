#pragma once
#include "writer.hpp"
#include <vector>
#include <sstream>
#include <iostream>

/* Base class for TeX-based writers. */

namespace umd::doc
{

    struct w_tex : writer
    {
        stream &out;
        w_tex( stream &out ) : out( out ) {}

        bool in_math = false;
        sv format;

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

            out.emit( "\\def\\mm", key, "{", value, "}\n" );
        }

        virtual void meta_end()
        {
            if ( !format.empty() )
                out.emit( "\\input{prelude-", format, ".tex}\n" );
        }

        virtual void text( std::u32string_view t )
        {
            bool in_index = false;
            sv indices = U"₁₂₃₄₅₆₇₈₉ᵢ₌",
                 dices = U"123456789i=";

            auto char_cb = [&]( auto flush, char32_t c )
            {
                auto idx = indices.find( c );

                if ( in_math )
                {
                    if ( !in_index && idx != indices.npos )
                        flush(), out.emit( "_{" ), in_index = true;

                    if ( in_index )
                        flush( 1 );

                    if ( in_index && idx != indices.npos )
                        out.emit( dices.substr( idx, 1 ) );

                    if ( in_index && idx == indices.npos )
                        out.emit( "}" );

                    if ( idx == indices.npos )
                        in_index = false;
                }

                switch ( c )
                {
                    case 0x0307:
                        if ( in_math )
                            flush( 2 ), out.emit( "\\dot " ), flush();
                        break;
                    case U'&': flush(); out.emit( "\\&" ); break;
                    case U'%': flush(); out.emit( "\\%" ); break;
                    case U'$': flush(); out.emit( "\\$" ); break;
                    case U'#': flush(); out.emit( "\\#" ); break;
                    case U'|': flush(); out.emit( "\\|" ); break;
                    case U'_': if ( !in_math ) flush(), out.emit( "\\_" ); break;
                    case U'{': if ( !in_math ) flush(), out.emit( "\\{" ); break;
                    case U'}': if ( !in_math ) flush(), out.emit( "\\}" ); break;
                    case U'~': flush(); out.emit( "\\textasciitilde{}" ); break;
                    case U'^': if ( !in_math ) flush(), out.emit( "\\textasciicircum{}" ); break;
                    case U'\\': if ( !in_math ) flush(), out.emit( "\\textbackslash{}" ); break;
                    default: ;
                }
            };

            process( t, char_cb, [&]( auto s ) { out.emit( s ); } );
            if ( in_index ) out.emit( "}" );
        }

        /* spans ; may be also called within mpost btex/etex */
        virtual void em_start() { out.emit( "{\\em{}" ); }
        virtual void em_stop()  { out.emit( "}" ); }
        virtual void tt_start() { out.emit( "{\\code{}" ); }
        virtual void tt_stop()  { out.emit( "}" ); }
        virtual void math_start() { out.emit( "\\math{" ); in_math = true; }
        virtual void math_stop() { out.emit( "}" ); in_math = false; }

        virtual void enum_item() { out.emit( "\\item " ); }
        virtual void bullet_item() { out.emit( "\\item " ); }

        virtual void code_line( std::u32string_view l ) { out.emit( l, "\n" ); }
    };

}
