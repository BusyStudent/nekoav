#define _NEKO_SOURCE
#include "../media/frame.hpp"
#include "../factory.hpp"
#include "../format.hpp"

#include "opengl_helper.hpp"
#include "opengl_func.hpp"
#include "template.hpp"
#include "shader.hpp"

NEKO_NS_BEGIN

const char *kernelFragShader = R"(
#version 330 core
out vec4 fragColor;
in  vec2 texturePos;

uniform sampler2D rgbTexture;
uniform mat3 kernel;
uniform int textureWidth;
uniform int textureHeight;

void main() {
    fragColor = vec4(0.0);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            vec2 pos = vec2(float(i), float(j)) - 1.5;
            fragColor += texture(rgbTexture, texturePos + pos) * kernel[i][j];
        }
    }
    fragColor.a = 1.0;
}

)";

class GLKernelImpl final : public OpenGLImpl<GLKernel, GLFunctions_3_0> {
public:
    GLKernelImpl() {
        ::memcpy(mKernel, mNopKernel, sizeof(mKernel));
    }
    void setKernel(const Kernel<float, 3, 3> &kernel) override {
        static_assert(sizeof(kernel.data) == sizeof(mKernel));
        mIsNopKernel = (::memcmp(kernel.data, mNopKernel, sizeof(kernel.data)) == 0);
        ::memcpy(mKernel, kernel.data, sizeof(kernel.data));
        
        // Update if inited
        if (state() != State::Null) {
            invokeMethodQueued(&GLKernelImpl::_updateKernel, this);
        }
    }
    Error onGLInitialize() override {
        CreateGLComputeEnv(this, &mVertexArray, &mBuffer); //< Setup compute

        mProgram = CompileGLProgram(this, vertexCommonShader, kernelFragShader);
        if (mProgram == 0) {
            return Error::External;
        }
        glUseProgram(mProgram);
        glUniform1i(glGetUniformLocation(mProgram, "rgbTexture"), 0);
        glUniform1i(glGetUniformLocation(mProgram, "textureWidth"), 0);
        glUniform1i(glGetUniformLocation(mProgram, "textureHeight"), 0);
        glUniformMatrix3fv(glGetUniformLocation(mProgram, "kernel"), 1, GL_FALSE, (GLfloat *)mKernel);
        glUseProgram(0);

        mIsNopKernel = (::memcmp(mKernel, mNopKernel, sizeof(mKernel)) == 0);
        return Error::Ok;
    }
    Error onGLTeardown() override {
        DeleteGLComputeEnv(this, &mVertexArray, &mBuffer);
        if (mProgram) {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }
        _clearFrameBuffer();
        return Error::Ok;
    }
    void _updateKernel() {
        glUseProgram(mProgram);
        glUniformMatrix3fv(glGetUniformLocation(mProgram, "kernel"), 1, GL_FALSE, (GLfloat *)mKernel);
        glUseProgram(0);
    }
    void _clearFrameBuffer() {
        if (!mFrameBuffer) {
            return ;
        }
        if (mFrameBufferTexture) {
            glDeleteTextures(1, &mFrameBufferTexture);
            mFrameBufferTexture = 0;
        }
        glDeleteFramebuffers(1, &mFrameBuffer);
        mFrameBuffer = 0;
        mFrameBufferWidth = 0;
        mFrameBufferHeight = 0;
    }
    void _bindFrame(GLFrame *frame) {
        int width = frame->width();
        int height = frame->height();

        // Clear if size not matched
        if (width != mFrameBufferWidth || height != mFrameBufferHeight) {
            _clearFrameBuffer();
        }
        if (!mFrameBuffer) {
            mFrameBufferWidth = width;
            mFrameBufferHeight = height;

            // Make framebuffer
            glGenFramebuffers(1, &mFrameBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffer);

            // Make FrameBuffer Texture
            GLint prev = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);

            glGenTextures(1, &mFrameBufferTexture);
            glBindTexture(GL_TEXTURE_2D, mFrameBufferTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mFrameBufferTexture, 0);

            // Reset to prev;
            glBindTexture(GL_TEXTURE_2D, prev);

            // Config shader
            glUniform1i(glGetUniformLocation(mProgram, "textureWidth"), width);
            glUniform1i(glGetUniformLocation(mProgram, "textureHeight"), height);
        }
        // glBindRenderbuffer(GL_RENDERBUFFER, mRenderBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffer);
    }
    Error onSinkPush(View<Pad>, View<Resource> resource) override {
        auto frame = resource.viewAs<GLFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        // Check kernel is N-op
        if (mIsNopKernel) {
            return pushTo(mSource, frame);
        }

        NEKO_ASSERT(frame->format() == GLFrame::RGBA);

        glUseProgram(mProgram);
        glBindVertexArray(mVertexArray);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, frame->texture(0));

        _bindFrame(frame);

        // Do Compute
        int width = frame->width();
        int height = frame->height();       
        glViewport(0, 0, width, height);
        DispatchGLCompute(this);

        // Copy to texture
        auto glframe = createGLFrame(width, height, GLFrame::RGBA);

        GLuint dstTexture = glframe->texture(0);
        glBindTexture(GL_TEXTURE_2D, dstTexture);

        // Begin Copy
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, width, height, 0);
        
        glframe->setTimestamp(frame->timestamp());
        glframe->setDuration(frame->duration());

        // Compute End, cleanup
        glUseProgram(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);

        // Push to next element
        return pushTo(mSource, glframe);
    }
private:
    static constexpr float mNopKernel[3][3] {
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    GLuint mProgram = 0;
    float mKernel[3][3];

    // FrameBuffer
    int mFrameBufferWidth = 0;
    int mFrameBufferHeight = 0;
    GLuint mFrameBuffer = 0;
    GLuint mFrameBufferTexture = 0;

    GLuint mVertexArray = 0;
    GLuint mBuffer = 0;
    bool mIsNopKernel = true;

    Pad *mSink = addInput("sink");
    Pad *mSource = addOutput("src");
};

NEKO_REGISTER_ELEMENT(GLKernel, GLKernelImpl);

NEKO_NS_END