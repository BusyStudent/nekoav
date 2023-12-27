#pragma once

#include "../elements.hpp"

NEKO_NS_BEGIN

template <typename T, size_t Col, size_t Row>
struct Kernel {
    T data[Col][Row] {};
};

class GLShader : public Element {
public:

};
class GLKernel : public Element {
public:
    virtual void setKernel(const Kernel<float, 3, 3> &kernel) = 0;

    static Kernel<float, 3, 3> createBlurKernel(float sigma) noexcept;
    static Kernel<float, 3, 3> createSharpenKernel() noexcept;
    static Kernel<float, 3, 3> createEdgeDetectKernel() noexcept;
};


// Kernel List here
inline Kernel<float, 3, 3> GLKernel::createSharpenKernel() noexcept {
    return {{
         0.0f, -1.0f,  0.0f,
        -1.0f,  5.0f, -1.0f,
         0.0f, -1.0f,  0.0f
    }};
}
inline Kernel<float, 3, 3> GLKernel::createEdgeDetectKernel() noexcept {
    return {{
        -1.0f, -1.0f, -1.0f,
        -1.0f,  8.0f, -1.0f,
        -1.0f, -1.0f, -1.0f
    }};
}

NEKO_NS_END