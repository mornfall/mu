#pragma once
#include "doc/writer.hpp"
#include "pic/writer.hpp"
#include <string_view>
#include <stack>

namespace umd::doc
{

    struct convert : pic::writer
    {
        doc::writer &w;
        std::u32string_view todo;
        convert( std::u32string_view t, doc::writer &w ) : w( w ), todo( t ) {}

        enum list_type { bullets, numbered };
        std::stack< list_type > _list;

        bool in_picture = false;
        bool in_code = false;
        bool in_quote = false;
        int in_math = 0;

        void emit_mpost( std::string_view s ) { w.mpost_write( s ); }
        void emit_tex( std::u32string_view s ) { emit_text( s ); }

        char32_t nonwhite();
        void skip_white( std::u32string_view &l );

        int white_count( std::u32string_view v );
        int white_count() { return white_count( todo ); }

        void skip_white() { skip_white( todo ); }
        bool skip( char32_t c );
        void skip_item_lead();

        template< typename F >
        std::u32string_view fetch( std::u32string_view &v, F pred )
        {
            auto l = v;
            while ( !v.empty() && !pred( v[ 0 ] ) )
                v.remove_prefix( 1 );
            if ( !v.empty() )
                v.remove_prefix( 1 );
            return l.substr( 0, l.size() - v.size() - 1 );
        }

        static bool newline( char32_t c ) { return c == U'\n'; }
        static bool space( char32_t c )   { return std::isspace( c ); }

        std::u32string_view fetch_line() { return fetch( todo, newline ); }
        std::u32string_view fetch_word() { return fetch( todo, space ); }

        void emit_text( std::u32string_view v );

        void heading();
        void start_list( list_type l );
        bool end_list( int count = 1, bool xspace = true );
        void ensure_list( int l, list_type t );
        bool try_enum();

        void emit_quote();
        void end_quote();

        auto code_types();
        void emit_code();
        void end_code();

        void try_picture();
        void try_table();
        void try_dispmath();

        void header();
        void body();
        void run();
    };

}
