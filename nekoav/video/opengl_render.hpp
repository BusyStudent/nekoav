#pragma once

#include "renderer.hpp"

NEKO_NS_BEGIN

class _EmptyBase { };


template <typename T = _EmptyBase>
class GLVideoRenderer final : public T {
public:
    template <typename ...Args>
    GLVideoRenderer(Args &&...args) : T(std::forward<Args>(args)...) {
        
    }
    ~GLVideoRenderer() {

    }
private:
    
};

NEKO_NS_END