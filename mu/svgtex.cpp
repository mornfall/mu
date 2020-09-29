#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <string_view>
#include <limits>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>

#include <glib.h>
#include <poppler.h>
#include <poppler-document.h>
#include <poppler-page.h>
#include <cairo.h>
#include <cairo-svg.h>

extern char **environ;

namespace brq
{
    template< typename char_t >
    using split_view_t = std::pair< std::basic_string_view< char_t >, std::basic_string_view< char_t > >;
    using split_view = split_view_t< char >;

    template< typename char_t >
    static inline split_view_t< char_t > split( std::basic_string_view< char_t > p,
                                                decltype( p ) d, bool reverse = false )
    {
        using view = std::basic_string_view< char_t >;
        auto s = reverse ? p.rfind( d ) : p.find( d );
        if ( s == p.npos )
            return reverse ? split_view_t< char_t >{ view(), p } : split_view_t< char_t >{ p, view() };
        return { p.substr( 0, s ), p.substr( s + d.size(), p.npos ) };
    }

    static inline split_view split( std::string_view p, std::string_view d, bool reverse = false )
    {
        return split< char >( p, d, reverse );
    }

    std::string read_file( std::ifstream &in, size_t length = std::numeric_limits< size_t >::max() )
    {
        if ( !in.is_open() )
            throw std::runtime_error( "reading filestream" );

        in.seekg( 0, std::ios::end );
        length = std::min< size_t >( length, in.tellg() );
        in.seekg( 0, std::ios::beg );

        std::string buffer;
        buffer.resize( length );

        in.read( &buffer[ 0 ], length );
        return buffer;
    }

    std::string read_file( const std::string &file, size_t length = std::numeric_limits< size_t >::max() )
    {
        std::ifstream in( file.c_str(), std::ios::binary );
        if ( !in.is_open() )
            throw std::runtime_error( "reading filestream" );
        return read_file( in, length );
    }

    std::string read_file_or( const std::string& file, const std::string& def,
                              size_t length = std::numeric_limits< size_t >::max() )
    {
        std::ifstream in( file.c_str(), std::ios::binary );
        if ( !in.is_open() )
            return def;
        return read_file( in, length );
    }
}

template< typename... args_t >
void run( args_t... args )
{
    std::vector< std::string > argvs = { std::string( args )... };
    char *argv[ sizeof...( args ) + 1 ] = { nullptr };

    for ( size_t i = 0; i < sizeof...( args ); ++ i )
        argv[ i ] = argvs[ i ].data();

    posix_spawn_file_actions_t fact;
    int pid, status;

    int devnull = open( "/dev/null", O_WRONLY );
    posix_spawn_file_actions_init( &fact );
    posix_spawn_file_actions_adddup2( &fact, devnull, 1 );

    int err = posix_spawnp( &pid, argv[ 0 ], &fact, nullptr, argv, environ );

    posix_spawn_file_actions_destroy( &fact );
    close( devnull );

    if ( err )
        throw std::runtime_error( "spawn of " + argvs[ 0 ] + " failed" );

    waitpid( pid, &status, 0 );
    if ( !WIFEXITED( status ) || WEXITSTATUS( status ) != 0 )
        throw std::runtime_error( "error running " + argvs[ 0 ] +
                " exit code " + std::to_string( WEXITSTATUS( status ) ) );
}

auto write_sv = []( int fd, std::string_view str )
{
    return write( fd, str.begin(), str.size() );
};

void convert_page( PopplerPage *page, int docid )
{
    double width, height;

    poppler_page_get_size( page, &width, &height );
    std::string buf;

    auto do_write = []( void *capture, const unsigned char *data, unsigned len )
    {
        std::string *buf = reinterpret_cast< std::string * >( capture );
        *buf += std::string_view( reinterpret_cast< const char * >( data ), len );
        return CAIRO_STATUS_SUCCESS;
    };

    auto surface = cairo_svg_surface_create_for_stream( do_write, &buf, width, height );

    auto drawcontext = cairo_create(surface);
    poppler_page_render_for_printing(page, drawcontext);
    cairo_show_page(drawcontext);
    cairo_destroy(drawcontext);

    drawcontext = cairo_create(surface);
    poppler_page_render_for_printing(page, drawcontext);
    cairo_show_page(drawcontext);
    cairo_destroy(drawcontext);

    cairo_surface_destroy(surface);

    std::string_view todo = buf;
    todo.remove_prefix( strlen( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" ) );

    std::vector< std::string_view > tweak =
    {
        "id=\"clip",
        "url(#clip",
        "id=\"glyph",
        "xlink:href=\"#glyph"
    };

    while ( !todo.empty() )
    {
        std::string_view next = tweak[ 0 ];
        size_t offset = INT_MAX;

        for ( auto str : tweak )
            if ( todo.find( str ) < offset )
                offset = todo.find( str ), next = str;

        auto [ pass, tail ] = brq::split( todo, next );

        if ( tail.empty() )
            break;

        ::write( 1, pass.begin(), pass.size() );
        write_sv( 1, next );
        write_sv( 1, "-" );
        write_sv( 1, std::to_string( docid ) );
        write_sv( 1, "-" );
        todo = tail;
    }

    ::write( 1, todo.begin(), todo.size() - 1 );

    g_object_unref(page);
}

std::string read_stdin()
{
    int count;
    char buf[ 1024 ];
    std::string doc;

    while ( ( count = read( 0, buf, 1024 ) ) > 0 )
        doc += std::string_view( buf, count );

    return doc;
}

void process()
{
    std::string doc = read_stdin();
    std::string_view w( doc );
    std::vector< std::string_view > keep;
    std::set< int > use_yshift;

    int mp = open( "tosvg.tex", O_WRONLY | O_CREAT, 0666 );
    write_sv( mp, "\\input{prelude-typescript.tex}\n" );
    write_sv( mp, "\\starttext" );

    while ( !w.empty() )
    {
        auto [ pass, examine ] = brq::split( w, "<tex>" );
        auto [ process, tail ] = brq::split( examine, "</tex>" );

        if ( process.find( "yshift.txt" ) != process.npos )
            use_yshift.insert( keep.size() );

        w = tail;
        keep.push_back( pass );
        write_sv( mp, process );
    }

    write_sv( mp, "\\stoptext" );
    close( mp );

    run( "context", "tosvg.tex" );
    std::ifstream yshift( "yshift.txt" );

    auto pdf_data = brq::read_file( "tosvg.pdf" );
    auto pdf = poppler_document_new_from_data( pdf_data.data(), pdf_data.size(), nullptr, nullptr );

    if ( int( keep.size() - 1 ) != poppler_document_get_n_pages( pdf ) )
        throw std::runtime_error( "each <tex> must produce exactly 1 page" );

    for ( unsigned i = 0; i < keep.size() - 1; ++ i )
    {
        write_sv( 1, keep[ i ] );

        if ( use_yshift.count( i ) )
        {
            std::string yshift_amount;
            yshift >> yshift_amount;
            write_sv( 1, "<span style=\"vertical-align: " );
            write_sv( 1, yshift_amount );
            write_sv( 1, "pt\">" );
        }

        convert_page( poppler_document_get_page( pdf, i ), i );

        if ( use_yshift.count( i ) )
            write_sv( 1, "</span>" );
    }

    write_sv( 1, keep.back() );
    g_object_unref( pdf );
}

int main()
{
    char tmpdir[] = "/tmp/svgtex.XXXXXX";

    if ( !mkdtemp( tmpdir ) )
        abort();
    chdir( tmpdir );

    process();
    run( "rm", "-r", tmpdir );
}
