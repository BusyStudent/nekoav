#pragma once

#include <nekoav/defines.hpp>
#include <bit>

namespace nekoav {

/**
 * @brief Pixel Format, as same as FFmpeg
 * 
 */
enum class PixelFormat : int {
    None = -1,

    // YUV Planar Formats
    YUV420P,      // 4:2:0, 12 bpp
    YUV422P,      // 4:2:2, 16 bpp
    YUV444P,      // 4:4:4, 24 bpp
    YUV410P,      // 4:1:0, 9 bpp
    YUV411P,      // 4:1:1, 12 bpp
    YUV440P,      // 4:4:0, 16 bpp

    // YUV Semi-Planar Formats
    NV12,         // 4:2:0, 12 bpp, Y-plane, then UV-plane
    NV21,         // 4:2:0, 12 bpp, Y-plane, then VU-plane
    NV16,         // 4:2:2, 16 bpp, Y-plane, then UV-plane
    NV24,         // 4:4:4, 24 bpp, Y-plane, then UV-plane
    NV42,         // 4:4:4, 24 bpp, Y-plane, then VU-plane
    P010LE, P010BE, // 10-bit NV12
    P012LE, P012BE, // 12-bit NV12
    P016LE, P016BE, // 16-bit NV12

    // UV Packed Formats
    YUYV422,      // Y0 Cb Y1 Cr
    UYVY422,      // Cb Y0 Cr Y1
    YVYU422,      // Y0 Cr Y1 Cb
    Y210LE, Y210BE, // 10-bit packed 4:2:2

    // RGB Packed Formats 
    RGB24,        // R, G, B, 24 bpp
    BGR24,        // B, G, R, 24 bpp
    RGBA,         // R, G, B, A, 32 bpp
    BGRA,         // B, G, R, A, 32 bpp
    ARGB,         // A, R, G, B, 32 bpp
    ABGR,         // A, B, G, R, 32 bpp
    RGB565LE, RGB565BE,
    BGR565LE, BGR565BE,
    RGB555LE, RGB555BE,
    BGR555LE, BGR555BE,
    RGB48LE,  RGB48BE,   // 16-bit per component
    BGR48LE,  BGR48BE,   // 16-bit per component
    RGBA64LE, RGBA64BE,  // 16-bit per component
    BGRA64LE, BGRA64BE,  // 16-bit per component
    X2RGB10LE, X2RGB10BE, // 10-bit per component with padding
    X2BGR10LE, X2BGR10BE,

    // RGB Planar Formats
    GBRP,
    GBRP10LE, GBRP10BE,
    GBRP12LE, GBRP12BE,
    GBRP16LE, GBRP16BE,
    GBRAP,
    GBRAP16LE, GBRAP16BE,

    // Grayscale & Alpha Formats
    GRAY8,
    GRAY16LE, GRAY16BE, 
    YA8,
    YA16LE, YA16BE,

    // High Bit Depth YUV
    YUV420P10LE, YUV420P10BE,
    YUV422P10LE, YUV422P10BE,
    YUV444P10LE, YUV444P10BE,
    YUV420P12LE, YUV420P12BE,
    YUV422P12LE, YUV422P12BE,
    YUV444P12LE, YUV444P12BE,
    YUV420P16LE, YUV420P16BE,
    YUV422P16LE, YUV422P16BE,
    YUV444P16LE, YUV444P16BE,

    // YUV with Alpha
    YUVA420P,
    YUVA422P,
    YUVA444P,
    YUVA420P10LE, YUVA420P10BE,
    YUVA422P10LE, YUVA422P10BE,
    YUVA444P10LE, YUVA444P10BE,
    YUVA420P16LE, YUVA420P16BE,
    YUVA422P16LE, YUVA422P16BE,
    YUVA444P16LE, YUVA444P16BE,

    // Raw Sensor Formats
    BAYER_BGGR8, BAYER_RGGB8, BAYER_GBRG8, BAYER_GRBG8,
    BAYER_BGGR16LE, BAYER_BGGR16BE,
    BAYER_RGGB16LE, BAYER_RGGB16BE,
    BAYER_GBRG16LE, BAYER_GBRG16BE,
    BAYER_GRBG16LE, BAYER_GRBG16BE,
    
    // Float
    RGBF32LE, RGBF32BE,
    RGBAF32LE, RGBAF32BE,
    GBRPF32LE, GBRPF32BE,
    GBRAPF32LE, GBRAPF32BE,

    // Hardware Formats
    VAAPI,
    DXVA2_VLD,
    D3D11,
    D3D12,
    QSV,
    CUDA,
    VDPAU,
    VIDEOTOOLBOX,
    MEDIACODEC,
    VULKAN,
    OPENCL,
    DRM_PRIME,

    // Other
    PAL8,

