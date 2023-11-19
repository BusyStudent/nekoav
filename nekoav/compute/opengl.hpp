#pragma once

#include "../defs.hpp"

NEKO_NS_BEGIN

class Thread;
/**
 * @brief Interface for Element who using OpenGL, All OpenGL Element in a pipeline run in the same thread
 * 
 */
class GLElement {
public:

};
class GLContext {
public:
    virtual ~GLContext() = default;
    
    virtual void  makeCurrent() = 0;
    virtual void  doneCurrent() = 0;
    virtual void *getProcAddress(const char *name) = 0;
};
class GLDisplay {
public:
    virtual Arc<GLContext> createContext() = 0;
};

/**
 * @brief A Controller for OpenGL, you need to register the Controller to the Context, otherwise, The pipeline will fail on setState
 * 
 * @code {.cpp}
 *  auto ctxt = pipeline->context();
 *  ctxt.addObject<GLController>(controller);
 * @endcode
 * 
 * 
 */
class GLController {
public:
 
};


/**
 * @brief Create a GLDisplay by local platform opengl interface
 * 
 * @return Box<GLDisplay> 
 */
extern NEKO_API Box<GLDisplay> CreateGLDisplay();

NEKO_NS_END