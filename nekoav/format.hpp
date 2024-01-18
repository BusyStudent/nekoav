#pragma once

#include "defs.hpp"
#include <cstdint>
#include <utility>
#include <bit>

NEKO_NS_BEGIN

/**
 * @brief Internal for select native endiain
 * 
 * @param big 
 * @param little 
 * @return constexpr int 
 */
template <typename T>
constexpr T _NativeEndian(T big, T little) noexcept {
    return std::endian::native == std::endian::big ? big : little;
}

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

    RGBA, //< R8G8B8A8
    BGRA,
    ARGB,

    RGBA64LE, //< R16 G16 B16 A16
    RGBA64BE, //< R16 G16 B16 A16

    P010LE,
    P010BE,

    DXVA2, //< AV_PIX_FMT_DXVA2_VLD in ffmpeg, data[3] contains LPDIRECT3DSURFACE9
    D3D11, //< AV_PIX_FMT_D3D11 in ffmpeg, data[0] contains a ID3D11Texture2D, data[1] contains intptr_t
    VDPAU, //< AV_PIX_FMT_VDPAU in ffmpeg, data[3] contains a VdpVideoSurface
    VAAPI, //< AV_PIX_FMT_VAAPI in ffmpeg, data[3] contains a VASurfaceID
    OpenCL, //< AV_PIX_FMT_OPENCL in ffmpeg, data[i] contains a cl_mem

    RGBA64 = _NativeEndian(RGBA64BE, RGBA64LE),
    P010   = _NativeEndian(P010BE  , P010LE),
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

/**
 * @brief Check if the given sample format is planar.
 *
 * @param fmt the sample format to check
 *
 * @return true if the sample format is planar, false otherwise
 *
 * @throws None
 */
constexpr bool IsSampleFormatPlanar(SampleFormat fmt) noexcept {
    return fmt == SampleFormat::U8P || fmt == SampleFormat::S16P || 
          fmt == SampleFormat::S32P || fmt == SampleFormat::FLTP || fmt == SampleFormat::DBLP;
}
/**
 * @brief Calculates the number of bytes per sample based on the given sample format.
 *
 * @param fmt the sample format for which to calculate the bytes per sample
 *
 * @return the number of bytes per sample
 *
 * @throws None
 */
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
/**
 * @brief Calculates the number of bytes per frame based on the sample format and number of channels.
 *
 * @param fmt the sample format
 * @param channels the number of audio channels
 *
 * @return the number of bytes per frame
 *
 * @throws none
 */
constexpr size_t GetBytesPerFrame(SampleFormat fmt, int channels) noexcept {
    return GetBytesPerSample(fmt) * channels;
}
/**
 * @brief Returns the packed sample format corresponding to the given sample format.
 *
 * @param fmt the sample format to be packed
 *
 * @return the packed sample format
 * 
 * @throws none
 */
constexpr SampleFormat GetPackedSampleFormat(SampleFormat fmt) noexcept {
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
/**
 * @brief Retrieves the planar sample format corresponding to the given sample format.
 *
 * @param fmt the sample format to convert
 *
 * @return the planar sample format
 *
 * @throws none
 */
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

/**
 * @brief Check if the given pixel format is hardware
 * 
 * @param fmt 
 * @return true 
 * @return false 
 */
constexpr bool IsHardwarePixelFormat(PixelFormat fmt) noexcept {
    constexpr PixelFormat hwfmts [] {
        PixelFormat::OpenCL,
        PixelFormat::D3D11,
        PixelFormat::DXVA2,
        PixelFormat::VAAPI,
        PixelFormat::VDPAU,
    };
    return std::find(std::begin(hwfmts), std::end(hwfmts), fmt) != std::end(hwfmts);
}

NEKO_NS_END