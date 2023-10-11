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
    YUV444P = AV_PIX_FMT_YUV444P,
    YUV410P = AV_PIX_FMT_YUV410P,
    YUV411P = AV_PIX_FMT_YUV411P,
    YUVJ420P = AV_PIX_FMT_YUVJ420P,
    YUVJ422P = AV_PIX_FMT_YUVJ422P,
    YUVJ444P = AV_PIX_FMT_YUVJ444P,
    UYVY422 = AV_PIX_FMT_UYVY422,
    UYYVYY411 = AV_PIX_FMT_UYYVYY411,
    BGR8 = AV_PIX_FMT_BGR8,
    BGR4 = AV_PIX_FMT_BGR4,
    BGR4_BYTE = AV_PIX_FMT_BGR4_BYTE,
    RGB8 = AV_PIX_FMT_RGB8,
    RGB4 = AV_PIX_FMT_RGB4,
    RGB4_BYTE = AV_PIX_FMT_RGB4_BYTE,
    NV12 = AV_PIX_FMT_NV12,
    NV21 = AV_PIX_FMT_NV21,

    RGBA    = AV_PIX_FMT_RGBA,
    BGRA    = AV_PIX_FMT_BGRA,
    ARGB    = AV_PIX_FMT_ARGB,
};

enum class SampleFormat : int {
    None    = AV_SAMPLE_FMT_NONE,
    U8      = AV_SAMPLE_FMT_U8,
    S16     = AV_SAMPLE_FMT_S16,
    S32     = AV_SAMPLE_FMT_S32,
    FLT     = AV_SAMPLE_FMT_FLT,
    DBL     = AV_SAMPLE_FMT_DBL,

    U8P     = AV_SAMPLE_FMT_U8P,
    S16P    = AV_SAMPLE_FMT_S16P,
    S32P    = AV_SAMPLE_FMT_S32P,
    FLTP    = AV_SAMPLE_FMT_FLTP,
    DBLP    = AV_SAMPLE_FMT_DBLP,
};

inline bool IsSampleFormatPlanar(SampleFormat fmt) {
    return fmt == SampleFormat::U8P || fmt == SampleFormat::S16P || 
          fmt == SampleFormat::S32P || fmt == SampleFormat::FLTP || fmt == SampleFormat::DBLP;
}
inline size_t GetBytesPerSample(SampleFormat fmt) {
    switch (fmt) {
        case SampleFormat::None: return 0;
        
        case SampleFormat::U8: return sizeof(uint8_t);
        case SampleFormat::S16: return sizeof(int16_t);
        case SampleFormat::S32: return sizeof(int32_t);
        case SampleFormat::FLT: return sizeof(float);
        case SampleFormat::DBL: return sizeof(double);

        case SampleFormat::U8P: return sizeof(uint8_t);
        case SampleFormat::S16P: return sizeof(int16_t);
        case SampleFormat::S32P: return sizeof(int32_t);
        case SampleFormat::FLTP: return sizeof(float);
        case SampleFormat::DBLP: return sizeof(double);
    }
}
NEKO_NS_END