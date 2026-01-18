#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/format.hpp>
#include <nekoav/error.hpp>
#include <system_error>
#include <chrono>

#if defined(__cpp_lib_format)
    #include <format>
#else
    #define NEKOAV_NO_LOG
#endif // __cpp_lib_format

#if defined(NDEBUG)
    #define NEKOAV_NO_LOG
#endif // NDEBUG

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
            case PixelFormat::None:            return AV_PIX_FMT_NONE;
            case PixelFormat::YUV420P:         return AV_PIX_FMT_YUV420P;
            case PixelFormat::YUV422P:         return AV_PIX_FMT_YUV422P;
            case PixelFormat::YUV444P:         return AV_PIX_FMT_YUV444P;
            case PixelFormat::YUV410P:         return AV_PIX_FMT_YUV410P;
            case PixelFormat::YUV411P:         return AV_PIX_FMT_YUV411P;
            case PixelFormat::YUV440P:         return AV_PIX_FMT_YUV440P;
            case PixelFormat::NV12:            return AV_PIX_FMT_NV12;
            case PixelFormat::NV21:            return AV_PIX_FMT_NV21;
            case PixelFormat::NV16:            return AV_PIX_FMT_NV16;
            case PixelFormat::NV24:            return AV_PIX_FMT_NV24;
            case PixelFormat::NV42:            return AV_PIX_FMT_NV42;
            case PixelFormat::P010LE:          return AV_PIX_FMT_P010LE;
            case PixelFormat::P010BE:          return AV_PIX_FMT_P010BE;
            case PixelFormat::P012LE:          return AV_PIX_FMT_P012LE;
            case PixelFormat::P012BE:          return AV_PIX_FMT_P012BE;
            case PixelFormat::P016LE:          return AV_PIX_FMT_P016LE;
            case PixelFormat::P016BE:          return AV_PIX_FMT_P016BE;
            case PixelFormat::YUYV422:         return AV_PIX_FMT_YUYV422;
            case PixelFormat::UYVY422:         return AV_PIX_FMT_UYVY422;
            case PixelFormat::YVYU422:         return AV_PIX_FMT_YVYU422;
            case PixelFormat::Y210LE:          return AV_PIX_FMT_Y210LE;
            case PixelFormat::Y210BE:          return AV_PIX_FMT_Y210BE;
            case PixelFormat::RGB24:           return AV_PIX_FMT_RGB24;
            case PixelFormat::BGR24:           return AV_PIX_FMT_BGR24;
            case PixelFormat::RGBA:            return AV_PIX_FMT_RGBA;
            case PixelFormat::BGRA:            return AV_PIX_FMT_BGRA;
            case PixelFormat::ARGB:            return AV_PIX_FMT_ARGB;
            case PixelFormat::ABGR:            return AV_PIX_FMT_ABGR;
            case PixelFormat::RGB565LE:        return AV_PIX_FMT_RGB565LE;
            case PixelFormat::RGB565BE:        return AV_PIX_FMT_RGB565BE;
            case PixelFormat::BGR565LE:        return AV_PIX_FMT_BGR565LE;
            case PixelFormat::BGR565BE:        return AV_PIX_FMT_BGR565BE;
            case PixelFormat::RGB555LE:        return AV_PIX_FMT_RGB555LE;
            case PixelFormat::RGB555BE:        return AV_PIX_FMT_RGB555BE;
            case PixelFormat::BGR555LE:        return AV_PIX_FMT_BGR555LE;
            case PixelFormat::BGR555BE:        return AV_PIX_FMT_BGR555BE;
            case PixelFormat::RGB48LE:         return AV_PIX_FMT_RGB48LE;
            case PixelFormat::RGB48BE:         return AV_PIX_FMT_RGB48BE;
            case PixelFormat::BGR48LE:         return AV_PIX_FMT_BGR48LE;
            case PixelFormat::BGR48BE:         return AV_PIX_FMT_BGR48BE;
            case PixelFormat::RGBA64LE:        return AV_PIX_FMT_RGBA64LE;
            case PixelFormat::RGBA64BE:        return AV_PIX_FMT_RGBA64BE;
            case PixelFormat::BGRA64LE:        return AV_PIX_FMT_BGRA64LE;
            case PixelFormat::BGRA64BE:        return AV_PIX_FMT_BGRA64BE;
            case PixelFormat::X2RGB10LE:       return AV_PIX_FMT_X2RGB10LE;
            case PixelFormat::X2RGB10BE:       return AV_PIX_FMT_X2RGB10BE;
            case PixelFormat::X2BGR10LE:       return AV_PIX_FMT_X2BGR10LE;
            case PixelFormat::X2BGR10BE:       return AV_PIX_FMT_X2BGR10BE;
            case PixelFormat::GBRP:            return AV_PIX_FMT_GBRP;
            case PixelFormat::GBRP10LE:        return AV_PIX_FMT_GBRP10LE;
            case PixelFormat::GBRP10BE:        return AV_PIX_FMT_GBRP10BE;
            case PixelFormat::GBRP12LE:        return AV_PIX_FMT_GBRP12LE;
            case PixelFormat::GBRP12BE:        return AV_PIX_FMT_GBRP12BE;
            case PixelFormat::GBRP16LE:        return AV_PIX_FMT_GBRP16LE;
            case PixelFormat::GBRP16BE:        return AV_PIX_FMT_GBRP16BE;
            case PixelFormat::GBRAP:           return AV_PIX_FMT_GBRAP;
            case PixelFormat::GBRAP16LE:       return AV_PIX_FMT_GBRAP16LE;
            case PixelFormat::GBRAP16BE:       return AV_PIX_FMT_GBRAP16BE;
            case PixelFormat::GRAY8:           return AV_PIX_FMT_GRAY8;
            case PixelFormat::GRAY16LE:        return AV_PIX_FMT_GRAY16LE;
            case PixelFormat::GRAY16BE:        return AV_PIX_FMT_GRAY16BE;
            case PixelFormat::YA8:             return AV_PIX_FMT_YA8;
            case PixelFormat::YA16LE:          return AV_PIX_FMT_YA16LE;
            case PixelFormat::YA16BE:          return AV_PIX_FMT_YA16BE;
            case PixelFormat::YUV420P10LE:     return AV_PIX_FMT_YUV420P10LE;
            case PixelFormat::YUV420P10BE:     return AV_PIX_FMT_YUV420P10BE;
            case PixelFormat::YUV422P10LE:     return AV_PIX_FMT_YUV422P10LE;
            case PixelFormat::YUV422P10BE:     return AV_PIX_FMT_YUV422P10BE;
            case PixelFormat::YUV444P10LE:     return AV_PIX_FMT_YUV444P10LE;
            case PixelFormat::YUV444P10BE:     return AV_PIX_FMT_YUV444P10BE;
            case PixelFormat::YUV420P12LE:     return AV_PIX_FMT_YUV420P12LE;
            case PixelFormat::YUV420P12BE:     return AV_PIX_FMT_YUV420P12BE;
            case PixelFormat::YUV422P12LE:     return AV_PIX_FMT_YUV422P12LE;
            case PixelFormat::YUV422P12BE:     return AV_PIX_FMT_YUV422P12BE;
            case PixelFormat::YUV444P12LE:     return AV_PIX_FMT_YUV444P12LE;
            case PixelFormat::YUV444P12BE:     return AV_PIX_FMT_YUV444P12BE;
            case PixelFormat::YUV420P16LE:     return AV_PIX_FMT_YUV420P16LE;
            case PixelFormat::YUV420P16BE:     return AV_PIX_FMT_YUV420P16BE;
            case PixelFormat::YUV422P16LE:     return AV_PIX_FMT_YUV422P16LE;
            case PixelFormat::YUV422P16BE:     return AV_PIX_FMT_YUV422P16BE;
            case PixelFormat::YUV444P16LE:     return AV_PIX_FMT_YUV444P16LE;
            case PixelFormat::YUV444P16BE:     return AV_PIX_FMT_YUV444P16BE;
            case PixelFormat::YUVA420P:        return AV_PIX_FMT_YUVA420P;
            case PixelFormat::YUVA422P:        return AV_PIX_FMT_YUVA422P;
            case PixelFormat::YUVA444P:        return AV_PIX_FMT_YUVA444P;
            case PixelFormat::YUVA420P10LE:    return AV_PIX_FMT_YUVA420P10LE;
            case PixelFormat::YUVA420P10BE:    return AV_PIX_FMT_YUVA420P10BE;
            case PixelFormat::YUVA422P10LE:    return AV_PIX_FMT_YUVA422P10LE;
            case PixelFormat::YUVA422P10BE:    return AV_PIX_FMT_YUVA422P10BE;
            case PixelFormat::YUVA444P10LE:    return AV_PIX_FMT_YUVA444P10LE;
            case PixelFormat::YUVA444P10BE:    return AV_PIX_FMT_YUVA444P10BE;
            case PixelFormat::YUVA420P16LE:    return AV_PIX_FMT_YUVA420P16LE;
            case PixelFormat::YUVA420P16BE:    return AV_PIX_FMT_YUVA420P16BE;
            case PixelFormat::YUVA422P16LE:    return AV_PIX_FMT_YUVA422P16LE;
            case PixelFormat::YUVA422P16BE:    return AV_PIX_FMT_YUVA422P16BE;
            case PixelFormat::YUVA444P16LE:    return AV_PIX_FMT_YUVA444P16LE;
            case PixelFormat::YUVA444P16BE:    return AV_PIX_FMT_YUVA444P16BE;
            case PixelFormat::BAYER_BGGR8:     return AV_PIX_FMT_BAYER_BGGR8;
            case PixelFormat::BAYER_RGGB8:     return AV_PIX_FMT_BAYER_RGGB8;
            case PixelFormat::BAYER_GBRG8:     return AV_PIX_FMT_BAYER_GBRG8;
            case PixelFormat::BAYER_GRBG8:     return AV_PIX_FMT_BAYER_GRBG8;
            case PixelFormat::BAYER_BGGR16LE:  return AV_PIX_FMT_BAYER_BGGR16LE;
            case PixelFormat::BAYER_BGGR16BE:  return AV_PIX_FMT_BAYER_BGGR16BE;
            case PixelFormat::BAYER_RGGB16LE:  return AV_PIX_FMT_BAYER_RGGB16LE;
            case PixelFormat::BAYER_RGGB16BE:  return AV_PIX_FMT_BAYER_RGGB16BE;
            case PixelFormat::BAYER_GBRG16LE:  return AV_PIX_FMT_BAYER_GBRG16LE;
            case PixelFormat::BAYER_GBRG16BE:  return AV_PIX_FMT_BAYER_GBRG16BE;
            case PixelFormat::BAYER_GRBG16LE:  return AV_PIX_FMT_BAYER_GRBG16LE;
            case PixelFormat::BAYER_GRBG16BE:  return AV_PIX_FMT_BAYER_GRBG16BE;
            case PixelFormat::RGBF32LE:        return AV_PIX_FMT_RGBF32LE;
            case PixelFormat::RGBF32BE:        return AV_PIX_FMT_RGBF32BE;
            case PixelFormat::RGBAF32LE:       return AV_PIX_FMT_RGBAF32LE;
            case PixelFormat::RGBAF32BE:       return AV_PIX_FMT_RGBAF32BE;
            case PixelFormat::GBRPF32LE:       return AV_PIX_FMT_GBRPF32LE;
            case PixelFormat::GBRPF32BE:       return AV_PIX_FMT_GBRPF32BE;
            case PixelFormat::GBRAPF32LE:      return AV_PIX_FMT_GBRAPF32LE;
            case PixelFormat::GBRAPF32BE:      return AV_PIX_FMT_GBRAPF32BE;
            case PixelFormat::VAAPI:           return AV_PIX_FMT_VAAPI;
            case PixelFormat::DXVA2_VLD:       return AV_PIX_FMT_DXVA2_VLD;
            case PixelFormat::D3D11:           return AV_PIX_FMT_D3D11;
            case PixelFormat::D3D12:           return AV_PIX_FMT_D3D12;
            case PixelFormat::QSV:             return AV_PIX_FMT_QSV;
            case PixelFormat::CUDA:            return AV_PIX_FMT_CUDA;
            case PixelFormat::VDPAU:           return AV_PIX_FMT_VDPAU;
            case PixelFormat::VIDEOTOOLBOX:    return AV_PIX_FMT_VIDEOTOOLBOX;
            case PixelFormat::MEDIACODEC:      return AV_PIX_FMT_MEDIACODEC;
            case PixelFormat::VULKAN:          return AV_PIX_FMT_VULKAN;
            case PixelFormat::OPENCL:          return AV_PIX_FMT_OPENCL;
            case PixelFormat::DRM_PRIME:       return AV_PIX_FMT_DRM_PRIME;
            case PixelFormat::PAL8:            return AV_PIX_FMT_PAL8;
            default:                           return AV_PIX_FMT_NONE;
        }
    };

    inline auto fromFFmpeg(AVPixelFormat fmt) -> PixelFormat {
        switch (fmt) {
            case AV_PIX_FMT_NONE:            return PixelFormat::None;
            case AV_PIX_FMT_YUV420P:         return PixelFormat::YUV420P;
            case AV_PIX_FMT_YUV422P:         return PixelFormat::YUV422P;
            case AV_PIX_FMT_YUV444P:         return PixelFormat::YUV444P;
            case AV_PIX_FMT_YUV410P:         return PixelFormat::YUV410P;
            case AV_PIX_FMT_YUV411P:         return PixelFormat::YUV411P;
            case AV_PIX_FMT_YUV440P:         return PixelFormat::YUV440P;
            // ffmpeg depercated J-Format
            case AV_PIX_FMT_YUVJ420P:         return PixelFormat::YUV420P;
            case AV_PIX_FMT_YUVJ422P:         return PixelFormat::YUV422P;
            case AV_PIX_FMT_YUVJ444P:         return PixelFormat::YUV444P;
            case AV_PIX_FMT_YUVJ411P:         return PixelFormat::YUV411P;
            case AV_PIX_FMT_YUVJ440P:         return PixelFormat::YUV440P;
            case AV_PIX_FMT_NV12:            return PixelFormat::NV12;
            case AV_PIX_FMT_NV21:            return PixelFormat::NV21;
            case AV_PIX_FMT_NV16:            return PixelFormat::NV16;
            case AV_PIX_FMT_NV24:            return PixelFormat::NV24;
            case AV_PIX_FMT_NV42:            return PixelFormat::NV42;
            case AV_PIX_FMT_P010LE:          return PixelFormat::P010LE;
            case AV_PIX_FMT_P010BE:          return PixelFormat::P010BE;
            case AV_PIX_FMT_P012LE:          return PixelFormat::P012LE;
            case AV_PIX_FMT_P012BE:          return PixelFormat::P012BE;
            case AV_PIX_FMT_P016LE:          return PixelFormat::P016LE;
            case AV_PIX_FMT_P016BE:          return PixelFormat::P016BE;
            case AV_PIX_FMT_YUYV422:         return PixelFormat::YUYV422;
            case AV_PIX_FMT_UYVY422:         return PixelFormat::UYVY422;
            case AV_PIX_FMT_YVYU422:         return PixelFormat::YVYU422;
            case AV_PIX_FMT_Y210LE:          return PixelFormat::Y210LE;
            case AV_PIX_FMT_Y210BE:          return PixelFormat::Y210BE;
            case AV_PIX_FMT_RGB24:           return PixelFormat::RGB24;
            case AV_PIX_FMT_BGR24:           return PixelFormat::BGR24;
            case AV_PIX_FMT_RGBA:            return PixelFormat::RGBA;
            case AV_PIX_FMT_BGRA:            return PixelFormat::BGRA;
            case AV_PIX_FMT_ARGB:            return PixelFormat::ARGB;
            case AV_PIX_FMT_ABGR:            return PixelFormat::ABGR;
            case AV_PIX_FMT_RGB565LE:        return PixelFormat::RGB565LE;
            case AV_PIX_FMT_RGB565BE:        return PixelFormat::RGB565BE;
            case AV_PIX_FMT_BGR565LE:        return PixelFormat::BGR565LE;
            case AV_PIX_FMT_BGR565BE:        return PixelFormat::BGR565BE;
            case AV_PIX_FMT_RGB555LE:        return PixelFormat::RGB555LE;
            case AV_PIX_FMT_RGB555BE:        return PixelFormat::RGB555BE;
            case AV_PIX_FMT_BGR555LE:        return PixelFormat::BGR555LE;
            case AV_PIX_FMT_BGR555BE:        return PixelFormat::BGR555BE;
            case AV_PIX_FMT_RGB48LE:         return PixelFormat::RGB48LE;
            case AV_PIX_FMT_RGB48BE:         return PixelFormat::RGB48BE;
            case AV_PIX_FMT_BGR48LE:         return PixelFormat::BGR48LE;
            case AV_PIX_FMT_BGR48BE:         return PixelFormat::BGR48BE;
            case AV_PIX_FMT_RGBA64LE:        return PixelFormat::RGBA64LE;
            case AV_PIX_FMT_RGBA64BE:        return PixelFormat::RGBA64BE;
            case AV_PIX_FMT_BGRA64LE:        return PixelFormat::BGRA64LE;
            case AV_PIX_FMT_BGRA64BE:        return PixelFormat::BGRA64BE;
            case AV_PIX_FMT_X2RGB10LE:       return PixelFormat::X2RGB10LE;
            case AV_PIX_FMT_X2RGB10BE:       return PixelFormat::X2RGB10BE;
            case AV_PIX_FMT_X2BGR10LE:       return PixelFormat::X2BGR10LE;
            case AV_PIX_FMT_X2BGR10BE:       return PixelFormat::X2BGR10BE;
            case AV_PIX_FMT_GBRP:            return PixelFormat::GBRP;
            case AV_PIX_FMT_GBRP10LE:        return PixelFormat::GBRP10LE;
            case AV_PIX_FMT_GBRP10BE:        return PixelFormat::GBRP10BE;
            case AV_PIX_FMT_GBRP12LE:        return PixelFormat::GBRP12LE;
            case AV_PIX_FMT_GBRP12BE:        return PixelFormat::GBRP12BE;
            case AV_PIX_FMT_GBRP16LE:        return PixelFormat::GBRP16LE;
            case AV_PIX_FMT_GBRP16BE:        return PixelFormat::GBRP16BE;
            case AV_PIX_FMT_GBRAP:           return PixelFormat::GBRAP;
            case AV_PIX_FMT_GBRAP16LE:       return PixelFormat::GBRAP16LE;
            case AV_PIX_FMT_GBRAP16BE:       return PixelFormat::GBRAP16BE;
            case AV_PIX_FMT_GRAY8:           return PixelFormat::GRAY8;
            case AV_PIX_FMT_GRAY16LE:        return PixelFormat::GRAY16LE;
            case AV_PIX_FMT_GRAY16BE:        return PixelFormat::GRAY16BE;
            case AV_PIX_FMT_YA8:             return PixelFormat::YA8;
            case AV_PIX_FMT_YA16LE:          return PixelFormat::YA16LE;
            case AV_PIX_FMT_YA16BE:          return PixelFormat::YA16BE;
            case AV_PIX_FMT_YUV420P10LE:     return PixelFormat::YUV420P10LE;
            case AV_PIX_FMT_YUV420P10BE:     return PixelFormat::YUV420P10BE;
            case AV_PIX_FMT_YUV422P10LE:     return PixelFormat::YUV422P10LE;
            case AV_PIX_FMT_YUV422P10BE:     return PixelFormat::YUV422P10BE;
            case AV_PIX_FMT_YUV444P10LE:     return PixelFormat::YUV444P10LE;
            case AV_PIX_FMT_YUV444P10BE:     return PixelFormat::YUV444P10BE;
            case AV_PIX_FMT_YUV420P12LE:     return PixelFormat::YUV420P12LE;
            case AV_PIX_FMT_YUV420P12BE:     return PixelFormat::YUV420P12BE;
            case AV_PIX_FMT_YUV422P12LE:     return PixelFormat::YUV422P12LE;
            case AV_PIX_FMT_YUV422P12BE:     return PixelFormat::YUV422P12BE;
            case AV_PIX_FMT_YUV444P12LE:     return PixelFormat::YUV444P12LE;
            case AV_PIX_FMT_YUV444P12BE:     return PixelFormat::YUV444P12BE;
            case AV_PIX_FMT_YUV420P16LE:     return PixelFormat::YUV420P16LE;
            case AV_PIX_FMT_YUV420P16BE:     return PixelFormat::YUV420P16BE;
            case AV_PIX_FMT_YUV422P16LE:     return PixelFormat::YUV422P16LE;
            case AV_PIX_FMT_YUV422P16BE:     return PixelFormat::YUV422P16BE;
            case AV_PIX_FMT_YUV444P16LE:     return PixelFormat::YUV444P16LE;
            case AV_PIX_FMT_YUV444P16BE:     return PixelFormat::YUV444P16BE;
            case AV_PIX_FMT_YUVA420P:        return PixelFormat::YUVA420P;
            case AV_PIX_FMT_YUVA422P:        return PixelFormat::YUVA422P;
            case AV_PIX_FMT_YUVA444P:        return PixelFormat::YUVA444P;
            case AV_PIX_FMT_YUVA420P10LE:    return PixelFormat::YUVA420P10LE;
            case AV_PIX_FMT_YUVA420P10BE:    return PixelFormat::YUVA420P10BE;
            case AV_PIX_FMT_YUVA422P10LE:    return PixelFormat::YUVA422P10LE;
            case AV_PIX_FMT_YUVA422P10BE:    return PixelFormat::YUVA422P10BE;
            case AV_PIX_FMT_YUVA444P10LE:    return PixelFormat::YUVA444P10LE;
            case AV_PIX_FMT_YUVA444P10BE:    return PixelFormat::YUVA444P10BE;
            case AV_PIX_FMT_YUVA420P16LE:    return PixelFormat::YUVA420P16LE;
            case AV_PIX_FMT_YUVA420P16BE:    return PixelFormat::YUVA420P16BE;
            case AV_PIX_FMT_YUVA422P16LE:    return PixelFormat::YUVA422P16LE;
            case AV_PIX_FMT_YUVA422P16BE:    return PixelFormat::YUVA422P16BE;
            case AV_PIX_FMT_YUVA444P16LE:    return PixelFormat::YUVA444P16LE;
            case AV_PIX_FMT_YUVA444P16BE:    return PixelFormat::YUVA444P16BE;
            case AV_PIX_FMT_BAYER_BGGR8:     return PixelFormat::BAYER_BGGR8;
            case AV_PIX_FMT_BAYER_RGGB8:     return PixelFormat::BAYER_RGGB8;
            case AV_PIX_FMT_BAYER_GBRG8:     return PixelFormat::BAYER_GBRG8;
            case AV_PIX_FMT_BAYER_GRBG8:     return PixelFormat::BAYER_GRBG8;
            case AV_PIX_FMT_BAYER_BGGR16LE:  return PixelFormat::BAYER_BGGR16LE;
            case AV_PIX_FMT_BAYER_BGGR16BE:  return PixelFormat::BAYER_BGGR16BE;
            case AV_PIX_FMT_BAYER_RGGB16LE:  return PixelFormat::BAYER_RGGB16LE;
            case AV_PIX_FMT_BAYER_RGGB16BE:  return PixelFormat::BAYER_RGGB16BE;
            case AV_PIX_FMT_BAYER_GBRG16LE:  return PixelFormat::BAYER_GBRG16LE;
            case AV_PIX_FMT_BAYER_GBRG16BE:  return PixelFormat::BAYER_GBRG16BE;
            case AV_PIX_FMT_BAYER_GRBG16LE:  return PixelFormat::BAYER_GRBG16LE;
            case AV_PIX_FMT_BAYER_GRBG16BE:  return PixelFormat::BAYER_GRBG16BE;
            case AV_PIX_FMT_RGBF32LE:        return PixelFormat::RGBF32LE;
            case AV_PIX_FMT_RGBF32BE:        return PixelFormat::RGBF32BE;
            case AV_PIX_FMT_RGBAF32LE:       return PixelFormat::RGBAF32LE;
            case AV_PIX_FMT_RGBAF32BE:       return PixelFormat::RGBAF32BE;
            case AV_PIX_FMT_GBRPF32LE:       return PixelFormat::GBRPF32LE;
            case AV_PIX_FMT_GBRPF32BE:       return PixelFormat::GBRPF32BE;
            case AV_PIX_FMT_GBRAPF32LE:      return PixelFormat::GBRAPF32LE;
            case AV_PIX_FMT_GBRAPF32BE:      return PixelFormat::GBRAPF32BE;
            case AV_PIX_FMT_VAAPI:           return PixelFormat::VAAPI;
            case AV_PIX_FMT_DXVA2_VLD:       return PixelFormat::DXVA2_VLD;
            case AV_PIX_FMT_D3D11:           return PixelFormat::D3D11;
            case AV_PIX_FMT_D3D12:           return PixelFormat::D3D12;
            case AV_PIX_FMT_QSV:             return PixelFormat::QSV;
            case AV_PIX_FMT_CUDA:            return PixelFormat::CUDA;
            case AV_PIX_FMT_VDPAU:           return PixelFormat::VDPAU;
            case AV_PIX_FMT_VIDEOTOOLBOX:    return PixelFormat::VIDEOTOOLBOX;
            case AV_PIX_FMT_MEDIACODEC:      return PixelFormat::MEDIACODEC;
            case AV_PIX_FMT_VULKAN:          return PixelFormat::VULKAN;
            case AV_PIX_FMT_OPENCL:          return PixelFormat::OPENCL;
            case AV_PIX_FMT_DRM_PRIME:       return PixelFormat::DRM_PRIME;
            case AV_PIX_FMT_PAL8:            return PixelFormat::PAL8;
            default:                         return PixelFormat::None;
        }
    }
} // namespace pixfmt

