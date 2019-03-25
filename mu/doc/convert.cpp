#include "pic/scene.hpp"
#include "pic/reader.hpp"
#include "pic/convert.hpp"
#include "doc/writer.hpp"
#include "doc/convert.hpp"

#include <sstream>
#include <codecvt>

namespace umd::doc
{

    char32_t convert::nonwhite()
    {
        auto l = todo;
        skip_white( l );
        return l.empty() ? 0 : l[ 0 ];
    }

    void convert::skip_white( std::u32string_view &l )
    {
        while ( !l.empty() && l[0] != U'\n' && std::isspace( l[ 0 ] ) )
            l.remove_prefix( 1 );
    }

    int convert::white_count( std::u32string_view v )
    {
        auto l = v;
        skip_white( l );
        return v.size() - l.size();
    }

    bool convert::skip( char32_t c )
    {
        if ( todo.empty() ) return false;
        if ( todo[ 0 ] == c )
            return todo.remove_prefix( 1 ), true;
        else
            return false;
    }

    void convert::skip_item_lead()
    {
        if ( todo.empty() )
            return;

        switch ( todo[ 0 ] )
        {
            case U'•': case U'◦': case U'‣':
                todo.remove_prefix( 1 );
                skip_white();
                return;
            default:
                if ( std::isalpha( todo[ 0 ] ) )
                    todo.remove_prefix( 1 );
                else
                    while ( !todo.empty() && std::isdigit( todo[ 0 ] ) )
                        todo.remove_prefix( 1 );
                skip( U'.' );
                skip_white();
        }
    }

    std::u32string_view convert::fetch_line( std::u32string_view &v )
    {
        auto l = v;
        while ( !v.empty() && v[ 0 ] != U'\n' )
            v.remove_prefix( 1 );
        if ( !v.empty() )
            v.remove_prefix( 1 );
        return l.substr( 0, l.size() - v.size() - 1 );
    }

    void convert::heading()
    {
        int level = 0;
        skip_white();
        while ( skip( U'#' ) )
            ++ level;
        skip_white();
        w.heading( fetch_line(), level );
    }

    void convert::end_list( int count )
    {
        if ( _list.empty() || !count )
            return;

        switch ( _list.top() )
        {
            case bullets: w.bullet_stop(); break;
            case numbered: w.enum_stop(); break;
        }

        _list.pop();
        end_list( count - 1 );
    }

    void convert::start_list( list_type l )
    {
        switch ( l )
        {
            case bullets: w.bullet_start( _list.size() ); break;
            case numbered: w.enum_start( _list.size() ); break;
        }

        _list.push( l );
    }

    void convert::ensure_list( int l, list_type t )
    {
        while ( _list.size() > l )
            end_list();

        if ( _list.size() == l - 1 )
            start_list( t );

        assert( _list.size() == l );
        skip_white();
        skip_item_lead();

        switch ( t )
        {
            case bullets: w.bullet_item(); break;
            case numbered: w.enum_item(); break;
        }

        emit_text( fetch_line() ); w.text( U"\n" );
    }

    bool convert::try_enum()
    {
        auto l = todo;
        skip_white( l );

        int digits = 0;
        bool alpha = false;

        while ( !l.empty() && std::isdigit( l[ 0 ] ) )
            l.remove_prefix( 1 ), ++ digits;

        if ( !digits && !l.empty() && std::isalpha( l[ 0 ] ) )
            l.remove_prefix( 1 ), alpha = true;

        if ( l.empty() || l[ 0 ] != U'.' )
            return false;

        if ( digits )
            ensure_list( 1, numbered );
        if ( alpha )
            ensure_list( 2, numbered );

        return true;
    }

    void convert::emit_text( std::u32string_view v )
    {
        int i;
        using wt = doc::writer;
        void (wt::*action)() = nullptr;

        for ( i = 0; i < v.size(); ++i )
        {
            switch ( v[ i ] )
            {
                case U'｢': action = &wt::tt_start; break;
                case U'｣': action = &wt::tt_stop; break;
                case U'‹': action = &wt::em_start; break;
                case U'›': action = &wt::em_stop; break;
                case U'⟦':
                    if ( !in_math )
                        action = &wt::math_start;
                    ++ in_math;
                    break;
                case U'⟧':
                    if ( in_math )
                        action = &wt::math_stop;
                    -- in_math;
                    break;
                default: ;
            }

            if ( action )
            {
                w.text( v.substr( 0, i ) );
                (w.*action)();
                return emit_text( v.substr( i + 1, v.npos ) );
            }
        }
        w.text( v );
    }

