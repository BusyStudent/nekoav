#pragma once

#include <nekoav/defines.hpp>
#include <system_error> // std::error_code

namespace nekoav {

class ErrorCategory final : public std::error_category {
public:
    constexpr ErrorCategory() {}

    auto name() const noexcept -> const char * override;
    auto message(int code) const -> std::string override;
};

enum class Error : int {
    Ok = 0,                  //< No error

    // Topology / Pad
    NotLinked,               //< Pad is not linked
    NoPushCallback,          //< Pad push callback is not set
    InvalidTopology,         //< Invalid pipeline topology

    // Negotiate
    NotNegotiated,           //< Pad failed to negotiate

    // User / API misuse
    NoImpl,                  //< Function not implemented
    InvalidArguments,        //< Invalid arguments
    InvalidState,            //< Invalid state transition
    InvalidContext,          //< Invalid context

    // Media related
    NoStream,                //< No media stream found
    NoCodec,                 //< No codec found
    MediaFormatNotSupported, //< Media format not supported
    PixelFormatNotSupported, //< Pixel format not supported
    AudioFormatNotSupported, //< Audio format not supported
    SampleTypeNotSupported,  //< Sample type not supported

    // Resource / system
    OutOfMemory,             //< Out of memory
    FileNotFound,            //< File not found
    FileCorrupted,           //< File corrupted

    // Internal / external
    Internal,                //< Internal error
    External,                //< External library error
    FFmpeg,                  //< FFmpeg error
    Unknown,                 //< Unknown error
};

/**
 * @brief Make an error code of the Error
 * 
 * @param err 
 * @return std::error_code 
 */
extern NEKOAV_API auto make_error_code(Error err) noexcept -> std::error_code;

} // namespace nekoav

template <>
struct std::is_error_code_enum<nekoav::Error> : public std::true_type {};