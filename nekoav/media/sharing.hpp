#pragma once

#include "../defs.hpp"
#include <functional>

NEKO_NS_BEGIN

/**
 * @brief A Interface proveided OpenGL Sharing
 * 
 */
class OpenGLSharing {
public:
    /**
     * @brief Invoke callable at GL Thread, it should block until the fn was called
     * 
     * @param fn 
     */
    virtual void invokeAtGLThread(std::function<void()> &&fn) = 0;
};

NEKO_NS_END