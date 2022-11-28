#include "pic/scene.hpp"
#include "pic/reader.hpp"
#include "pic/convert.hpp"
#include "doc/writer.hpp"
#include "doc/convert.hpp"
#include "doc/util.hpp"

#include <set>
#include <sstream>
#include <iostream>
#include <codecvt>
#include <unicode/uchar.h>

namespace umd::doc
{

    char32_t convert::nonwhite()
    {
        sv l = todo.top();
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
        if ( !starts_with( c ) )
            return false;
        shift();
        return true;
    }

    int convert::skip_bullet_lead()
    {
        int indent = 0;
        bool saw_leader = false;

        while ( have_chars() )
        {
            auto ch = peek();
            if ( ch != U' ' && saw_leader ) return indent;

            ++ indent;
            shift();

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
            if ( std::islower( lstr[ 0 ] ) ) return int( lstr[ 0 ] - U'a' + 1 );
            if ( std::isupper( lstr[ 0 ] ) ) return int( lstr[ 0 ] - U'A' + 1 );
            return int( lstr[ 0 ] - U'α' + 1 );
        };

        while ( have_chars() )
        {
            auto ch = peek();

            if ( ch != U' ' && leader == 2 ) return { indent, first() };

            ++ indent;
            shift(); /* commit to eating the char */

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
            case list::numbered:
            case list::lettered: return skip_enum_lead();
        }
        __builtin_trap();
    }

    void convert::heading()
    {
        end_list( -1 );
        int level = 0;
        skip_white();
        while ( skip( U'#' ) )
            ++ level;
        skip_white();

        auto line = fetch_line();
        auto [ first, rest ] = brq::split( line, U' ' );

        if ( first.back() == '.' )
        {
            first.remove_suffix( 1 );
            w.heading_start( level, first, rest );
        }
        else
        {
            if ( brq::starts_with( line, U"‹" ) && brq::ends_with( line, U"›" ) )
                line = line.substr( 1, line.size() - 2 );

            w.heading_start( level, U"", line );
            emit_text( first );
            emit_text( U" " );
        }

        emit_text( rest );

        while ( skip( U'\n' ) );
        w.heading_stop();
    }

    bool convert::end_list( int count )
    {
        if ( _list.empty() || !count )
            return false;

        auto type = _list.top().t;
        _list.pop();

        switch ( type )
        {
            case list::bullets: w.bullet_stop(); break;
            case list::numbered:
            case list::lettered: w.enum_stop(); break;
        }

        end_list( count - 1 );
        return true;
    }

