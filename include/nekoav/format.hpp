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

    _Max,    // The number of pixel formats, don't use this in code.
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

    _Max, // The number of sample formats, don't use this in code.
};

extern NEKOAV_API auto toString(PixelFormat fmt) -> std::string_view;
extern NEKOAV_API auto toString(SampleFormat fmt) -> std::string_view;

} // namespace nekoav