#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/format.hpp>
#include <system_error>
#include <chrono>

extern "C" {
    #include <libavutil/avutil.h>
    #include <libavutil/frame.h>
    #include <libavutil/pixfmt.h>
    #include <libavutil/samplefmt.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

namespace nekoav {
namespace time {
    static constexpr auto NANO_TIME_BASE = AVRational {1, 1000000000};

    inline auto toFFmpeg(std::chrono::nanoseconds ns, AVRational timebase) -> int64_t {
        return av_rescale_q(ns.count(), NANO_TIME_BASE, timebase);
    }

    inline auto fromFFmpeg(int64_t ts, AVRational timebase) -> std::chrono::nanoseconds {
        return std::chrono::nanoseconds(av_rescale_q(ts, timebase, NANO_TIME_BASE));
    }
} // namespace time

namespace pixfmt {
    inline auto toFFmpeg(PixelFormat fmt) {
        switch (fmt) {
            case PixelFormat::None:    return AV_PIX_FMT_NONE;
            case PixelFormat::YUV420P: return AV_PIX_FMT_YUV420P;
            case PixelFormat::YUV422P: return AV_PIX_FMT_YUV422P;
            case PixelFormat::YUV444P: return AV_PIX_FMT_YUV444P;
            case PixelFormat::YUV410P: return AV_PIX_FMT_YUV410P;
            case PixelFormat::YUV411P: return AV_PIX_FMT_YUV411P;
            case PixelFormat::UYVY422: return AV_PIX_FMT_UYVY422;
            case PixelFormat::UYYVYY411: return AV_PIX_FMT_UYYVYY411;
            case PixelFormat::BGR8: return AV_PIX_FMT_BGR8;
            case PixelFormat::BGR4: return AV_PIX_FMT_BGR4;
            case PixelFormat::BGR4_BYTE: return AV_PIX_FMT_BGR4_BYTE;
            case PixelFormat::RGB8: return AV_PIX_FMT_RGB8;
            case PixelFormat::RGB4: return AV_PIX_FMT_RGB4;
            case PixelFormat::NV12: return AV_PIX_FMT_NV12;
            case PixelFormat::NV21: return AV_PIX_FMT_NV21;
            
            case PixelFormat::RGBA: return AV_PIX_FMT_RGBA;
            case PixelFormat::BGRA: return AV_PIX_FMT_BGRA;
            case PixelFormat::ARGB: return AV_PIX_FMT_ARGB;

            case PixelFormat::RGBA64LE: return AV_PIX_FMT_RGBA64LE;
            case PixelFormat::RGBA64BE: return AV_PIX_FMT_RGBA64BE;

            case PixelFormat::P010LE: return AV_PIX_FMT_P010LE;
            case PixelFormat::P010BE: return AV_PIX_FMT_P010BE;

            // Hardware
            case PixelFormat::DXVA2: return AV_PIX_FMT_DXVA2_VLD;
            case PixelFormat::D3D11: return AV_PIX_FMT_D3D11;
            case PixelFormat::VAAPI: return AV_PIX_FMT_VAAPI;
            case PixelFormat::VDPAU: return AV_PIX_FMT_VDPAU;
            case PixelFormat::OpenCL: return AV_PIX_FMT_OPENCL;

            default: return AV_PIX_FMT_NONE;
        }
    }

    inline auto fromFFmpeg(AVPixelFormat fmt) -> PixelFormat {
        switch (fmt) {
            case AV_PIX_FMT_NONE: return PixelFormat::None;
            case AV_PIX_FMT_YUV420P: return PixelFormat::YUV420P;
            case AV_PIX_FMT_YUV422P: return PixelFormat::YUV422P;
            case AV_PIX_FMT_YUV444P: return PixelFormat::YUV444P;
            case AV_PIX_FMT_YUV410P: return PixelFormat::YUV410P;
            case AV_PIX_FMT_YUV411P: return PixelFormat::YUV411P;
            case AV_PIX_FMT_UYVY422: return PixelFormat::UYVY422;
            case AV_PIX_FMT_UYYVYY411: return PixelFormat::UYYVYY411;
            case AV_PIX_FMT_BGR8: return PixelFormat::BGR8;
            case AV_PIX_FMT_BGR4: return PixelFormat::BGR4;
            case AV_PIX_FMT_BGR4_BYTE: return PixelFormat::BGR4_BYTE;
            case AV_PIX_FMT_RGB8: return PixelFormat::RGB8;
            case AV_PIX_FMT_RGB4: return PixelFormat::RGB4;
            case AV_PIX_FMT_NV12: return PixelFormat::NV12;
            case AV_PIX_FMT_NV21: return PixelFormat::NV21;
            
            case AV_PIX_FMT_RGBA: return PixelFormat::RGBA;
            case AV_PIX_FMT_BGRA: return PixelFormat::BGRA;
            case AV_PIX_FMT_ARGB: return PixelFormat::ARGB;

            case AV_PIX_FMT_RGBA64LE: return PixelFormat::RGBA64LE;
            case AV_PIX_FMT_RGBA64BE: return PixelFormat::RGBA64BE;

            case AV_PIX_FMT_P010LE: return PixelFormat::P010LE;
            case AV_PIX_FMT_P010BE: return PixelFormat::P010BE;

            // Hardware
            case AV_PIX_FMT_DXVA2_VLD: return PixelFormat::DXVA2;
            case AV_PIX_FMT_D3D11: return PixelFormat::D3D11;
            case AV_PIX_FMT_VDPAU: return PixelFormat::VDPAU;
            case AV_PIX_FMT_VAAPI: return PixelFormat::VAAPI;
            case AV_PIX_FMT_OPENCL: return PixelFormat::OpenCL;
            default: return PixelFormat::None;
        }
    }
} // namespace pixfmt

namespace sample_fmt {
    inline auto toFFmpeg(SampleFormat fmt) -> AVSampleFormat {
        switch (fmt) {
            case SampleFormat::None: return AV_SAMPLE_FMT_NONE;
            case SampleFormat::U8: return AV_SAMPLE_FMT_U8;
            case SampleFormat::S16: return AV_SAMPLE_FMT_S16;
            case SampleFormat::S32: return AV_SAMPLE_FMT_S32;
            case SampleFormat::FLT: return AV_SAMPLE_FMT_FLT;
            case SampleFormat::DBL: return AV_SAMPLE_FMT_DBL;

            case SampleFormat::U8P: return AV_SAMPLE_FMT_U8P;
            case SampleFormat::S16P: return AV_SAMPLE_FMT_S16P;
            case SampleFormat::S32P: return AV_SAMPLE_FMT_S32P;
            case SampleFormat::FLTP: return AV_SAMPLE_FMT_FLTP;
            case SampleFormat::DBLP: return AV_SAMPLE_FMT_DBLP;

            default: return AV_SAMPLE_FMT_NONE;
        }
    }

    inline auto fromFFmpeg(AVSampleFormat fmt) -> SampleFormat {
        switch (fmt) {
            case AV_SAMPLE_FMT_NONE: return SampleFormat::None;
            case AV_SAMPLE_FMT_U8: return SampleFormat::U8;
            case AV_SAMPLE_FMT_S16: return SampleFormat::S16;
            case AV_SAMPLE_FMT_S32: return SampleFormat::S32;
            case AV_SAMPLE_FMT_FLT: return SampleFormat::FLT;
            case AV_SAMPLE_FMT_DBL: return SampleFormat::DBL;

            case AV_SAMPLE_FMT_U8P: return SampleFormat::U8P;
            case AV_SAMPLE_FMT_S16P: return SampleFormat::S16P;
            case AV_SAMPLE_FMT_S32P: return SampleFormat::S32P;
            case AV_SAMPLE_FMT_FLTP: return SampleFormat::FLTP;
            case AV_SAMPLE_FMT_DBLP: return SampleFormat::DBLP;

            default: return SampleFormat::None;
        }
    }
} // namespace sample_fmt

namespace error {

} // error

} // namespace nekoav