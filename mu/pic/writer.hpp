#pragma once
#include <string_view>
#include <sstream>

namespace umd::pic
{
    struct writer
    {
        virtual void emit_mpost( std::string_view ) = 0;
        virtual void emit_tex( std::u32string_view ) = 0;
    };

    template< typename T >
    auto operator<<( writer &w, const T &t ) -> decltype( std::declval< std::stringstream & >() << t, w )
    {
        std::stringstream str;
        str << t;
        w.emit_mpost( str.str() );
        return w;
    }
}