namespace color_range {
    inline auto toFFmpeg(ColorRange cr) -> AVColorRange {
        switch (cr) {
            case ColorRange::Unknown: return AVCOL_RANGE_UNSPECIFIED;
            case ColorRange::MPEG: return AVCOL_RANGE_MPEG;
            case ColorRange::JPEG: return AVCOL_RANGE_JPEG;
            default: return AVCOL_RANGE_UNSPECIFIED;
        }
    }

    inline auto fromFFmpeg(AVColorRange cr) -> ColorRange {
        switch (cr) {
            case AVCOL_RANGE_UNSPECIFIED: return ColorRange::Unknown;
            case AVCOL_RANGE_MPEG: return ColorRange::MPEG;
            case AVCOL_RANGE_JPEG: return ColorRange::JPEG;
            default: return ColorRange::Unknown;
        }
    }
} // namespace color_range

namespace color_primaries {
    inline auto toFFmpeg(ColorPrimaries cp) -> AVColorPrimaries {
        switch (cp) {
            case ColorPrimaries::Unknown: return AVCOL_PRI_UNSPECIFIED;
            case ColorPrimaries::BT709: return AVCOL_PRI_BT709;
            case ColorPrimaries::BT470M: return AVCOL_PRI_BT470M;
            case ColorPrimaries::BT470BG: return AVCOL_PRI_BT470BG;
            case ColorPrimaries::SMPTE170M: return AVCOL_PRI_SMPTE170M;
            case ColorPrimaries::SMPTE240M: return AVCOL_PRI_SMPTE240M;
            case ColorPrimaries::FILM: return AVCOL_PRI_FILM;
            case ColorPrimaries::BT2020: return AVCOL_PRI_BT2020;
            case ColorPrimaries::SMPTE428: return AVCOL_PRI_SMPTE428;
            case ColorPrimaries::SMPTE431: return AVCOL_PRI_SMPTE431;
            case ColorPrimaries::SMPTE432: return AVCOL_PRI_SMPTE432;
            case ColorPrimaries::JEDEC_P22: return AVCOL_PRI_JEDEC_P22;
            default: return AVCOL_PRI_UNSPECIFIED;
        }
    }

