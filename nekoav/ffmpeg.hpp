#pragma once

#include "defs.hpp"

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
}

NEKO_NS_BEGIN

template <typename T>
struct FFPtr {

};


NEKO_NS_END