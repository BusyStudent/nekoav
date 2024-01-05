#pragma once

#include <initializer_list>
#include <type_traits>
#include <typeinfo>
#include <optional>
#include <utility>
#include <variant>
#include <cstdarg>
#include <memory>
#include <cstdio>
#include <string>
#include <chrono>
#include <tuple>
#include <array>
#include <mutex>
#include <assert.h>

#ifdef __GNUC__
    #define NEKO_STRINGIFY_TYPE_RAW(x) NEKO_STRINGIFY_TYPEINFO(typeid(x))
    #define NEKO_FUNCTION __PRETTY_FUNCTION__
    #include <cxxabi.h>
    #include <functional>
    #include <vector>
    #include <deque>
    #include <list>
    #include <map>

    template <typename T, T Value>
    constexpr auto _Neko_GetEnumName() noexcept {
        // constexpr auto _Neko_GetEnumName() [with T = MyEnum; T Value = MyValues]
        // constexpr auto _Neko_GetEnumName() [with T = MyEnum; T Value = (MyEnum)114514]"
        std::string_view name(__PRETTY_FUNCTION__);
        size_t eqBegin = name.find_last_of(' ');
        size_t end = name.find_last_of(']');
        std::string_view body = name.substr(eqBegin + 1, end - eqBegin - 1);
        if (body[0] == '(') {
            // Failed
            return std::string_view();
        }
        return body;
    }
    inline std::string NEKO_STRINGIFY_TYPEINFO(const std::type_info &info) {
        int status;
        auto str = ::abi::__cxa_demangle(info.name(), nullptr, nullptr, &status);
        if (str) {
            std::string ret(str);
            ::free(str);
            return ret;
        }
        return info.name();
    }
#elif defined(_MSC_VER)
    #define NEKO_STRINGIFY_TYPE_RAW(type) NEKO_STRINGIFY_TYPEINFO(typeid(type))
    #define NEKO_ENUM_TO_NAME(enumType)
    #define NEKO_FUNCTION __FUNCTION__

    inline const char *NEKO_STRINGIFY_TYPEINFO(const std::type_info &info) {
        // Skip struct class prefix
        auto name = info.name();
        if (::strncmp(name, "class ", 6) == 0) {
            return name + 6;
        }
        if (::strncmp(name, "struct ", 7) == 0) {
            return name + 7;
        }
        if (::strncmp(name, "enum ", 5) == 0) {
            return name + 5;
        }
        return name;
    }
    template <typename T, T Value>
    constexpr auto _Neko_GetEnumName() noexcept {
        // auto __cdecl _Neko_GetEnumName<enum main::MyEnum,(enum main::MyEnum)0x2>(void)
        // auto __cdecl _Neko_GetEnumName<enum main::MyEnum,main::MyEnum::Wtf>(void)
        std::string_view name(__FUNCSIG__);
        size_t dotBegin = name.find_first_of(',');
        size_t end = name.find_last_of('>');
        std::string_view body = name.substr(dotBegin + 1, end - dotBegin - 1);
        if (body[0] == '(') {
            // Failed
            return std::string_view();
        }
        return body;
    }
    // STD Container forward decl
    namespace std {
        template <typename T, typename Allocator> class vector;
        template <typename T, typename Allocator> class list;
        template <typename T, typename Allocator> class deque;
        template <typename Key, typename Value, typename Compare, typename Allocator> class map;
        template <typename T> class function;
    }
#else
    #define NEKO_STRINGIFY_TYPE_RAW(type) typeid(type).name()
    template <typename T, T Value>
    constexpr auto _Neko_GetEnumName() noexcept {
        // Unsupported
        return std::string_view();
    }
#endif

#if   defined(_WIN32) && !defined(NEKO_NO_TTY_CHECK)
    #include <Windows.h>
    #undef min
    #undef max
#elif defined(__linux) && !defined(NEKO_NO_TTY_CHECK)
    #include <unistd.h>
#endif