    inline auto fromFFmpeg(AVColorPrimaries cp) -> ColorPrimaries {
        switch (cp) {
            case AVCOL_PRI_UNSPECIFIED: return ColorPrimaries::Unknown;
            case AVCOL_PRI_BT709: return ColorPrimaries::BT709;
            case AVCOL_PRI_BT470M: return ColorPrimaries::BT470M;
            case AVCOL_PRI_BT470BG: return ColorPrimaries::BT470BG;
            case AVCOL_PRI_SMPTE170M: return ColorPrimaries::SMPTE170M;
            case AVCOL_PRI_SMPTE240M: return ColorPrimaries::SMPTE240M;
            case AVCOL_PRI_FILM: return ColorPrimaries::FILM;
            case AVCOL_PRI_BT2020: return ColorPrimaries::BT2020;
            case AVCOL_PRI_SMPTE428: return ColorPrimaries::SMPTE428;
            case AVCOL_PRI_SMPTE431: return ColorPrimaries::SMPTE431;
            case AVCOL_PRI_SMPTE432: return ColorPrimaries::SMPTE432;
            case AVCOL_PRI_JEDEC_P22: return ColorPrimaries::JEDEC_P22;
            default: return ColorPrimaries::Unknown;
        }
    }
} // namespace color_primaries

