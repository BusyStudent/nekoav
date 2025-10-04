#include <nekoav/format.hpp>
#include <string_view>
#include <utility>
#include <string>
#include <tuple>
#include <array>

namespace nekoav {
namespace refl {

/**
 * @brief Get the name of the type in compile time
 * 
 * @tparam T 
 * @return std::string_view 
 */
template <auto T>
consteval auto nameof() {
#ifdef _MSC_VER
    constexpr std::string_view name(__FUNCSIG__);
    constexpr size_t nsEnd = name.find_last_of("::");
    constexpr size_t end = name.find('>', nsEnd);
    // size_t dotBegin = name.find_first_of(',');
    // size_t end = name.find_last_of('>');
    // return name.substr(dotBegin + 1, end - dotBegin - 1);
    return name.substr(nsEnd + 1, end - nsEnd - 1);
#else
    constexpr std::string_view name(__PRETTY_FUNCTION__);
    constexpr size_t eqBegin = name.find_last_of(' ');
    constexpr size_t end = name.find_last_of(']');
    constexpr std::string_view str = name.substr(eqBegin + 1, end - eqBegin - 1);
    // Try skip the namespace ::
    constexpr size_t delim = str.find_last_of("::");
    if (delim != std::string_view::npos) {
        return str.substr(delim + 1);
    }
    return str;
#endif
}

/**
 * @brief Get the array of the name of the type in compile time
 * 
 * @tparam T 
 * @return std::array<char,? + 1> (has null terminator)
 */
template <auto T>
consteval auto nameof2() {
    constexpr auto name = nameof<T>();
    std::array<char, name.size() + 1> buffer {0};
    for (size_t i = 0; i < name.size(); ++i) {
        // To lower case, as same as ffmpeg
        if (name[i] >= 'A' && name[i] <= 'Z') {
            buffer[i] = name[i] + 'a' - 'A';
        }
        else {
            buffer[i] = name[i];
        }
    }
    return buffer;
}

template <size_t ...N, typename T>
auto enum2str(std::index_sequence<N...>, T i) -> std::string_view {
    constexpr static auto data = std::tuple {
        refl::nameof2<T(N)>()...
    };
    constexpr std::array<std::string_view, sizeof...(N)> table {
        std::string_view(
            std::get<N>(data).data(),
            std::get<N>(data).size() - 1 // remove null terminator
        )...
    };
    auto idx = static_cast<int64_t>(i);
    if (idx < 0 || idx >= int64_t(table.size())) {
        return "Unknown";
    }
    return table[idx];
}

} // namespace nekoav::refl

auto toString(PixelFormat fmt) -> std::string_view {
    return refl::enum2str(std::make_index_sequence<size_t(PixelFormat::_Max)>(), fmt);
}

auto toString(SampleFormat fmt) -> std::string_view {
    return refl::enum2str(std::make_index_sequence<size_t(SampleFormat::_Max)>(), fmt);
}

}