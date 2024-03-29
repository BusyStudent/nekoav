#pragma once

#include "defs.hpp"
#include <cstdarg>
#include <string>
#include <span>

NEKO_NS_BEGIN

namespace libc {
    /**
     * @brief Get the Human readable string by this type_info 
     * 
     * @param type The type_info ref
     * @return The string of this name, you can not free it
     */
    extern NEKO_API const char *typenameof(const std::type_info &type);
    /**
     * @brief Format string by fmt and args
     * 
     * @param fmt The format string
     * @param ... The args
     * @return std::string
     */
    extern NEKO_API std::string asprintf(const char *fmt, ...);
    /**
     * @brief Format string by fmt and args
     * 
     * @param fmt The format string
     * @param varg The args
     * @return std::string
     */
    extern NEKO_API std::string vasprintf(const char *fmt, va_list varg);
    /**
     * @brief Format string to target buffer
     * 
     * @param buf The target buffer (can not be nullptr)
     * @param fmt The format string
     * @param varg The args
     * @return size_t num of chars output 
     */
    extern NEKO_API size_t vsprintf(std::string *buf, const char *fmt, va_list varg);
        /**
     * @brief Format string to target buffer
     * 
     * @param buf The target buffer (can not be nullptr)
     * @param fmt The format string
     * @param ... The args
     * @return size_t num of chars output 
     */
    extern NEKO_API size_t sprintf(std::string *buf, const char *fmt, ...);
    /**
     * @brief Map a readonly file
     * 
     * @param path 
     * @return NEKO_API 
     */
    extern NEKO_API std::span<uint8_t> mmap(const char *path);
    /**
     * @brief Unmap a readonly mapping
     * 
     * @param data 
     * @return NEKO_API 
     */
    extern NEKO_API void               munmap(std::span<uint8_t> data);
    /**
     * @brief Open a file but support unicode
     * 
     * @param path The utf8 string
     * @param mode 
     * @return NEKO_API* 
     */
    extern NEKO_API FILE              *u8fopen(const char *path, const char *mode);
#ifdef _WIN32
    /**
     * @brief Convert u8 to 16
     * 
     * @param u8 
     * @return NEKO_API 
     */
    extern NEKO_API std::u16string     to_utf16(std::string_view u8);
    /**
     * @brief Convert u16 to u8
     * 
     * @param u16 
     * @return NEKO_API 
     */
    extern NEKO_API std::string        to_utf8(std::u16string_view u16);
    /**
     * @brief Convert u16 to local
     * 
     * @param u16 
     * @return NEKO_API 
     */
    extern NEKO_API std::string        to_local(std::u16string_view u16);
    /**
     * @brief Convert u8 to local
     * 
     * @param u8 
     * @return NEKO_API 
     */
    inline std::string                  to_local(std::string_view u8) {
        static_assert(sizeof(wchar_t) == sizeof(char16_t));
        return to_local(to_utf16(u8));
    }
    /**
     * @brief Convert
     * 
     * @param u16 
     * @return std::string 
     */
    inline std::string                  to_utf8(std::wstring_view u16) {
        static_assert(sizeof(wchar_t) == sizeof(char16_t));
        return to_utf8({reinterpret_cast<const char16_t *>(u16.data()), u16.size()});
    }
#endif
}

NEKO_NS_END