    // Don't use
    Nax
};

/**
 * @brief The Visual Color Range (AVColorRange)
 */
enum class ColorRange : uint8_t {
    Unknown = 0, // AVCOL_RANGE_UNSPECIFIED
    MPEG,        // Limited range (16-235 for Y, 16-240 for UV)
    JPEG,        // Full range (0-255)

    // Alias
    Limited = MPEG,
    Full    = JPEG,
    
    // Don't use
    Nax
};

/**
 * @brief Color Primaries (AVColorPrimaries)
 */
enum class ColorPrimaries : uint8_t {
    Unknown = 0,     // AVCOL_PRI_RESERVED0 / AVCOL_PRI_UNSPECIFIED
    BT709,           // HDTV (Rec.709) - Most common for HD
    BT470M,
    BT470BG,         // PAL/SECAM
    SMPTE170M,       // NTSC (Rec.601)
    SMPTE240M,
    FILM,
    BT2020,          // UHDTV (Rec.2020) - 4K/8K
    SMPTE428,
    SMPTE431,        // DCI-P3
    SMPTE432,        // P3-D65 (Display P3)
    JEDEC_P22,

    // Don't use
    Nax
};

/**
 * @brief Color Transfer Characteristics (AVColorTransferCharacteristic)
 */
enum class ColorTransfer : uint8_t {
    Unknown = 0,    // Unspecified
    BT709,          // Rec.709 Gamma
    Gamma22,        // Gamma 2.2
    Gamma28,        // Gamma 2.8
    SMPTE170M,
    SMPTE240M,
    Linear,         // Linear transfer
    LOG,
    LOG_SQRT,
    IEC61966_2_1,   // sRGB
    BT2020_10,      // Rec.2020 10-bit
    BT2020_12,      // Rec.2020 12-bit
    SMPTE2084,      // PQ (Perceptual Quantizer) - HDR10
    ARIB_STD_B67,   // HLG (Hybrid Log-Gamma) - HDR HLG

    // Don't use
    Nax
};

/**
 * @brief Color Space Coefficients (AVColorSpace) 
 */
enum class ColorSpace : uint8_t {
    RGB = 0,         // GBR, No matrix (e.g. RGB24)
    BT709,           // Rec.709
    Undefined,       // Unspecified
    FCC,
    BT470BG,         // PAL/SECAM (Rec.601)
    SMPTE170M,       // NTSC (Rec.601)
    SMPTE240M,
    YCGCO,
    BT2020_NCL,      // Rec.2020 Non-constant Luminance (Standard 4K YUV)
    BT2020_CL,       // Rec.2020 Constant Luminance
    SMPTE2085,
    CHROMA_DERIVED_NCL,
    CHROMA_DERIVED_CL,
    ICTCP,           // Used in Dolby Vision

    // Don't use
    Nax
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

    // Don't use
    Nax
};

/**
 * @brief Check the sample format is planar or not
 * 
 * @param fmt 
 * @return true 
 * @return false 
 */
inline auto isPlanarFormat(SampleFormat fmt) -> bool {
    switch (fmt) {
        case SampleFormat::U8P:
        case SampleFormat::S16P:
        case SampleFormat::S32P:
        case SampleFormat::FLTP:
        case SampleFormat::DBLP:
            return true;
        default: return false;
    }
}

/**
 * @brief Get the bytes per sample
 * 
 * @param fmt 
 * @return size_t 
 */
inline auto bytesPerSample(SampleFormat fmt) -> size_t {
    switch (fmt) {
        case SampleFormat::U8: case SampleFormat::U8P: return 1;
        case SampleFormat::S16: case SampleFormat::S16P: return 2;
        case SampleFormat::S32: case SampleFormat::S32P: return 4;
        case SampleFormat::FLT: case SampleFormat::FLTP: return 4;
        case SampleFormat::DBL: case SampleFormat::DBLP: return 8;
        default: return 0;
    }
}

extern NEKOAV_API auto toString(PixelFormat fmt) -> std::string_view;
extern NEKOAV_API auto toString(ColorRange range) -> std::string_view;
extern NEKOAV_API auto toString(ColorPrimaries pri) -> std::string_view;
extern NEKOAV_API auto toString(ColorTransfer trans) -> std::string_view;
extern NEKOAV_API auto toString(ColorSpace space) -> std::string_view;
extern NEKOAV_API auto toString(SampleFormat fmt) -> std::string_view;

} // namespace nekoav

// Formatter
NEKOAV_FORMATTER_4(nekoav::PixelFormat);
NEKOAV_FORMATTER_4(nekoav::ColorRange);
NEKOAV_FORMATTER_4(nekoav::ColorPrimaries);
NEKOAV_FORMATTER_4(nekoav::ColorTransfer);
NEKOAV_FORMATTER_4(nekoav::ColorSpace);
NEKOAV_FORMATTER_4(nekoav::SampleFormat);