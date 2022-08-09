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
}