#define NEKO_FORMAT_TYPE_TO(type, str) \
    template <>                        \
    struct _Neko_TypeFormatter<type> { \
        static std::string name() {    \
            return str;                \
        }                              \
    }

template <typename T, typename _Cond = void>
struct _Neko_TypeFormatter {
    static std::string name() {
        return NEKO_STRINGIFY_TYPE_RAW(T);
    }
};

#ifndef NEKO_ENUM_SEARCH_DEPTH
    #define NEKO_ENUM_SEARCH_DEPTH 60
#endif

#define NEKO_STRINGIFY_TYPE(x) _Neko_TypeFormatter<x>::name()

#ifndef NEKO_NO_LOG
    #define NEKO_DEBUG(x)                                            \
        {                                                            \
        auto _neko_logstr = ::_Neko_FormatDebug(#x, x);              \
        std::lock_guard _logmtx(::_Neko_GetLogMutex());              \
        ::_Neko_LogPath(__FILE__, __LINE__, NEKO_FUNCTION);          \
        ::fputs(_neko_logstr.c_str(), stderr);                       \
        ::fputc('\n', stderr);                                       \
        ::fflush(stderr);                                            \
        }
    #define NEKO_LOG(...)                                            \
        {                                                            \
        auto _neko_logstr = ::_Neko_FormatLog(__VA_ARGS__);          \
        std::lock_guard _logmtx(::_Neko_GetLogMutex());              \
        ::_Neko_LogPath(__FILE__, __LINE__, NEKO_FUNCTION);          \
        ::fputs(_neko_logstr.c_str(), stderr);                       \
        ::fputc('\n', stderr);                                       \
        ::fflush(stderr);                                            \
        }
#else
    #define NEKO_DEBUG(x)
    #define NEKO_LOG(...)
#endif

// Add default types chrono
#ifdef _MSC_VER
    NEKO_FORMAT_TYPE_TO(std::chrono::system_clock::duration, "std::chrono::system_clock::duration");
#endif
NEKO_FORMAT_TYPE_TO(std::chrono::system_clock::time_point, "std::chrono::system_clock::time_point");
NEKO_FORMAT_TYPE_TO(std::chrono::nanoseconds, "std::chrono::nanoseconds");
NEKO_FORMAT_TYPE_TO(std::chrono::milliseconds, "std::chrono::milliseconds");
NEKO_FORMAT_TYPE_TO(std::chrono::microseconds, "std::chrono::microseconds");
NEKO_FORMAT_TYPE_TO(std::chrono::seconds, "std::chrono::seconds");
NEKO_FORMAT_TYPE_TO(std::chrono::minutes, "std::chrono::minutes");
NEKO_FORMAT_TYPE_TO(std::chrono::hours, "std::chrono::hours");

// string
NEKO_FORMAT_TYPE_TO(std::string, "std::string");
NEKO_FORMAT_TYPE_TO(std::u16string, "std::u16string");
NEKO_FORMAT_TYPE_TO(std::u32string, "std::u32string");
NEKO_FORMAT_TYPE_TO(std::wstring, "std::wstring");
NEKO_FORMAT_TYPE_TO(std::string_view, "std::string_view");
NEKO_FORMAT_TYPE_TO(std::u16string_view, "std::u16string_view");
NEKO_FORMAT_TYPE_TO(std::u32string_view, "std::u32string_view");
NEKO_FORMAT_TYPE_TO(std::wstring_view, "std::wstring_view");

// int
NEKO_FORMAT_TYPE_TO(int8_t, "int8_t");
NEKO_FORMAT_TYPE_TO(int16_t, "int16_t");
NEKO_FORMAT_TYPE_TO(int32_t, "int32_t");
NEKO_FORMAT_TYPE_TO(int64_t, "int64_t");
NEKO_FORMAT_TYPE_TO(uint8_t, "uint8_t");
NEKO_FORMAT_TYPE_TO(uint16_t, "uint16_t");
NEKO_FORMAT_TYPE_TO(uint32_t, "uint32_t");
NEKO_FORMAT_TYPE_TO(uint64_t, "uint64_t");

NEKO_FORMAT_TYPE_TO(void, "void");

// Check a type is iterable like array or vector
template <typename T, typename _Cond = void>
struct _Neko_IsIterable : public std::false_type {

};
template <typename T>
struct _Neko_IsIterable<T, std::void_t<decltype(std::end(std::declval<T>()))> > : public std::true_type {

};

// Check a type has to docoument
template <typename T, typename _Cond = void>
struct _Neko_HasToDocument : public std::false_type {

};
template <typename T>
struct _Neko_HasToDocument<T, std::void_t<decltype(&T::toDocoument)> > : public std::true_type {

};

// STL container
template <typename Key, typename Value, typename Compare, typename Allocator>
struct _Neko_TypeFormatter<std::map<Key, Value, Compare, Allocator> > {
    static std::string name() {
        return std::string("std::map<") + NEKO_STRINGIFY_TYPE(Key) + ", " + NEKO_STRINGIFY_TYPE(Value) + ">";
    }
};
template <typename T, typename Alloc>
struct _Neko_TypeFormatter<std::vector<T, Alloc> > {
    static std::string name() {
        return std::string("std::vector<") + NEKO_STRINGIFY_TYPE(T) + ">";
    }
};
template <typename T, typename Alloc>
struct _Neko_TypeFormatter<std::list<T, Alloc> > {
    static std::string name() {
        return "std::list<" + NEKO_STRINGIFY_TYPE(T) + ">";
    }
};
template <typename T, typename Alloc>
struct _Neko_TypeFormatter<std::deque<T, Alloc> > {
    static std::string name() {
        return "std::deque<" + NEKO_STRINGIFY_TYPE(T) + ">"; 
    }
};
template <typename T>
struct _Neko_TypeFormatter<std::initializer_list<T> > {
    static std::string name() {
        return "std::initializer_list<" + NEKO_STRINGIFY_TYPE(T) + ">";
    }
};
template <typename T, size_t N>
struct _Neko_TypeFormatter<std::array<T, N> > {
    static std::string name() {
        return "std::array<" + NEKO_STRINGIFY_TYPE(T) + ", " + std::to_string(N) + ">";
    }
};
template <typename First, typename Second>
struct _Neko_TypeFormatter<std::pair<First, Second> > {
    static std::string name() {
        return "std::pair<" + NEKO_STRINGIFY_TYPE(First) + ", " + NEKO_STRINGIFY_TYPE(Second) + ">";
    }
};
template <typename T>
struct _Neko_TypeFormatter<std::optional<T> > {
    static std::string name() {
        return "std::optional<" + NEKO_STRINGIFY_TYPE(T) + ">"; 
    }
};
template <typename T>
struct _Neko_TypeFormatter<std::shared_ptr<T> > {
    static std::string name() {
        return "std::shared_ptr<" + NEKO_STRINGIFY_TYPE(T) + ">"; 
    }
};
template <typename T>
struct _Neko_TypeFormatter<std::unique_ptr<T> > {
    static std::string name() {
        return "std::unique_ptr<" + NEKO_STRINGIFY_TYPE(T) + ">"; 
    }
};
template <typename T>
struct _Neko_TypeFormatter<std::weak_ptr<T> > {
    static std::string name() {
        return "std::weak_ptr<" + NEKO_STRINGIFY_TYPE(T) + ">"; 
    }
};
template <typename ...Types>
struct _Neko_TypeFormatter<std::variant<Types...> > {
    static std::string name() {
        auto names = {_Neko_TypeFormatter<Types>::name()...};
        std::string str;
        str += "std::variant<";
        for (const auto &val : names) {
            str += val;
            str += ", ";
        }
        str.pop_back();
        str.pop_back();
        str += ">";
        return str;
    }
};
template <typename ...Types>
struct _Neko_TypeFormatter<std::tuple<Types...> > {
    static std::string name() {
        auto names = {_Neko_TypeFormatter<Types>::name()...};
        std::string str;
        str += "std::tuple<";
        for (const auto &val : names) {
            str += val;
            str += ", ";
        }
        str.pop_back();
        str.pop_back();
        str += ">";
        return str;
    }
};
template <typename RetT>
struct _Neko_TypeFormatter<std::function<RetT()> > {
    static std::string name() {
        return "std::function<" + NEKO_STRINGIFY_TYPE(RetT) + "()>";
    }
};
template <typename RetT, typename ...Args>
struct _Neko_TypeFormatter<std::function<RetT(Args...)> > {
    static std::string name() {
        auto args = {_Neko_TypeFormatter<Args>::name()...};
        std::string str;
        str += "std::function<";
        str += _Neko_TypeFormatter<RetT>::name();
        str += "(";
        for (const auto &val : args) {
            str += val;
            str += ", ";
        }
        str.pop_back();
        str.pop_back();
        str += ">";
        return str;
    }
};
// -- Type Format End


// -- EnumToString Here
template <typename T, T Value>
constexpr bool     _Neko_IsValidEnum() noexcept {
    return !_Neko_GetEnumName<T, Value>().empty();
}
template <typename T, size_t ...N>
constexpr size_t   _Neko_GetValidEnumCount(std::index_sequence<N...> seq) noexcept {
    return (... + _Neko_IsValidEnum<T, T(N)>());
}
template <typename T, size_t ...N>
constexpr auto     _Neko_GetValidEnumNames(std::index_sequence<N...> seq) noexcept {
    constexpr auto validCount = _Neko_GetValidEnumCount<T>(seq);
    
    std::array<std::pair<T, std::string_view>, validCount> arr;
    std::string_view vstr[sizeof ...(N)] {
        _Neko_GetEnumName<T, T(N)>()...
    };

    size_t n = 0;
    size_t left = validCount;
    auto iter = arr.begin();

    for (auto i : vstr) {
        if (!i.empty()) {
            // Valid name
            iter->first = T(n);
            iter->second = i;
            ++iter;
        }
        if (left == 0) {
            break;
        }

        n += 1;
    }
    
    return arr;
}
template <typename T>
inline std::string _Neko_EnumToString(T en) {
    constexpr auto map = _Neko_GetValidEnumNames<T>(std::make_index_sequence<NEKO_ENUM_SEARCH_DEPTH>());
    for (auto [value, name] : map) {
        if (value == en) {
            return std::string(name);
        }
    }
    return std::to_string(std::underlying_type_t<T>(en));
}

inline std::string _Neko_asprintf(const char *fmt, ...) {
    va_list varg;
    int s;
    
    va_start(varg, fmt);
#ifdef _WIN32
    s = _vscprintf(fmt, varg);
#else
    s = vsnprintf(nullptr, 0, fmt, varg);
#endif
    va_end(varg);

    std::string str;
    str.resize(s);

    va_start(varg, fmt);
    vsprintf(str.data(), fmt, varg);
    va_end(varg);

    return str;
}

// Number
template <typename T,
          typename _Cond = std::enable_if_t<std::is_arithmetic_v<T> >
>
inline std::string _Neko_ToString(T val) {
    if constexpr (std::is_same_v<T, bool>) {
        if (val) {
            return "true";
        }
        return "false";
    }
    return std::to_string(val);
}

// Function
template <typename RetT, typename ...Args>
inline std::string _Neko_ToString(const std::function<RetT(Args...)> &func) {
    if (func) {
        return NEKO_STRINGIFY_TYPEINFO(func.target_type());
    }
    return "nullptr";
}

// Enum
template <typename T,
          typename std::enable_if<std::is_enum<T>::value>::type* = nullptr
>
inline std::string _Neko_ToString(T en) {
    return _Neko_EnumToString(en);
}

inline std::string _Neko_ToString(const std::string &s) {
    return '"' + s + '"';
}
inline std::string _Neko_ToString(std::string_view s) {
    return '"' + std::string(s) + '"';
}

// Type
inline std::string _Neko_ToString(const std::type_info &info) {
    return NEKO_STRINGIFY_TYPEINFO(info);
}

// Chrono parts format to string
inline std::string _Neko_ToString(std::chrono::system_clock::time_point timepoint) {
    auto timet = std::chrono::system_clock::to_time_t(timepoint);
    char buf[128] = {0};
    ::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ::localtime(&timet));
    return buf;
}
inline std::string _Neko_ToString(std::chrono::nanoseconds s) {
    return std::to_string(s.count()) + "ns";
}
inline std::string _Neko_ToString(std::chrono::microseconds s) {
    return std::to_string(s.count()) + "us";
}
inline std::string _Neko_ToString(std::chrono::milliseconds s) {
    return std::to_string(s.count()) + "ms";
}
inline std::string _Neko_ToString(std::chrono::seconds s) {
    return std::to_string(s.count()) + "s";
}
inline std::string _Neko_ToString(std::chrono::minutes s) {
    return std::to_string(s.count()) + "min";
}
inline std::string _Neko_ToString(std::chrono::hours s) {
    return std::to_string(s.count()) + "h";
}

