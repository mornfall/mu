#include "pic/scene.hpp"
#include "pic/reader.hpp"
#include "pic/convert.hpp"
#include "doc/writer.hpp"
#include "doc/convert.hpp"
#include "doc/util.hpp"

#include <set>
#include <sstream>
#include <codecvt>
#include <unicode/uchar.h>

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
        while ( !l.empty() && l[0] != U'\n' && space( l ) )
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

    int convert::skip_bullet_lead()
    {
        int indent = 0;
        bool saw_leader = false;

        while ( !todo.empty() )
        {
            auto ch = todo[ 0 ];
            if ( ch != U' ' && saw_leader ) return indent;

            ++ indent;
            todo.remove_prefix( 1 );

            if ( ch == U'•' || ch == U'◦' || ch == U'‣' )
                saw_leader = true;
        }

        return indent;
    }

    std::pair< int, int > convert::skip_enum_lead()
    {
        int indent = 0;
        int leader = 0;
        std::u32string lstr;

        auto first = [&]()
        {
            if ( lstr.empty() ) return 0;
            if ( std::isdigit( lstr[ 0 ] ) ) return stoi( lstr );
            if ( std::isalpha( lstr[ 0 ] ) ) return int( lstr[ 0 ] - U'a' + 1 );
            return int( lstr[ 0 ] - U'α' + 1 );
        };

        while ( !todo.empty() )
        {
            auto ch = todo[ 0 ];

            if ( ch != U' ' && leader == 2 ) return { indent, first() };

            ++ indent;
            todo.remove_prefix( 1 ); /* commit to eating the char */

            if ( ch == U' ' ) continue;
            if ( ch == U'.' && leader == 1 ) leader = 2; /* go on to eat whitespace */

            if ( std::isalnum( ch ) && leader < 2 )
            {
                lstr += ch;
                leader = 1;
            }
        }

        return { indent, first() };
    }

    std::pair< int, int > convert::skip_item_lead( list::type t )
    {
        switch ( t )
        {
            case list::bullets:  return { skip_bullet_lead(), 1 };
            case list::numbered: return skip_enum_lead();
        }
        __builtin_trap();
    }

    void convert::heading()
    {
        end_list( -1, false );
        int level = 0;
        skip_white();
        while ( skip( U'#' ) )
            ++ level;
        skip_white();
        w.heading( fetch_line(), level );
    }

    bool convert::end_list( int count, bool xspace )
    {
        if ( _list.empty() || !count )
            return false;

        auto type = _list.top().t;
        _list.pop();

        switch ( type )
        {
            case list::bullets: w.bullet_stop( _list.empty() ? xspace : false ); break;
            case list::numbered: w.enum_stop( _list.empty() ? xspace : false ); break;
        }

        end_list( count - 1, xspace );
        return true;
    }

    void convert::start_list( list::type l, int indent, int first )
    {
        switch ( l )
        {
            case list::bullets:  w.bullet_start( _list.size() ); break;
            case list::numbered: w.enum_start( _list.size(), first ); break;
        }

        _list.emplace( l, indent );
    }

    void convert::ensure_list( int l, list::type t )
    {
        while ( int( _list.size() ) > l )
            end_list();

        auto [ indent, first ] = skip_item_lead( t );

        if ( int( _list.size() ) == l - 1 )
            start_list( t, indent, first );
        else
            indent = _list.top().indent;

        assert( int( _list.size() ) == l );

        switch ( t )
        {
            case list::bullets:  w.bullet_item(); break;
            case list::numbered: w.enum_item(); break;
        }

        std::u32string buf{ fetch_line() };
        buf += U'\n';

        while ( !todo.empty() )
        {
            if ( white_count() < indent )
                break;

            auto l = fetch_line();
            l.remove_prefix( indent );
            buf += l;
            buf += U"\n";
        }

        recurse( buf );
    }

    bool convert::try_enum()
    {
        auto l = todo;
        skip_white( l );

        int digits = 0;
        bool alpha = false;

        while ( !l.empty() && std::isdigit( l[ 0 ] ) )
            l.remove_prefix( 1 ), ++ digits;

        if ( !digits && !l.empty() && std::islower( l[ 0 ] ) )
            l.remove_prefix( 1 ), alpha = true;

        if ( l.size() < 2 || l[ 0 ] != U'.' || l[ 1 ] != U' ' )
            return false;

        if ( digits )
            ensure_list( 1, list::numbered );
        if ( alpha )
            ensure_list( 2, list::numbered );

        return digits || alpha;
    }

    void convert::emit_text( std::u32string_view v )
    {
        sv sup = U"¹²³⁴⁵⁶⁷⁸⁹";
        auto char_cb = [&]( auto flush, char32_t c )
        {
            switch ( c )
            {
                case U'‹': if ( !in_math ) flush(), w.tt_start(); break;
                case U'›': if ( !in_math ) flush(), w.tt_stop(); break;
                case U'«': flush(); w.em_start(); break;
                case U'»': flush(); w.em_stop(); break;
                case U'❮': flush(); w.bf_start(); break;
                case U'❯': flush(); w.bf_stop(); break;
                case U'⟦':
                    if ( !in_math )
                        flush(), w.math_start();
                    ++ in_math;
                    break;
                case U'⟧':
                    if ( in_math )
                        flush(), w.math_stop();
                    -- in_math;
                    break;
                case 0x2028:
                    flush(); w.linebreak(); break;
                default:
                    if ( !in_math && sup.find( c ) != sup.npos )
                        flush(), emit_footnote( c );
            }
        };

        process( v, char_cb, [&]( auto s ) { w.text( s ); } );
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
        std::map< std::u32string_view, std::u32string > rv;
        rv[ U"/* C++ */" ] = U"cxx";
        rv[ U"/* C */" ]   = U"cxx"; /* TODO make a separate C syntax file */
        rv[ U"# shell" ]   = U"shell";
        rv[ U"# python" ]  = U"python";
        return rv;
    }

    void convert::emit_code()
    {
        for ( int i = 0; i < 4; ++i )
            skip( ' ' );
        auto l = fetch_line();

        if ( !in_code && !l.empty() )
        {
            std::u32string type;
            for ( auto [ k, v ] : code_types() )
                if ( auto pos = l.find( k ) ; pos != l.npos )
                {
                    type = v;
                    if ( l.size() == pos + k.size() )
                        l = l.substr( 0, pos );
                }
            w.code_start( type.empty() ? default_typing : type );
            in_code = true;
        }

        w.code_line( l );
    }

    void convert::end_code()
    {
        if ( in_code )
            w.code_stop(), in_code = false;
    }

    /* scan ahead: quadratic in the number of footnotes, not a
     * bottleneck for now */
    void convert::emit_footnote( char32_t head )
    {
        auto backup = todo;
        while ( !todo.empty() )
        {
            auto par = fetch_par();
            if ( par[ 0 ] == head )
            {
                w.footnote_start();
                emit_text( par.substr( 1 ) );
                w.footnote_stop();
                break;
            }
        }
        todo = backup;
    }

    void convert::try_picture()
    {
        if ( !white_count() )
            return;

        bool special = false;
        int i = 0;

        for ( ; i < int( todo.size() ) && todo[ i ] != '\n'; ++i )
            if ( ( todo[ i ] >= 0x2500 && todo[ i ] < 0x2580 ) ||
                 todo[ i ] == U'●' )
                special = true;

        if ( !special ) return;

        for ( ; i < int( todo.size() ) - 1 && ( todo[ i ] != '\n' || todo[ i + 1 ] != '\n' ) ; ++ i );

        w.mpost_start();
        auto grid = pic::reader::read_grid( todo.substr( 0, i ) );
        auto scene = pic::convert::scene( grid );
        scene.emit( *this );
        w.mpost_stop();

        todo = todo.substr( i, todo.npos );
    }

    bool convert::try_dispmath()
    {
        auto for_index = []( auto l, int w, auto f )
        {
            int idx = 0;
            for ( int i = 0; i < int( l.size() ); ++i )
            {
                if ( !u_getCombiningClass( l[ i ] ) ) ++idx;
                f( idx, l.substr( i, i + w ) );
            }
        };

        auto substr = []( auto l, size_t from, size_t to )
        {
            size_t f = 0, t = l.npos, idx = 0;
            for ( int i = 0; i < int( l.size() ); ++i )
            {
                if ( !u_getCombiningClass( l[ i ] ) ) ++idx;
                if ( idx == from ) f = i;
                if ( idx == to ) t = i;
            }
            return l.substr( f, t - f );
        };

        if ( white_count() <= 3 && nonwhite() == U'⟦' )
        {
            std::vector< std::u32string_view > lines;
            int offset = white_count() + 1;

            auto backup = todo;
            do
            {
                lines.push_back( fetch_line() );
                lines.back().remove_prefix( offset );
            }
            while ( white_count() >= offset );

            if ( lines.empty() || lines.back().empty() || lines.back().back() != U'⟧' )
            {
                todo = backup;
                return false;
            }

            lines.back().remove_suffix( 1 );

            std::set< int > align;

            auto align_on = [&]( std::u32string_view find )
            {
                std::set< int > a;

                auto per_line = [&]( auto f )
                {
                    for ( auto l : lines )
                        for_index( l, 3, f );
                };

                auto match = [&]( auto w )
                {
                    return w[ 0 ] == U' ' && w[ 2 ] == U' ' && find.find( w[ 1 ] ) != find.npos;
                };

                per_line( [&]( int idx, auto w ) { if (  match( w ) ) a.insert( idx + 1 ); } );
                per_line( [&]( int idx, auto w ) { if ( !match( w ) ) a.erase( idx + 1 ); } );
                for ( int i : a )
                    align.insert( i );
            };

            if ( lines.size() > 1 )
            {
                align_on( U"=" );
                align_on( U"→←" );
                align_on( U"+-⋅/" );
            }

            std::string astr;

            for ( unsigned i = 0; i < align.size(); ++ i )
                astr += "lc";

            w.eqn_start( 2 * align.size() + 1, astr );

            for ( auto l : lines )
            {
                int last = 0;
                for ( int i : align )
                    if ( i < int( l.size() ) )
                    {
                        w.eqn_new_cell();
                        w.text( substr( l, last, i ) );
                        w.eqn_new_cell();
                        w.text( substr( l, i, i + 1 ) );
                        last = i + 1;
                    }
                w.eqn_new_cell();
                w.text( substr( l, last, l.npos ) );
                w.eqn_new_row();
            }
            w.eqn_stop();
            return true;
        }

        return false;
    }

    void convert::try_table()
    {
        auto backup = todo;
        auto revert = [&]{ todo = backup; };

        std::vector< sv > lines;
        sv sep;
        int wc = white_count();
        int sep_line = 0;

        while ( white_count() == wc )
        {
            skip_white();
            if ( todo[ 0 ] == U'├' )
                sep = fetch_line(), sep_line = lines.size();
            else if ( todo[ 0 ] == U'│' )
                lines.push_back( fetch_line() );
            else
                break;
        }

        if ( sep.empty() || lines.empty() )
            return revert();

        bool rule = false, hdr_rule = false, even = false, small = false;

        std::deque< int > colw;
        std::u32string txt;

        auto emit_line = [&]( auto l )
        {
            auto remains = colw;
            int span = 1;

            bool draw_rule = rule;

            rule = true;
            for ( auto c : l )
                if ( c != U'│' && c != U'┄' )
                    rule = false;

            if ( rule ) return;

            w.table_new_row( draw_rule );

            for ( int i = 1; i < int( l.size() ); ++i )
            {
                if ( l[ i ] == U'│' )
                {
                    w.table_new_cell( span );
                    emit_text( txt );
                    txt.clear();
                    span = 1;
                    remains.pop_front();
                }
                else
                {
                    txt += l[ i ];
                    if ( --remains.front() == 0 )
                        remains.pop_front(), ++ span;
                }
            }
        };

        std::vector< char > cols;
        int finished = 0;
        int width = 0;
        bool next_rule = false;

        for ( int i = 1; i < int( sep.size() ); ++i )
        {
            ++ width;

            switch ( sep[ i ] )
            {
                case U'◀': case U'◅': cols.push_back( 'l' ); break;
                case U'▶': case U'▻':
                    if ( finished != int( cols.size() ) && cols.back() == 'l' )
                        cols.back() = next_rule ? '|' : 'c';
                    else
                        cols.push_back( next_rule ? ']' : 'r' );
                    break;
                case U's': small = true; break;
                case U'─': hdr_rule = true; break;
                case U'=': even = true; break;
                case U'┄': break;
                case U'│': case U'┼': case U'┤':
                    if ( finished == int( cols.size() ) )
                        cols.push_back( next_rule ? '|' : 'c' );
                    else if ( cols.back() == 'l' && next_rule )
                        cols.back() = '[';
                    next_rule = sep[ i ] == U'│';
                    colw.push_back( width );
                    width = 0;
                    ++ finished;
                    break;
                default:
                    throw std::runtime_error( "unexpected char in table header" );
            }
        }

        w.table_start( cols, even );
        if ( small )
            w.small_start();

        for ( int i = 0; i < sep_line; ++i )
            emit_line( lines[ i ] );

        rule = hdr_rule;

        for ( int i = sep_line; i < int( lines.size() ); ++i )
            emit_line( lines[ i ] );

        if ( small )
            w.small_stop();
        w.table_stop();
    }

    void convert::recurse( const std::u32string &buf )
    {
        int ldepth = rec_list_depth;
        rec_list_depth = _list.size();
        auto backup = todo;
        todo = buf;
        body();
        end_list( _list.size() - rec_list_depth );
        todo = backup;
        rec_list_depth = ldepth;
    }

    void convert::try_nested()
    {
        std::u32string buf;

        while ( true )
        {
            if ( todo.empty() ) break;
            if ( todo[ 0 ] != U'│' ) break;

            auto l = fetch_line();
            l.remove_prefix( std::min( 2lu, l.size() ) ); /* strip │ and a space */
            buf += l;
            buf += U"\n";
        }

        if ( !buf.empty() )
        {
            w.nest_start();
            recurse( buf );
            w.nest_end();
        }
    }

    bool convert::try_directive()
    {
        if ( starts_with( todo, U"$$raw_mpost" ) )
        {
            w.mpost_start();
            auto l = fetch_line();
            while ( !todo.empty() )
            {
                l = fetch_line();
                if ( l == U"$$end_mpost" )
                    break;
                w.mpost_write( to_utf8( l ) );
                w.mpost_write( "\n" );
            }
            w.mpost_stop();

            return true;
        }

        if ( starts_with( todo, U"$$html" ) )
        {
            auto l = fetch_line();
            l.remove_prefix( 7 );
            w.html( l );

            return true;
        }

        return false;
    }

    void convert::body()
    {
        if ( try_directive() )
            body();

        try_table();
        while ( try_dispmath() );
        if ( !in_code )
            try_picture();
        try_nested();

        if ( todo.empty() )
        {
            end_code();
            return;
        }

        if ( !in_code && _list.empty() && todo[ 0 ] == U'\n' )
            return w.paragraph(), fetch_line(), body();

        for ( auto c : U"┄" )
        {
            bool hrule = todo.size() >= 40;

            for ( int i = 0; hrule && i < 40; ++ i )
                if ( todo[ i ] != c )
                    hrule = false;

            if ( hrule )
                fetch_line(), w.hrule( c );
        }

        if ( sv( U"¹²³⁴⁵⁶⁷⁸⁹" ).find( todo[ 0 ] ) != sv::npos )
            return fetch_par(), body();

        if ( todo[ 0 ] == U'' )
            return end_list( -1 ), w.pagebreak(), fetch_line(), body();

        if ( todo[ 0 ] == U'>' )
            return emit_quote(), body();
        else
            end_quote();

        if ( white_count() >= 4 )
            return emit_code(), body();
        else
            end_code();

        switch ( nonwhite() )
        {
            case 0: return;
            case U'•': ensure_list( 1, list::bullets ); break;
            case U'◦': ensure_list( 2, list::bullets ); break;
            case U'‣': ensure_list( 3, list::bullets ); break;
            case U'#': heading(); break;

            default:
                if ( !try_enum() )
                {
                    auto l = fetch_line();
                    end_list( _list.size() - rec_list_depth );
                    emit_text( l );
                    w.text( U"\n" );
                }
        }

        body();
    }

    void convert::header()
    {
        if ( todo[ 0 ] != U':' )
            return w.meta_end();
        skip( U':' );
        skip_white();
        auto key = fetch_word();
        skip_white();
        if ( skip( U':' ) )
        {
            skip_white();
            auto v = fetch_line();
            w.meta( key, v );

            if ( key == U"typing" )
                default_typing = v;
        }

        header();
    }

    void convert::run()
    {
        header();
        body();
        end_list( -1 );
        w.end();
    }

}