namespace color_transfer {
    inline auto toFFmpeg(ColorTransfer ct) -> AVColorTransferCharacteristic {
        switch (ct) {
            case ColorTransfer::Unknown: return AVCOL_TRC_UNSPECIFIED;
            case ColorTransfer::BT709: return AVCOL_TRC_BT709;
            case ColorTransfer::Gamma22: return AVCOL_TRC_GAMMA22;
            case ColorTransfer::Gamma28: return AVCOL_TRC_GAMMA28;
            case ColorTransfer::SMPTE170M: return AVCOL_TRC_SMPTE170M;
            case ColorTransfer::SMPTE240M: return AVCOL_TRC_SMPTE240M;
            case ColorTransfer::Linear: return AVCOL_TRC_LINEAR;
            case ColorTransfer::LOG: return AVCOL_TRC_LOG;
            case ColorTransfer::LOG_SQRT: return AVCOL_TRC_LOG_SQRT;
            case ColorTransfer::IEC61966_2_1: return AVCOL_TRC_IEC61966_2_1;
            case ColorTransfer::BT2020_10: return AVCOL_TRC_BT2020_10;
            case ColorTransfer::BT2020_12: return AVCOL_TRC_BT2020_12;
            case ColorTransfer::SMPTE2084: return AVCOL_TRC_SMPTE2084;
            case ColorTransfer::ARIB_STD_B67: return AVCOL_TRC_ARIB_STD_B67;
            default: return AVCOL_TRC_UNSPECIFIED;
        }
    }

