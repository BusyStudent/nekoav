#pragma once

#include "../defs.hpp"
#include "../resource.hpp"

NEKO_NS_BEGIN

class Thread;
class GLFunctions;

class GLContext {
public:
    virtual ~GLContext() = default;
    
    virtual bool  makeCurrent() = 0;
    virtual void  doneCurrent() = 0;
    virtual void *getProcAddress(const char *name) const = 0;
};
class GLDisplay {
public:
    virtual ~GLDisplay() = default;

    virtual Box<GLContext> createContext() = 0;
};
class GLFrame : public Resource {
public:
    /**
     * @brief Get the texture object
     * 
     * @param plane 
     * @return int 
     */
    virtual int         texture(int plane) const = 0;
    /**
     * @brief Get the number of textures
     * 
     * @return size_t 
     */
    virtual size_t      textureCount() const = 0;
    /**
     * @brief Get the pixel format of this frame
     * 
     * @return PixelFormat 
     */
    virtual PixelFormat pixelFormat() const = 0;
    virtual double      timestamp() const = 0;
    virtual double      duration() const = 0;
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
    /**
     * @brief Set the Display object, otherwisem the controller will create a new display by default (using CreateGLDisplay)
     * 
     * @note The Controller will take it's ownship of the display
     * @param display 
     */
    virtual void setDisplay(Box<GLDisplay> display) = 0;
    /**
     * @brief Release the gl thread, dereference it
     * 
     * @param thread 
     */
    virtual void freeThread(Thread *thread) = 0;
    /**
     * @brief Release the gl context, dereference it
     * 
     * @param ctxt 
     */
    virtual void freeContext(GLContext *ctxt) = 0;
    /**
     * @brief Allocate a gl thread, reference it
     * 
     * @return Thread* 
     */
    virtual Thread    *allocThread() = 0;
    /**
     * @brief Allocate a gl context, reference it
     * 
     * @return GLContext* 
     */
    virtual GLContext *allocContext() = 0;
};


/**
 * @brief Create a GLDisplay by local platform opengl interface
 * 
 * @return Box<GLDisplay> 
 */
extern NEKO_API Box<GLDisplay>    CreateGLDisplay();
/**
 * @brief Create a GLController in default implementation
 * 
 * @return Box<GLController> 
 */
extern NEKO_API Box<GLController> CreateGLController();
/**
 * @brief Create a OpenGL Frame
 * 
 * @param ctxt 
 * @param func 
 * @param width 
 * @param height 
 * @param format 
 * @return NEKO_API 
 */
extern NEKO_API Arc<GLFrame>      CreateGLFrame(View<GLContext> ctxt, View<GLFunctions> func, int width, int height, PixelFormat format);

NEKO_NS_END