    void convert::start_list( list::type l, int indent, int first )
    {
        switch ( l )
        {
            case list::bullets:  w.bullet_start( _list.size() ); break;
            case list::numbered:
            case list::lettered: w.enum_start( _list.size(), first, l == list::lettered ); break;
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
        else if ( _list.empty() )
            brq::raise() << "inner list without an outer list";
        else
            indent = _list.top().indent;

        assert( int( _list.size() ) == l );

        switch ( t )
        {
            case list::bullets:  w.bullet_item(); break;
            case list::numbered:
            case list::lettered: w.enum_item(); break;
        }

        std::u32string buf{ fetch_line() };
        buf += U'\n';

        while ( have_chars() )
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
        auto l = todo.top();
        skip_white( l );

        int digits = 0;
        enum { none, upper, lower } alpha = none;

        while ( !l.empty() && std::isdigit( l[ 0 ] ) )
            l.remove_prefix( 1 ), ++ digits;

        if ( !digits && !l.empty() )
            if ( auto letter = l[ 0 ]; std::isalpha( letter ) )
            {
                l.remove_prefix( 1 );
                alpha = std::islower( letter ) ? lower : upper;
            }

        if ( l.size() < 2 || l[ 0 ] != U'.' || l[ 1 ] != U' ' )
            return false;

        if ( digits || alpha == upper )
            ensure_list( 1, alpha == upper ? list::lettered : list::numbered );
        else if ( alpha == lower )
            ensure_list( 2, list::numbered );
        else
            UNREACHABLE( digits, alpha, l );

        return digits || alpha != none;
    }

    inline bool is_ref( span s )
    {
        return s == span::lref || s == span::gref;
    }

    template< typename flush_t >
    void convert::span_start( flush_t flush, span s )
    {
        if ( !_spans.empty() && is_ref( _spans.top() ) )
            span_stop( flush, _spans.top() );

        _spans.push( s );
        flush();

        switch ( s )
        {
            case span::tt: w.tt_start(); break;
            case span::em: w.em_start(); break;
            case span::bf: w.bf_start(); break;
            case span::lref:
            case span::gref: break;
        }
    }

    template< typename flush_t >
    void convert::span_stop( flush_t flush, span s )
    {
        if ( is_ref( _spans.top() ) && !is_ref( s ) )
            span_stop( flush, _spans.top() );

        if ( _spans.top() != s )
            brq::raise() << "mismatched span: tried to close " << s << " but expected " << _spans.top();

        if ( is_ref( _spans.top() ) )
            w.ref_start( flush( 0, false ), _spans.top() == span::gref );

        flush();
        _spans.pop();

        switch ( s )
        {
            case span::tt: w.tt_stop(); break;
            case span::em: w.em_stop(); break;
            case span::bf: w.bf_stop(); break;
            case span::lref:
            case span::gref: w.ref_stop(); break;
        }
    }

    void convert::emit_text( std::u32string_view v )
    {
        sv sup = U"¹²³⁴⁵⁶⁷⁸⁹";

        auto char_cb = [&]( auto flush, char32_t c )
        {
            switch ( c )
            {
                case U'‹': if ( !in_math ) span_start( flush, span::tt ); break;
                case U'›': if ( !in_math ) span_stop( flush, span::tt ); break;
                case U'▷': span_start( flush, span::lref ); break;
                case U'▶': span_start( flush, span::gref ); break;
                case U'«': span_start( flush, span::em ); break;
                case U'»': span_stop( flush, span::em ); break;
                case U'❮': span_start( flush, span::bf ); break;
                case U'❯': span_stop( flush, span::bf ); break;
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
                        emit_footnote( flush, c );
            }
        };

        process( v, char_cb, [&]( auto s ) { w.text( s ); } );
    }

    void convert::try_quote()
    {
        std::u32string buf;

        while ( true )
        {
            if ( todo.empty() ) break;
            if ( !starts_with( U'>' ) ) break;

            auto l = fetch_line();
            l.remove_prefix( std::min( 2lu, l.size() ) );
            buf += l;
            buf += U"\n";
        }

        if ( !buf.empty() )
        {
            w.quote_start();
            recurse( buf );
            w.quote_stop();
        }
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

    template< typename flush_t >
    void convert::process_footnote( flush_t flush, sv par )
    {
        w.footnote_head();
        flush();
        w.footnote_start();

        while ( !par.empty() )
        {
            auto line = fetch( par, newline );

            if ( line.size() >= 1 && sv( U"¹²³⁴⁵⁶⁷⁸⁹" ).find( line[ 0 ] ) != sv::npos )
                break; /* another footnote → done */

            _last_footnote = line;
            emit_text( line );
        }

        w.footnote_stop();
    }

    template< typename flush_t >
    void convert::emit_footnote( flush_t flush, char32_t head )
    {
        std::stack< sv > backup;

        while ( todo.size() > 1 )
            backup.emplace( todo.top() ), todo.pop();
        checkpoint();
        skip_to_addr( &*_last_footnote.end() + 1 );
        while ( skip( U'\n' ) );

        while ( have_chars() )
        {
            auto par = fetch_par();
            if ( par.size() >= 2 && par[ 0 ] == head && par[ 1 ] == U' ' )
            {
                _footnotes.insert( par.data() );
                process_footnote( flush, par.substr( 2 ) );
                break;
            }
            else if ( sv( U"¹²³⁴⁵⁶⁷⁸⁹" ).find( par[ 0 ] ) != sv::npos )
                break; /* mismatch → not a footnote */
        }

        restore();
        while ( backup.size() )
            todo.emplace( backup.top() ), backup.pop();
    }

    void convert::try_picture()
    {
        if ( !white_count() )
            return;

        bool special = false;
        int i = 0;

        auto chk = todo.top();

        for ( ; i < int( chk.size() ) && chk[ i ] != '\n'; ++i )
            if ( ( chk[ i ] >= 0x2500 && chk[ i ] < 0x2580 ) || chk[ i ] == U'●' )
                special = true;

        if ( !special ) return;

        for ( ; i < int( chk.size() ) - 1 && ( chk[ i ] != '\n' || chk[ i + 1 ] != '\n' ) ; ++ i );

        w.mpost_start();

        try
        {
            auto grid = pic::reader::read_grid( peek( i ) );
            auto scene = pic::convert::scene( grid );
            scene.emit( *this );
        }
        catch ( const pic::bad_picture &bp )
        {
            std::cerr << bp.what() << std::endl;
            std::cerr << to_utf8( bp.picture ) << std::endl;
            w.mpost_write( "label( 0, 0, btex error processing figure etex );" );
        }

        w.mpost_stop();

        shift( i );
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

        if ( white_count() > 0 && white_count() <= 3 && nonwhite() == U'⟦' )
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
            if ( starts_with( U'├' ) )
                sep = fetch_line(), sep_line = lines.size();
            else if ( starts_with( U'│' ) )
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
        todo.emplace( buf );
        body();
        end_list( _list.size() - rec_list_depth );
        todo.pop();
        rec_list_depth = ldepth;
    }

    void convert::try_nested()
    {
        std::u32string buf;

        while ( true )
        {
            if ( todo.empty() ) break;
            if ( !starts_with( U'│' ) ) break;

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
        if ( starts_with( U"$$raw_mpost" ) )
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

        if ( starts_with( U"$$html" ) )
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
        try_quote();

        if ( todo.empty() )
        {
            end_code();
            return;
        }

        if ( !in_code && _list.empty() && starts_with( U'\n' ) )
            return w.paragraph(), fetch_line(), body();

        for ( auto c : U"┄" )
            if ( starts_with( std::u32string( 40, c ) ) )
                fetch_line(), w.hrule( c );

        if ( _footnotes.count( todo.top().data() ) )
            return fetch_par(), body();

        if ( starts_with( U'' ) )
            return end_list( -1 ), w.pagebreak(), fetch_line(), body();

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
        if ( !starts_with( U':' ) )
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