    inline auto fromFFmpeg(AVColorTransferCharacteristic ct) -> ColorTransfer {
        switch (ct) {
            case AVCOL_TRC_UNSPECIFIED: return ColorTransfer::Unknown;
            case AVCOL_TRC_BT709: return ColorTransfer::BT709;
            case AVCOL_TRC_GAMMA22: return ColorTransfer::Gamma22;
            case AVCOL_TRC_GAMMA28: return ColorTransfer::Gamma28;
            case AVCOL_TRC_SMPTE170M: return ColorTransfer::SMPTE170M;
            case AVCOL_TRC_SMPTE240M: return ColorTransfer::SMPTE240M;
            case AVCOL_TRC_LINEAR: return ColorTransfer::Linear;
            case AVCOL_TRC_LOG: return ColorTransfer::LOG;
            case AVCOL_TRC_LOG_SQRT: return ColorTransfer::LOG_SQRT;
            case AVCOL_TRC_IEC61966_2_1: return ColorTransfer::IEC61966_2_1;
            case AVCOL_TRC_BT2020_10: return ColorTransfer::BT2020_10;
            case AVCOL_TRC_BT2020_12: return ColorTransfer::BT2020_12;
            case AVCOL_TRC_SMPTE2084: return ColorTransfer::SMPTE2084;
            case AVCOL_TRC_ARIB_STD_B67: return ColorTransfer::ARIB_STD_B67;
            default: return ColorTransfer::Unknown;
        }
    }
} // namespace color_transfer

