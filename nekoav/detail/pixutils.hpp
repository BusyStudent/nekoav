#pragma once

#include "../format.hpp"

NEKO_NS_BEGIN

template <typename T>
class PixColor {
public:
    T r;
    T g;
    T b;
    T a;
};

template <typename T = uint32_t>
inline PixColor<T> UnpackRGBA(uint32_t rgba) noexcept {
    T r = (rgba >> 24) & 0xFF;
    T g = (rgba >> 16) & 0xFF;
    T b = (rgba >> 8) & 0xFF;
    T a = rgba & 0xFF;
    return {r, g, b, a};
}
inline uint32_t PackRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a) noexcept {
    return (r << 24) | (g << 16) | (b << 8) | a;
}

NEKO_NS_END