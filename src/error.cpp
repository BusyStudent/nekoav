#include <nekoav/defines.hpp>
#include <nekoav/error.hpp>

namespace nekoav {

class ErrorCategory final : public std::error_category {
public:
    constexpr ErrorCategory() {}

    auto name() const noexcept -> const char * override;
    auto message(int code) const -> std::string override;
};

auto ErrorCategory::name() const noexcept -> const char * {
    return "nekoav";
}

auto ErrorCategory::message(int code) const -> std::string {
    static auto constexpr array = []() consteval {
        constexpr size_t N = static_cast<size_t>(Error::Unknown) + 1;
        std::array<std::string_view, N> arr{};
        arr.fill("Error code message missing");

        arr[static_cast<size_t>(Error::Ok)]                      = "No error";
        arr[static_cast<size_t>(Error::NotLinked)]               = "Pad is not linked";
        arr[static_cast<size_t>(Error::NoPushCallback)]          = "Pad push callback is not set";
        arr[static_cast<size_t>(Error::InvalidTopology)]         = "Invalid pipeline topology";

        arr[static_cast<size_t>(Error::NotNegotiated)]           = "Pad is not negotiated";

        arr[static_cast<size_t>(Error::NoImpl)]                  = "Function not implemented";
        arr[static_cast<size_t>(Error::InvalidArguments)]        = "Invalid arguments";
        arr[static_cast<size_t>(Error::InvalidState)]            = "Invalid state transition";
        arr[static_cast<size_t>(Error::InvalidContext)]          = "Invalid context";
        arr[static_cast<size_t>(Error::InBusy)]                  = "This method can't be called cocurently";

        arr[static_cast<size_t>(Error::NoStream)]                = "No media stream found";
        arr[static_cast<size_t>(Error::NoCodec)]                 = "No codec found";
        arr[static_cast<size_t>(Error::MediaFormatNotSupported)]  = "Media format not supported";
        arr[static_cast<size_t>(Error::PixelFormatNotSupported)]  = "Pixel format not supported";
        arr[static_cast<size_t>(Error::AudioFormatNotSupported)]  = "Audio format not supported";
        arr[static_cast<size_t>(Error::SampleTypeNotSupported)]   = "Sample type not supported";

        arr[static_cast<size_t>(Error::OutOfMemory)]             = "Out of memory";
        arr[static_cast<size_t>(Error::FileNotFound)]            = "File not found";
        arr[static_cast<size_t>(Error::FileCorrupted)]           = "File corrupted";

        arr[static_cast<size_t>(Error::Internal)]                = "Internal error";
        arr[static_cast<size_t>(Error::External)]                = "External library error";
        arr[static_cast<size_t>(Error::FFmpeg)]                  = "External error from ffmpeg";
        arr[static_cast<size_t>(Error::Unknown)]                 = "Unknown error";
        return arr;
    }();
    if (code < 0 || code >= array.size()) {
        return "Invalid error code";
    }
    return std::string(array[code]);
}

auto make_error_code(Error err) noexcept -> std::error_code {
    static constinit ErrorCategory category;
    return {static_cast<int>(err), category};
}

} // namespace nekoav