namespace color_space {
    inline auto toFFmpeg(ColorSpace cs) -> AVColorSpace {
        switch (cs) {
            case ColorSpace::RGB: return AVCOL_SPC_RGB;
            case ColorSpace::BT709: return AVCOL_SPC_BT709;
            case ColorSpace::Undefined: return AVCOL_SPC_UNSPECIFIED;
            case ColorSpace::FCC: return AVCOL_SPC_FCC;
            case ColorSpace::BT470BG: return AVCOL_SPC_BT470BG;
            case ColorSpace::SMPTE170M: return AVCOL_SPC_SMPTE170M;
            case ColorSpace::SMPTE240M: return AVCOL_SPC_SMPTE240M;
            case ColorSpace::YCGCO: return AVCOL_SPC_YCGCO;
            case ColorSpace::BT2020_NCL: return AVCOL_SPC_BT2020_NCL;
            case ColorSpace::BT2020_CL: return AVCOL_SPC_BT2020_CL;
            case ColorSpace::SMPTE2085: return AVCOL_SPC_SMPTE2085;
            case ColorSpace::CHROMA_DERIVED_NCL: return AVCOL_SPC_CHROMA_DERIVED_NCL;
            case ColorSpace::CHROMA_DERIVED_CL: return AVCOL_SPC_CHROMA_DERIVED_CL;
            case ColorSpace::ICTCP: return AVCOL_SPC_ICTCP;
            default: return AVCOL_SPC_UNSPECIFIED;
        }
    }