// Pointer
inline std::string _Neko_ToString(void *ptr) {
    return _Neko_asprintf("%p", ptr);
}
inline std::string _Neko_ToString(char *ptr) {
    return _Neko_asprintf("%s", ptr);
}
inline std::string _Neko_ToString(const char *ptr) {
    return _Neko_asprintf("%s", ptr);
}
template <typename RetT, typename ...Args>
inline std::string _Neko_ToString(RetT(*func)(Args...)) {
    return _Neko_asprintf("%p", func);
}
template <typename T>
inline std::string _Neko_ToString(const std::shared_ptr<T> &ptr) {
    return _Neko_ToString(ptr.get());
}
template <typename T>
inline std::string _Neko_ToString(const std::unique_ptr<T> &ptr) {
    return _Neko_ToString(ptr.get());
}



// Optional
template <typename T>
inline std::string _Neko_ToString(const std::optional<T> &optional) {
    if (optional.has_value()) {
        return _Neko_ToString(optional.value());
    }
    return "nullopt";
}

// Variant
template <typename ...Types>
inline std::string _Neko_ToString(const std::variant<Types...> &variant) {
    return std::visit([](auto &&val) {
        return _Neko_ToString(val);
    }, variant);
}

// Tuple
template <typename ...Args>
inline auto       _Neko_TupleToStringHelper(Args ...args) {
    std::array<std::string, sizeof...(Args)> arr = {_Neko_ToString(std::forward<Args>(args))...};
    return arr;
}
template <typename ...Types>
inline std::string _Neko_ToString(const std::tuple<Types...> &tuple) {
    std::string str;
    str += "{";
    auto array = std::apply(_Neko_TupleToStringHelper<Types...>, tuple);
    for (const auto &val : array) {
        str += val;
        str += ", ";
    }

    str.pop_back();
    str.pop_back();
    str += "}";
    return str;
}

