#define _NEKO_SOURCE
#include "opengl.hpp"
#include "opengl_pfns.hpp"
#include "opengl_macros.hpp"
#include <vector>

NEKO_NS_BEGIN

class GLFrameFunc {
public:
    GLContext               *mCtxt = nullptr;
    PFNGLDELETETEXTURESPROC glDeleteTextures = nullptr;
};

class GLFrameImpl final : public GLFrame {
public:
    GLFrameImpl(GLFrameFunc func, int width, int height, GLFrame::Format format) 
        : mFunc(func), mWidth(width), mHeight(height), mFormat(format) 
    {

    }
    ~GLFrameImpl() {
        mFunc.glDeleteTextures(mNumofTexture, mTextures);
    }
    unsigned int texture(int plane) const override {
        if (plane < mNumofTexture) {
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
    void addTexture(unsigned int texture) override {
        NEKO_ASSERT(mNumofTexture < 8);
        mTextures[mNumofTexture] = texture;
        mNumofTexture += 1;
    }
private:
    GLFrameFunc mFunc;

    int mWidth = 0;
    int mHeight = 0;
    Format mFormat {};

    double mDuration = 0.0;
    double mTimestamp = 0.0;

    GLuint mTextures[8] {0};
    int    mNumofTexture = 0;
};

Arc<GLFrame> CreateGLFrame(View<GLContext> ctxt, int width, int height, GLFrame::Format format) {
    static thread_local GLFrameFunc glfn;

    if (glfn.mCtxt != ctxt.get()) {
        // Load it
        glfn.mCtxt = ctxt.get();
        glfn.glDeleteTextures = (PFNGLDELETETEXTURESPROC)glfn.mCtxt->getProcAddress("glDeleteTextures");
    }
    if (!glfn.glDeleteTextures) {
        return nullptr;
    }
    return make_shared<GLFrameImpl>(glfn, width, height, format);
}

NEKO_NS_END