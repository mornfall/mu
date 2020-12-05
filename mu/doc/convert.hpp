#pragma once
#include "doc/writer.hpp"
#include "pic/writer.hpp"
#include <string_view>
#include <stack>
#include <cassert>

namespace umd::doc
{

    static inline int stoi( std::u32string_view n )
    {
        std::wstring_convert< std::codecvt_utf8< char32_t >, char32_t > conv;
        return std::stoi( conv.to_bytes( n.begin(), n.end() ) );
    }

    struct convert : pic::writer
    {
        using sv = std::u32string_view;

        doc::writer &w;
        sv todo;
        convert( sv t, doc::writer &w ) : w( w ), todo( t ) {}

        struct list
        {
            enum type { bullets, numbered } t;
            int indent;
            list( type t, int i ) : t( t ), indent( i ) {}
        };

        std::stack< list > _list;
        int rec_list_depth = 0;

        bool in_picture = false;
        bool in_code = false;
        bool in_quote = false;
        int in_math = 0;

        std::u32string default_typing;

        void emit_mpost( std::string_view s ) { w.mpost_write( s ); }
        void emit_tex( std::u32string_view s ) { emit_text( s ); }

        char32_t nonwhite();
        void skip_white( std::u32string_view &l );

        int white_count( std::u32string_view v );
        int white_count() { return white_count( todo ); }

        void skip_white() { skip_white( todo ); }
        bool skip( char32_t c );

        std::pair< int, int > skip_item_lead( list::type );
        int skip_bullet_lead();
        std::pair< int, int > skip_enum_lead();

        template< typename F >
        std::u32string_view fetch( std::u32string_view &v, F pred )
        {
            auto l = v;
            while ( !v.empty() && !pred( v ) )
                v.remove_prefix( 1 );
            if ( !v.empty() )
                v.remove_prefix( pred( v ) );
            return l.substr( 0, l.size() - v.size() - 1 );
        }

        static int count( sv s, char32_t ch )
        {
            int count = 0;
            while ( !s.empty() && s[ 0 ] == ch )
                s.remove_prefix( 1 ), ++ count;
            return count;
        }

        static int newline( sv s )  { return s[ 0 ] == U'\n'; }
        static int space( sv s )    { return s[ 0 ] == U' ' || s[ 0 ] == U'\n'; }
        static int parbreak( sv s ) { int c = count( s, U'\n' ); return c >= 2 ? c : 0; }

        std::u32string_view fetch_line() { return fetch( todo, newline ); }
        std::u32string_view fetch_word() { return fetch( todo, space ); }
        std::u32string_view fetch_par()  { return fetch( todo, parbreak ); }

        void emit_text( std::u32string_view v );
        void emit_footnote( char32_t head );

        void heading();
        void start_list( list::type l, int indent, int first );
        bool end_list( int count = 1, bool xspace = true );
        void ensure_list( int l, list::type t );
        bool try_enum();

        void emit_quote();
        void end_quote();

        auto code_types();
        void emit_code();
        void end_code();

        void try_picture();
        void try_table();
        bool try_dispmath();
        void try_nested();
        void try_directive();

        void recurse( const std::u32string &data );

        void header();
        void body();
        void run();
    };

}
