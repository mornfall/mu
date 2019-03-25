#pragma once
#include <vector>
#include <codecvt> // codecvt_utf8
#include <locale>  // wstring_convert

namespace umd::doc
{

    struct stream
    {
        std::ostream &ostr;
        stream( std::ostream &o ) : ostr( o ) {}

        void emit() {}

        template< typename T, typename... Ts >
        void emit( const T &t, const Ts & ... ts );

        template< typename... Ts >
        void emit( const std::u32string_view &u, const Ts & ... ts )
        {
            std::wstring_convert< std::codecvt_utf8< char32_t >, char32_t > conv;
            ostr << conv.to_bytes( u.begin(), u.end() );
            emit( ts... );
        }

        template< typename... Ts >
        void emit( const std::u32string &u, const Ts & ... ts )
        {
            emit( std::u32string_view( u ), ts... );
        }

    };

    template< typename SV, typename C, typename S >
    void process( SV string, C char_cb, S seg_cb )
    {
        int start = 0, end;
        auto flush = [&]( int n = 0 )
        {
            seg_cb( string.substr( start, end - start - n ) );
            start = end + 1 - n;
        };

        for ( end = 0; end < string.size(); ++end )
            char_cb( flush, string[ end ] );

        if ( end > start )
            seg_cb( string.substr( start, end ) );
    }

    template< typename T, typename... Ts >
    void stream::emit( const T &t, const Ts & ... ts )
    {
        ostr << t;
        emit( ts... );
    }

    struct writer
    {
        using columns = std::vector< char >;

        virtual void text( std::u32string_view ) = 0;
        virtual void heading( std::u32string_view txt, int level ) = 0;

        /* spans ; may be also called within mpost btex/etex */
        virtual void em_start() {}
        virtual void em_stop() {}
        virtual void tt_start() {}
        virtual void tt_stop() {}
        virtual void math_start() = 0;
        virtual void math_stop() = 0;

        /* lists */
        virtual void enum_start( int level ) = 0;
        virtual void enum_item() = 0;
        virtual void enum_stop() = 0;
        virtual void bullet_start( int level ) = 0;
        virtual void bullet_item() = 0;
        virtual void bullet_stop() = 0;

        /* metapost figures */
        virtual void mpost_start() = 0;
        virtual void mpost_write( std::string_view ) = 0;
        virtual void mpost_stop() = 0;

        /* tables */
        virtual void table_start( columns, bool ) = 0;
        virtual void table_new_cell() = 0;
        virtual void table_new_row() = 0;
        virtual void table_stop() = 0;

        /* blocks */
        virtual void code_start( std::string type ) = 0;
        virtual void code_line( std::u32string_view ) = 0;
        virtual void code_stop() = 0;
        virtual void quote_start() {}
        virtual void quote_stop() {}

        /* paging */
        virtual void pagebreak() {}
    };

}
