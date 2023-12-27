#define _NEKO_SOURCE
#include "opengl.hpp"
#include "opengl_pfns.hpp"
#include "opengl_macros.hpp"
#include "../threading.hpp"
#include "../backtrace.hpp"
#include "../libc.hpp"
#include <vector>
#include <mutex>

NEKO_NS_BEGIN

// TODO : Texture Cache Pool
class GLFrameCache {
public:
    GLuint mTexture = 0;
    int    mWidth = 0;
    int    mHeight = 0;
    bool   mUsed = false;
    GLFrame::Format mFormat = GLFrame::Format::RGBA;
};
class GLFramePool {
public:
    PFNGLDELETETEXTURESPROC glDeleteTextures = nullptr;
    PFNGLGENTEXTURESPROC    glGenTextures = nullptr;
    PFNGLTEXIMAGE2DPROC     glTexImage2D = nullptr;
    PFNGLTEXPARAMETERIPROC  glTexParameteri = nullptr;
    PFNGLBINDTEXTUREPROC    glBindTexture = nullptr;
    PFNGLGETINTEGERVPROC    glGetIntegerv = nullptr;

    // Cache Item
    GLFrameCache  mCache[20];
};

class GLFrameImpl final : public GLFrame {
public:
    GLFrameImpl(GLFramePool &pool, int width, int height, GLFrame::Format format) 
        : mPool(pool), mWidth(width), mHeight(height), mFormat(format) 
    {
        // SAVE 
        GLint prevTexture = 0;
        mPool.glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
        switch (format) {
            case RGBA: {
                mNumofTexture = 1;
                mPool.glGenTextures(1, mTextures);
                mPool.glBindTexture(GL_TEXTURE_2D, mTextures[0]);
                mPool.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                mPool.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                mPool.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
            }
        }
        mPool.glBindTexture(GL_TEXTURE_2D, prevTexture);
    }
    ~GLFrameImpl() {
        mPool.glDeleteTextures(mNumofTexture, mTextures);
    }
    unsigned int texture(int plane) const override {
        if (plane >= mNumofTexture) {
            return 0;
        }
        return mTextures[plane];
    }
    size_t textureCount() const override {
        return mNumofTexture;
    }
    Format format() const override {
        return mFormat;
    }
    double timestamp() const override {
        return mTimestamp;
    }
    double duration() const override {
        return mDuration;
    }
    int width() const override {
        return mWidth;
    }
    int height() const override {
        return mHeight;
    }
    void setTimestamp(double pts) override {
        mTimestamp = pts;
    }
    void setDuration(double duration) override {
        mDuration = duration;
    }
    // void addTexture(unsigned int texture) override {
    //     NEKO_ASSERT(mNumofTexture < 8);
    //     mTextures[mNumofTexture] = texture;
    //     mNumofTexture += 1;
    // }
private:
    GLFramePool &mPool;

    int mWidth = 0;
    int mHeight = 0;
    Format mFormat {};

    double mDuration = 0.0;
    double mTimestamp = 0.0;

    GLuint mTextures[8] {0};
    int    mNumofTexture = 0;
};

