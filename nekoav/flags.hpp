#pragma once

#define NEKO_DECLARE_FLAGS(type) \
    constexpr type operator~ (type a) noexcept { \
        return static_cast<type>(~static_cast<std::underlying_type_t<type>>(a)); \
    } \
    constexpr type operator| (type a, type b) noexcept { \
        return static_cast<type>(static_cast<std::underlying_type_t<type>>(a) | static_cast<std::underlying_type_t<type>>(b)); \
    } \
    constexpr type operator& (type a, type b) noexcept { \
        return static_cast<type>(static_cast<std::underlying_type_t<type>>(a) & static_cast<std::underlying_type_t<type>>(b)); \
    } \
    constexpr type operator^ (type a, type b) noexcept { \
        return static_cast<type>(static_cast<std::underlying_type_t<type>>(a) ^ static_cast<std::underlying_type_t<type>>(b)); \
    } \
    inline    type& operator|= (type& a, type b) noexcept { \
        return reinterpret_cast<type&>(reinterpret_cast<std::underlying_type_t<type>&>(a) |= static_cast<std::underlying_type_t<type>>(b)); \
    } \
    inline    type& operator&= (type& a, type b) noexcept { \
        return reinterpret_cast<type&>(reinterpret_cast<std::underlying_type_t<type>&>(a) &= static_cast<std::underlying_type_t<type>>(b)); \
    } \
    inline    type& operator^= (type& a, type b) noexcept { \
        return reinterpret_cast<type&>(reinterpret_cast<std::underlying_type_t<type>&>(a) ^= static_cast<std::underlying_type_t<type>>(b)); \
    }


