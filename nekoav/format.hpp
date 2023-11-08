#pragma once

#include "defs.hpp"
#include <cstdint>

NEKO_NS_BEGIN

enum class PixelFormat : int {
    None    = -1,
    YUV420P ,
    YUV422P ,
    YUV444P ,
    YUV410P ,
    YUV411P ,
    YUVJ420P,
    YUVJ422P,
    YUVJ444P,
    UYVY422 ,
    UYYVYY411,
    BGR8,
    BGR4,
    BGR4_BYTE,
    RGB8,
    RGB4,
    NV12,
    NV21,

    RGBA,
    BGRA,
    ARGB,
};

enum class SampleFormat : int {
    None = -1,
    U8  ,
    S16 ,
    S32 ,
    FLT ,
    DBL ,

    U8P  ,
    S16P ,
    S32P ,
    FLTP ,
    DBLP ,
};

// SampleFormat here

constexpr bool IsSampleFormatPlanar(SampleFormat fmt) noexcept {
    return fmt == SampleFormat::U8P || fmt == SampleFormat::S16P || 
          fmt == SampleFormat::S32P || fmt == SampleFormat::FLTP || fmt == SampleFormat::DBLP;
}
constexpr size_t GetBytesPerSample(SampleFormat fmt) noexcept {
    switch (fmt) {
        default: return 0;
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
constexpr size_t GetBytesPerFrame(SampleFormat fmt, int channels) noexcept {
    return GetBytesPerSample(fmt) * channels;
}
constexpr SampleFormat GetPackedSampleFormat(SampleFormat fmt) {
    switch (fmt) {
        default: 
        case SampleFormat::None: return SampleFormat::None;

        case SampleFormat::U8P: return SampleFormat::U8;
        case SampleFormat::S16P: return SampleFormat::S16;
        case SampleFormat::S32P: return SampleFormat::S32;
        case SampleFormat::FLTP: return SampleFormat::FLT;
        case SampleFormat::DBLP: return SampleFormat::DBL;
    }
}
constexpr SampleFormat GetPlanarSampleFormat(SampleFormat fmt) noexcept {
    switch (fmt) {
        default: 
        case SampleFormat::None: return SampleFormat::None;

        case SampleFormat::U8: return SampleFormat::U8P;
        case SampleFormat::S16: return SampleFormat::S16P;
        case SampleFormat::S32: return SampleFormat::S32P;
        case SampleFormat::FLT: return SampleFormat::FLTP;
        case SampleFormat::DBL: return SampleFormat::DBLP;
    }
}
constexpr SampleFormat GetAltSampleFormat(SampleFormat fmt) noexcept {
    if (IsSampleFormatPlanar(fmt)) {
        return GetPackedSampleFormat(fmt);
    }
    return GetPlanarSampleFormat(fmt);
}

// PixelFormat here

NEKO_NS_END