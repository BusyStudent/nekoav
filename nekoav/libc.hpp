#pragma once

#include "defs.hpp"
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
     * @param fmt 
     * @param ... 
     * @return Box<char>
     */
    extern NEKO_API std::string asprintf(const char *fmt, ...);
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
}

NEKO_NS_END