    inline auto fromFFmpeg(AVColorSpace cs) -> ColorSpace {
        switch (cs) {
            case AVCOL_SPC_RGB: return ColorSpace::RGB;
            case AVCOL_SPC_BT709: return ColorSpace::BT709;
            case AVCOL_SPC_UNSPECIFIED: return ColorSpace::Undefined;
            case AVCOL_SPC_FCC: return ColorSpace::FCC;
            case AVCOL_SPC_BT470BG: return ColorSpace::BT470BG;
            case AVCOL_SPC_SMPTE170M: return ColorSpace::SMPTE170M;
            case AVCOL_SPC_SMPTE240M: return ColorSpace::SMPTE240M;
            case AVCOL_SPC_YCGCO: return ColorSpace::YCGCO;
            case AVCOL_SPC_BT2020_NCL: return ColorSpace::BT2020_NCL;
            case AVCOL_SPC_BT2020_CL: return ColorSpace::BT2020_CL;
            case AVCOL_SPC_SMPTE2085: return ColorSpace::SMPTE2085;
            case AVCOL_SPC_CHROMA_DERIVED_NCL: return ColorSpace::CHROMA_DERIVED_NCL;
            case AVCOL_SPC_CHROMA_DERIVED_CL: return ColorSpace::CHROMA_DERIVED_CL;
            case AVCOL_SPC_ICTCP: return ColorSpace::ICTCP;
            default: return ColorSpace::RGB;
        }
    }
} // namespace color_space

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

