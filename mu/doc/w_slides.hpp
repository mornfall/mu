#pragma once
#include "writer.hpp"
#include "w_context.hpp"
#include <vector>
#include <sstream>
#include <iostream>

namespace umd::doc
{
    struct w_slides : w_context
    {
        w_slides( stream &out ) : w_context( out ) { format = U"slides"; }
    };
}