// Pair
template <typename First, typename Second>
inline std::string _Neko_ToString(const std::pair<First, Second> &pair) {
    std::string str;
    str += "{";
    str += _Neko_ToString(pair.first);
    str += ", ";
    str += _Neko_ToString(pair.second);
    str += "}";
    return str;
}

// Container
template <typename T,
          typename _Cond = std::enable_if_t<_Neko_IsIterable<T>::value>
>
inline std::string _Neko_ToString(const T &container) {
    std::string str;
    str += "{";
    for (auto &val : container) {
        str += _Neko_ToString(val);
        str += ", ";
    }
    str.pop_back();
    str.pop_back();
    str += "}";
    return str;
}
template <typename T, size_t N>
inline std::string _Neko_ToString(const T (&container)[N]) {
    std::string str;
    str += "{";
    for (auto &val : container) {
        str += _Neko_ToString(val);
        str += ", ";
    }
    str.pop_back();
    str.pop_back();
    str += "}";
    return str;
}

// ToDocoument
template <typename T,
          typename _Cond = std::enable_if_t<_Neko_HasToDocument<T>::value>,
          char = 0
>
inline std::string _Neko_ToString(const T &obj) {
    return obj.toDocoument();
}

inline bool       _Neko_HasColorTTY() {
#if   defined(_WIN32) && !defined(NEKO_NO_TTY_CHECK)
    static bool value = []() {
        DWORD mode = 0;
        ::GetConsoleMode(::GetStdHandle(STD_ERROR_HANDLE), &mode);
        return mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    }();
    return value;
#elif defined(__linux) && !defined(NEKO_NO_TTY_CHECK)
    return ::isatty(::fileno(stdout));
#else
    return true;
#endif
}

