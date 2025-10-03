#pragma once

#include <nekoav/defines.hpp>
#include <bit>

namespace nekoav {

/**
 * @brief Pixel Format, as same as FFmpeg
 * 
 */
enum class PixelFormat : int {
    None    = -1,
    YUV420P ,
    YUV422P ,
    YUV444P ,
    YUV410P ,
    YUV411P ,
    UYVY422 ,
    UYYVYY411,
    BGR8,
    BGR4,
    BGR4_BYTE,
    RGB8,
    RGB4,
    NV12,
    NV21,

    RGBA,     // R8G8B8A8
    BGRA,
    ARGB,

    RGBA64LE, // R16 G16 B16 A16
    RGBA64BE, // R16 G16 B16 A16

    P010LE,
    P010BE,

    DXVA2,  // AV_PIX_FMT_DXVA2_VLD in ffmpeg, data[3] contains LPDIRECT3DSURFACE9
    D3D11,  // AV_PIX_FMT_D3D11 in ffmpeg, data[0] contains a ID3D11Texture2D, data[1] contains intptr_t
    VDPAU,  // AV_PIX_FMT_VDPAU in ffmpeg, data[3] contains a VdpVideoSurface
    VAAPI,  // AV_PIX_FMT_VAAPI in ffmpeg, data[3] contains a VASurfaceID
    OpenCL, // AV_PIX_FMT_OPENCL in ffmpeg, data[i] contains a cl_mem
};

/**
 * @brief The audio sample format, as same as FFmpeg
 * 
 */
enum class SampleFormat : int {
    None = -1,
    U8 ,
    S16,
    S32,
    FLT,
    DBL,

    U8P ,
    S16P,
    S32P,
    FLTP,
    DBLP,
};

inline auto toString(PixelFormat fmt) -> std::string_view {
    switch (fmt) {
        case PixelFormat::None: return "None";
        case PixelFormat::YUV420P: return "YUV420P";
        case PixelFormat::YUV422P: return "YUV422P";
        case PixelFormat::YUV444P: return "YUV444P";
        case PixelFormat::YUV410P: return "YUV410P";
        case PixelFormat::YUV411P: return "YUV411P";
        case PixelFormat::UYVY422: return "UYVY422";
        case PixelFormat::UYYVYY411: return "UYYVYY411";
        case PixelFormat::BGR8: return "BGR8";
        case PixelFormat::BGR4: return "BGR4";
        case PixelFormat::BGR4_BYTE: return "BGR4_BYTE";
        case PixelFormat::RGB8: return "RGB8";
        case PixelFormat::RGB4: return "RGB4";
        case PixelFormat::NV12: return "NV12";
        case PixelFormat::NV21: return "NV21";

        case PixelFormat::RGBA: return "RGBA";
        case PixelFormat::BGRA: return "BGRA";
        case PixelFormat::ARGB: return "ARGB";

        case PixelFormat::RGBA64LE: return "RGBA64LE";
        case PixelFormat::RGBA64BE: return "RGBA64BE";

        case PixelFormat::P010LE: return "P010LE";
        case PixelFormat::P010BE: return "P010BE";

        case PixelFormat::DXVA2: return "DXVA2";
        case PixelFormat::D3D11: return "D3D11";
        case PixelFormat::VDPAU: return "VDPAU";
        case PixelFormat::VAAPI: return "VAAPI";
        case PixelFormat::OpenCL: return "OpenCL";

        default: return "Unknown";
    }
}

inline auto toString(SampleFormat fmt) -> std::string_view {
    switch (fmt) {
        case SampleFormat::None: return "None";
        case SampleFormat::U8: return "U8";
        case SampleFormat::S16: return "S16";
        case SampleFormat::S32: return "S32";
        case SampleFormat::FLT: return "FLT";
        case SampleFormat::DBL: return "DBL";

        case SampleFormat::U8P: return "U8P";
        case SampleFormat::S16P: return "S16P";
        case SampleFormat::S32P: return "S32P";
        case SampleFormat::FLTP: return "FLTP";
        case SampleFormat::DBLP: return "DBLP";

        default: return "Unknown";
    }
}

} // namespace nekoav