namespace logger {

#if defined(NEKOAV_NO_LOG)
    inline auto info(auto &&...) -> void {}
    inline auto error(auto &&...) -> void {}
    inline auto warn(auto &&...) -> void {}
    inline auto debug(auto &&...) -> void {}
    inline auto verbose(auto &&...) -> void {}
#else
    template <typename... Args>
    inline auto doLog(int level, std::format_string<Args...> fmt, Args &&...args) -> void {
        ::av_log(nullptr, level, "%s\n", std::format(fmt, std::forward<Args>(args)...).c_str());
    }

    template <typename... Args>
    inline auto info(std::format_string<Args...> fmt, Args &&...args) -> void {
        doLog(AV_LOG_INFO, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline auto error(std::format_string<Args...> fmt, Args &&...args) -> void {
        doLog(AV_LOG_ERROR, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline auto warn(std::format_string<Args...> fmt, Args &&...args) -> void {
        doLog(AV_LOG_WARNING, fmt, std::forward<Args>(args)...);
    }
    
    template <typename... Args>
    inline auto debug(std::format_string<Args...> fmt, Args &&...args) -> void {
        doLog(AV_LOG_DEBUG, fmt, std::forward<Args>(args)...);
    }
#endif // NEKOAV_NO_LOG

} // namespace logger


namespace error {
    inline auto fromFFmpeg(int err) -> std::error_code {
        switch (err) {
            case 0: return Error::Ok;
            case AVERROR_DECODER_NOT_FOUND: return Error::NoCodec;
            case AVERROR_STREAM_NOT_FOUND:  return Error::NoStream;
            default:                        return Error::FFmpeg; // from ffmpeg
        }
    }

    inline auto toString(int err) -> std::string {
        char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
        return av_make_error_string(buf, sizeof(buf), err);
    }
} // namespace error

} // namespace nekoav