template <typename T>
inline std::string _Neko_FormatType() {
    using Type = std::remove_reference_t<T>;
    std::string ret;
    if constexpr (std::is_pointer_v<Type>) {
        ret = NEKO_STRINGIFY_TYPE(std::remove_pointer_t<Type>) + " *";
    }
    else if constexpr (std::is_array_v<std::remove_reference_t<Type> >) {
        ret = NEKO_STRINGIFY_TYPE(std::remove_extent_t<Type>) + " [" + std::to_string(std::extent_v<Type>) + "]";
    }
    else {
        ret = NEKO_STRINGIFY_TYPE(Type);
    }
    return ret;
}

template <typename T>
inline std::string _Neko_FormatDebug(const char *body, T &&val) {
    const char *fmt = "%s = %s (%s)";
    if (_Neko_HasColorTTY()) {
        fmt = "\033[96m%s\033[0m = \033[1m%s\033[0m (\033[32m%s\033[0m)";
    }
    return _Neko_asprintf(fmt, body, _Neko_ToString(std::forward<T>(val)).c_str(), _Neko_FormatType<T>().c_str());
}
template <size_t N>
inline std::string _Neko_FormatDebug(const char *body, const char (&text)[N]) {
    if (!_Neko_HasColorTTY()) {
        return text;
    }
    return std::string("\033[1m") + text + "\033[0m";
}
inline std::string _Neko_FormatLogBody(std::string_view fmt, const std::string *arrays) {
    std::string ret;
    if (_Neko_HasColorTTY()) {
        ret = "\033[1m";
    }

    auto pos = fmt.find_first_of('{');
    while (pos != std::string_view::npos) {
        ret += fmt.substr(0, pos);
        fmt = fmt.substr(pos + 1);
        pos = fmt.find_first_of('}');
        if (pos == std::string_view::npos) {
            ::abort();
        }
        ret += *arrays;
        arrays++;

        fmt = fmt.substr(pos + 1);
        pos = fmt.find_first_of('{');
    }

    if (_Neko_HasColorTTY()) {
        ret += "\033[0m";
    }

    return ret;
}
template <typename ...Args>
inline std::string _Neko_FormatLog(std::string_view fmt, Args &&...args) {
    if constexpr (sizeof ...(args) == 0) {
        return std::string(fmt);
    }
    else {
        std::string arrs [] = {_Neko_ToString(std::forward<Args>(args))...};
        return _Neko_FormatLogBody(fmt, arrs);
    }
}
inline void       _Neko_LogPath(const char *file, int line, const char *func) {
    const char *fmt = "[%s:%d (%s)] ";
    if (_Neko_HasColorTTY()) {
        fmt = "\033[90m[%s:%d (%s)]\033[0m ";
    }
    ::fprintf(stderr, fmt, file, line, func);
}

inline std::mutex &_Neko_GetLogMutex() {
    static std::mutex m;
    return m;
}

#define NEKO_LOG_LEVEL_INFO 0x0001
#define NEKO_LOG_LEVEL_WARNING 0x0002
#define NEKO_LOG_LEVEL_FATA 0x0003

#define NEKO_LOG_LEVEL_DEBUG 0x0100