// GLController
class GLControllerImpl final : public GLController {
public:
    GLControllerImpl() {

    }
    ~GLControllerImpl() {
        NEKO_ASSERT(mThreadRefcount == 0);
        NEKO_ASSERT(mContextRefcount == 0);
        NEKO_ASSERT(mThread == nullptr);
    }
    void setDisplay(Box<GLDisplay> display) override {
        mDisplay = std::move(display);
    }
    Thread *allocThread() override {
        std::lock_guard lock(mMutex);
        mThreadRefcount ++;
        if (mThread == nullptr) {
            mThread = std::make_unique<Thread>();
            mThread->setName("NekoGLThread");
        }
        return mThread.get();
    }
    GLContext *allocContext() override {
        NEKO_ASSERT(Thread::currentThread() == mThread.get());
        std::lock_guard lock(mMutex);
        mContextRefcount ++;
        if (mContext == nullptr) {
            if (!mDisplay) {
                mDisplay = CreateGLDisplay();
            }
            mContext = mDisplay->createContext();

            // Init Pool
            mContext->makeCurrent();
            _initPool(mContext.get());
            _initDebug(mContext.get());
            mContext->doneCurrent();
        }
        return mContext.get();
    }
    void freeThread(Thread *thread) override {
        NEKO_ASSERT(thread == mThread.get());
        NEKO_ASSERT(mThreadRefcount > 0);
        NEKO_ASSERT(Thread::currentThread() != mThread.get());

        std::lock_guard lock(mMutex);
        mThreadRefcount --;
        if (mThreadRefcount == 0) {
            mThread = nullptr;
        }
    }
    void freeContext(GLContext *context) override {
        NEKO_ASSERT(context == mContext.get());
        NEKO_ASSERT(mContextRefcount > 0);
        std::lock_guard lock(mMutex);
        mContextRefcount --;
        if (mContextRefcount == 0) {
            // Release cached Pool
            mContext->makeCurrent();
            _releasePool(mContext.get());
            mContext->doneCurrent();

            mContext = nullptr;
        }
    }

    // Frame
    Arc<GLFrame> _createFrame(int width, int height, GLFrame::Format format) {
        return MakeShared<GLFrameImpl>(std::ref(mPool), width, height, format);
    }
private:
    void _initPool(GLContext *ctxt) {
        mPool.glBindTexture = ctxt->getProcAddress<PFNGLBINDTEXTUREPROC>("glBindTexture");
        mPool.glTexImage2D = ctxt->getProcAddress<PFNGLTEXIMAGE2DPROC>("glTexImage2D");
        mPool.glGenTextures = ctxt->getProcAddress<PFNGLGENTEXTURESPROC>("glGenTextures");
        mPool.glDeleteTextures = ctxt->getProcAddress<PFNGLDELETETEXTURESPROC>("glDeleteTextures");
        mPool.glGetIntegerv = ctxt->getProcAddress<PFNGLGETINTEGERVPROC>("glGetIntegerv");
        mPool.glTexParameteri = ctxt->getProcAddress<PFNGLTEXPARAMETERIPROC>("glTexParameteri");
    }
    void _releasePool(GLContext *ctxt) {

    }
    void _initDebug(GLContext *ctxt) {
#ifndef NDEBUG
    if (!ctxt->hasExtension("GL_ARB_debug_output")) {
        return;
    }
    auto glDebugMessageCallback = reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKPROC>(ctxt->getProcAddress(
        "glDebugMessageCallback"
    ));
    auto glEnable = reinterpret_cast<PFNGLENABLEPROC>(ctxt->getProcAddress("glEnable"));
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    if (glDebugMessageCallback) {
        glDebugMessageCallback([](
            GLenum source, 
            GLenum type, 
            GLuint id, 
            GLenum severity, 
            GLsizei length, 
            const GLchar *message, 
            const void *userParam) 
        {
            if (type == GL_DEBUG_TYPE_ERROR) {
                Backtrace();
            }

            ::fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n", 
                (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
                type, severity, message);
            ::fflush(stderr);

            if (type == GL_DEBUG_TYPE_ERROR && libc::is_debugger_present()) {
                libc::breakpoint();
            }
        }, nullptr);
    }
#endif
    }


    std::mutex     mMutex;
    Atomic<size_t> mThreadRefcount {0};
    Atomic<size_t> mContextRefcount {0};

    // Managed by GLController
    Box<GLDisplay> mDisplay;
    Box<GLContext> mContext;
    Box<Thread>    mThread;

    // 
    GLFramePool    mPool;
};


Arc<GLFrame> CreateGLFrame(View<GLController> controller, int width, int height, GLFrame::Format format) {
    NEKO_ASSERT(controller.viewAs<GLControllerImpl>());
    auto ctl = static_cast<GLControllerImpl*>(controller.get());
    return ctl->_createFrame(width, height, format);
}

Box<GLController> CreateGLController() {
    return std::make_unique<GLControllerImpl>();
}

NEKO_NS_END