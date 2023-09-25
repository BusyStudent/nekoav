#pragma once

#include "defs.hpp"
#include "format.hpp"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
}

NEKO_NS_BEGIN

template <typename T>
struct FFPtr {

};

inline AVPixelFormat ToAVPixelFormat(PixelFormat fmt) {
    return AVPixelFormat(fmt);
}
inline   PixelFormat ToPixelFormat(AVPixelFormat fmt) {
    return PixelFormat(fmt);
}

NEKO_NS_END