    void convert::emit_quote()
    {
        if ( !in_quote )
            w.quote_start(), in_quote = true;
        skip( U'>' );
        emit_text( fetch_line() );
    }

    void convert::end_quote()
    {
        if ( in_quote )
            w.quote_stop(), in_quote = false;
    }

    auto convert::code_types()
    {
        std::map< std::u32string_view, std::string > rv;
        rv[ U"/* C++ */" ] = "cxx";
        rv[ U"# shell" ] = "sh";
        rv[ U"# python" ] = "python";
        return rv;
    }

    void convert::emit_code()
    {
        bool was_in_code = in_code;
        for ( int i = 0; i < 4; ++i )
            skip( ' ' );
        auto l = fetch_line();

        if ( !in_code )
        {
            std::string type;
            for ( auto [ k, v ] : code_types() )
                if ( auto pos = l.find( k ) ; pos != l.npos )
                {
                    type = v;
                    if ( l.size() == pos + k.size() )
                        l = l.substr( 0, pos );
                }
            w.code_start( type );
            in_code = true;
        }

        w.code_line( l );
    }

    void convert::end_code()
    {
        if ( in_code )
            w.code_stop(), in_code = false;
    }

    void convert::try_picture()
    {
        if ( !white_count() )
            return;

        bool special = false;
        int i = 0;

        for ( ; i < todo.size() && todo[ i ] != '\n'; ++i )
            if ( todo[ i ] >= 0x2500 && todo[ i ] < 0x2580 )
                special = true;

        if ( !special ) return;

        for ( ; i < todo.size() - 1 && ( todo[ i ] != '\n' || todo[ i + 1 ] != '\n' ) ; ++ i );

        w.mpost_start();
        auto grid = pic::reader::read_grid( todo.substr( 0, i ) );
        auto scene = pic::convert::scene( grid );
        scene.emit( *this );
        w.mpost_stop();

        todo = todo.substr( i, todo.npos );
    }

    void convert::try_table()
    {
        auto l = todo;
        int wc = white_count( l );
        skip_white( l );
        if ( l.empty() ) return;
        if ( l[ 0 ] != U'│' ) return; /* does not start with the right character */
        fetch_line( l );
        if ( wc != white_count( l ) ) return; /* misaligned → not a table */
        skip_white( l );
        if ( l[ 0 ] != U'├' ) return; /* the separator is wrong */

        /* we are reasonably sure that this is a table now */
        auto hdr = fetch_line();
        auto sep = fetch_line(); skip_white( sep );

        std::u32string txt;
        auto emit_line = [&]( auto l )
        {
            for ( int i = 0; i < l.size(); ++i )
                if ( i && l[ i ] == U'│' )
                    w.table_new_cell(), emit_text( txt ), txt.clear();
                else
                    txt += l[ i ];
        };

        std::vector< char > cols;
        int finished = 0;
        bool rules = false;

        for ( int i = 0; i < sep.size(); ++i )
            switch ( sep[ i ] )
            {
                case U'◀': cols.push_back( 'l' ); break;
                case U'▶':
                    if ( finished != cols.size() && cols.back() == 'l' )
                        cols.back() = 'c';
                    else
                        cols.push_back( 'r' );
                    break;
                case U'─': rules = true; break;
                case U'┄': break;
                default:
                    if ( finished == cols.size() )
                        cols.push_back( 'l' );
                    ++ finished;
            }

        w.table_start( cols, rules );
        emit_line( hdr );
        w.table_new_row();

        while ( !todo.empty() && nonwhite() == U'│' )
            emit_line( fetch_line() ), w.table_new_row();
        w.table_stop();
    }

    void convert::run()
    {
        try_table();
        try_picture();

        if ( todo.empty() ) return;

        if ( todo[ 0 ] == U'' )
            return w.pagebreak(), fetch_line(), run();

        if ( todo[ 0 ] == U'>' )
            return emit_quote(), run();
        else
            end_quote();

        if ( _list.empty() && white_count() >= 4 )
            return emit_code(), run();
        else
            end_code();

        switch ( char32_t c = nonwhite() )
        {
            case 0: return;
            case U'•': ensure_list( 1, bullets ); break;
            case U'◦': ensure_list( 2, bullets ); break;
            case U'‣': ensure_list( 3, bullets ); break;
            case U'#': heading(); break;

            default:
                if ( !try_enum() )
                {
                    if ( white_count() < 3 )
                        end_list( -1 );
                    emit_text( fetch_line() );
                    w.text( U"\n" );
                }
        }

        run();
    }

}
