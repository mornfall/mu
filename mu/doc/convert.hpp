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
        convert( std::u32string_view t, doc::writer &w ) : todo( t ), w( w ) {}

        enum list_type { bullets, numbered };
        std::stack< list_type > _list;

        bool in_picture = false;
        bool in_code = false;
        bool in_quote = false;

        void emit_mpost( std::string_view s ) { w.mpost_write( s ); }
        void emit_tex( std::u32string_view s ) { emit_text( s ); }

        char32_t nonwhite();
        void skip_white( std::u32string_view &l );

        int white_count( std::u32string_view v );
        int white_count() { return white_count( todo ); }

        void skip_white() { skip_white( todo ); }
        bool skip( char32_t c );
        void skip_item_lead();

        std::u32string_view fetch_line( std::u32string_view &v );
        std::u32string_view fetch_line() { return fetch_line( todo ); }

        void emit_text( std::u32string_view v );

        void heading();
        void start_list( list_type l );
        void end_list( int count = 1 );
        void ensure_list( int l, list_type t );
        bool try_enum();

        void emit_quote();
        void end_quote();

        auto code_types();
        void emit_code();
        void end_code();

        void try_picture();
        void try_table();

        void run();
    };

}
