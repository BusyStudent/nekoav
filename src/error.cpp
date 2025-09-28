#include <nekoav/defines.hpp>
#include <nekoav/error.hpp>

namespace nekoav {

auto ErrorCategory::name() const noexcept -> const char * {
    return "nekoav";
}

auto ErrorCategory::message(int code) const -> std::string {
    static auto const array = []() consteval {
        constexpr size_t N = static_cast<size_t>(Error::Unknown) + 1;
        std::array<std::string_view, N> arr{};

        arr[static_cast<size_t>(Error::Ok)]                      = "No error";
        arr[static_cast<size_t>(Error::NoLink)]                  = "Pad is not linked";
        arr[static_cast<size_t>(Error::InvalidTopology)]         = "Invalid pipeline topology";

        arr[static_cast<size_t>(Error::NoImpl)]                  = "Function not implemented";
        arr[static_cast<size_t>(Error::InvalidArguments)]        = "Invalid arguments";
        arr[static_cast<size_t>(Error::InvalidState)]            = "Invalid state transition";
        arr[static_cast<size_t>(Error::InvalidContext)]          = "Invalid context";

        arr[static_cast<size_t>(Error::NoStream)]                = "No media stream found";
        arr[static_cast<size_t>(Error::NoCodec)]                 = "No codec found";
        arr[static_cast<size_t>(Error::UnsupportedMediaFormat)]  = "Unsupported media format";
        arr[static_cast<size_t>(Error::UnsupportedPixelFormat)]  = "Unsupported pixel format";
        arr[static_cast<size_t>(Error::UnsupportedSampleFormat)] = "Unsupported sample format";
        arr[static_cast<size_t>(Error::UnsupportedResource)]     = "Unsupported resource type";

        arr[static_cast<size_t>(Error::OutOfMemory)]             = "Out of memory";
        arr[static_cast<size_t>(Error::FileNotFound)]            = "File not found";
        arr[static_cast<size_t>(Error::FileCorrupted)]           = "File corrupted";

        arr[static_cast<size_t>(Error::Internal)]                = "Internal error";
        arr[static_cast<size_t>(Error::External)]                = "External library error";
        arr[static_cast<size_t>(Error::Unknown)]                 = "Unknown error";
        return arr;
    }();
    if (code < 0 || code > array.size()) {
        return "Invalid error code";
    }
    return std::string(array[code]);
}

auto make_error_code(Error err) noexcept -> std::error_code {
    static constinit ErrorCategory category;
    return {static_cast<int>(err), category};
}

} // namespace nekoav
