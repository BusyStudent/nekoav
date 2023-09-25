#pragma once

#include "defs.hpp"

extern "C" {
    #include <libavutil/pixfmt.h>
    #include <libavutil/samplefmt.h>
}

NEKO_NS_BEGIN

enum class PixelFormat : int {
    None    = AV_PIX_FMT_NONE,
    YUV420P = AV_PIX_FMT_YUV420P,
    YUV422P = AV_PIX_FMT_YUV422P,

    RGBA    = AV_PIX_FMT_RGBA,
    BGRA    = AV_PIX_FMT_BGRA,
    ARGB    = AV_PIX_FMT_ARGB,
};

enum class SampleFormat : int {

};

NEKO_NS_END