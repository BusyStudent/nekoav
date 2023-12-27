#pragma once

#include "../defs.hpp"
#include "../resource.hpp"

NEKO_NS_BEGIN

class Thread;
class GLFunctions;

/**
 * @brief OpenGL Context
 * 
 */
class GLContext {
public:
    virtual ~GLContext() = default;
    
    virtual bool  makeCurrent() = 0;
    virtual void  doneCurrent() = 0;
    virtual void *getProcAddress(const char *name) const = 0;
    virtual bool  hasExtension(const char *name) const = 0;

    template <typename PFN>
    inline  PFN   getProcAddress(const char *name) const {
        return reinterpret_cast<PFN>(getProcAddress(name));
    }
};
/**
 * @brief A Display than can create context
 * 
 */
class GLDisplay {
public:
    virtual ~GLDisplay() = default;

    virtual Box<GLContext> createContext() = 0;
};
/**
 * @brief A OpenGL Video Frame
 * 
 */
class GLFrame : public Resource {
public:
    enum Format : int {
        RGBA, //< RGBA32 1 texture
        NV12,
        NV21,
    };

    /**
     * @brief Get the texture object
     * 
     * @param plane 
     * @return int 
     */
    virtual unsigned int texture(int plane) const = 0;
    /**
     * @brief Get the number of textures
     * 
     * @return size_t 
     */
    virtual size_t      textureCount() const = 0;
    /**
     * @brief Get the format of this frame
     * 
     * @return Format 
     */
    virtual Format      format() const = 0;
    /**
     * @brief Get the pts timestamp
     * 
     * @return double 
     */
    virtual double      timestamp() const = 0;
    /**
     * @brief Get the duration of this 
     * 
     * @return double 
     */
    virtual double      duration() const = 0;
    /**
     * @brief Get the width
     * 
     * @return int 
     */
    virtual int         width() const = 0;
    /**
     * @brief Get the height
     * 
     * @return int 
     */
    virtual int         height() const = 0;
    /**
     * @brief Set the Timestamp object
     * 
     * @param timestamp 
     */
    virtual void        setTimestamp(double timestamp) = 0;
    /**
     * @brief Set the Duration object
     * 
     * @param duration 
     */
    virtual void        setDuration(double duration) = 0;
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
    virtual ~GLController() = default;
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
 * @param controller 
 * @param width 
 * @param height 
 * @param format 
 * @return NEKO_API 
 */
extern NEKO_API Arc<GLFrame>      CreateGLFrame(View<GLController> controller, int width, int height, GLFrame::Format format);
/**
 * @brief Copy OpenGL Frame props (timestamp, duration)
 * 
 * @param dst 
 * @param src 
 */
inline void CopyGLFrameProps(GLFrame *dst, GLFrame *src) {
    dst->setTimestamp(src->timestamp());
    dst->setDuration(src->duration());
}

NEKO_NS_END