#define _NEKO_SOURCE
#include "factory_impl.hpp"

NEKO_NS_BEGIN

ElementFactory *GetFFmpegFactory() {
    static FFElementFactory f;
    return &f;
}

NEKO_NS_END