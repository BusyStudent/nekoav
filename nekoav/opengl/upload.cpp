#define _NEKO_SOURCE
#include "../threading.hpp"
#include "../factory.hpp"
#include "../format.hpp"
#include "../media.hpp"
#include "../pad.hpp"

#include "opengl_helper.hpp"
#include "opengl_func.hpp"
#include "template.hpp"
#include "upload.hpp"

#ifdef _WIN32
    #include "directx_interop.hpp"
#endif

NEKO_NS_BEGIN

enum {
    NV12Shader,
    NV21Shader,
    YUV420PShader,
    NumOfShader
};

static const char *fragShaderYUV420P = R"(
#version 330 core
out vec4 fragColor;
in  vec2 texturePos;

uniform sampler2D yTexture;
uniform sampler2D uTexture;
uniform sampler2D vTexture;

void main(){
    vec3 yuv;
    vec3 rgb;

    yuv.x = texture(yTexture, texturePos).r - 0.0625;
    yuv.y = texture(uTexture, texturePos).r - 0.5;
    yuv.z = texture(vTexture, texturePos).r - 0.5;

    rgb = mat3( 1,       1,         1,
        0,       -0.39465,  2.03211,
        1.13983, -0.58060,  0) * yuv;
    fragColor = vec4(rgb, 1);
}

)";
static const char *fragShaderNV12 = R"(
#version 330 core
out vec4 fragColor;
in  vec2 texturePos;

uniform sampler2D yTexture;
uniform sampler2D uvTexture;

void main(){
    vec3 yuv;
    vec3 rgb;

    yuv.x = texture(yTexture, texturePos).r - 0.0625;
    yuv.y = texture(uvTexture, texturePos).r - 0.5;
    yuv.z = texture(uvTexture, texturePos).g - 0.5;


    rgb = mat3( 1,       1,         1,
        0,       -0.39465,  2.03211,
        1.13983, -0.58060,  0) * yuv;
    fragColor = vec4(rgb, 1);
}

)";

class GLUploadImpl final : public OpenGLImpl<GLUpload, GLFunctions_3_0> {
public:
    GLUploadImpl() {
        mSink = addInput("sink");
        mSrc = addOutput("src");

        mSink->addProperty(Properties::SampleFormatList, {
            PixelFormat::RGBA,
            PixelFormat::NV12,
            // PixelFormat::NV21,
            PixelFormat::YUV420P,
            // FIXME : YUV4209 Shader always produce the red pixels in 0
        });
    }
    Error onGLInitialize() override {
        // Create Compute basic env
        CreateGLComputeEnv(this, &mVertexArray, &mBuffer);

        // Config shaders
        _initConvertShaders();

        // Config dx interop
#ifdef _WIN32
        mDxInterop = DXGLInterop::create(glcontext());
#endif

        return Error::Ok;
    }
    Error onGLTeardown() override {
        DeleteGLComputeEnv(this, &mVertexArray, &mBuffer);
        for (auto &program : mPrograms) {
            if (program != 0) {
                glDeleteProgram(program);
            }
            program = 0;
        }
        mVertexArray = 0;
        mBuffer = 0;

        // Release FrameBuffer (used in YUV cvt)
        // _clearFrameBuffer();

#ifdef _WIN32
        mDxInterop.reset();
#endif
        return Error::Ok;
    }
    Error onSinkPush(View<Pad> pad, View<Resource> resourceView) override {
        if (!isWorkThread()) {
            return invokeMethodQueued(&GLUploadImpl::onSinkPush, this, pad, resourceView);
        }
        makeCurrent();

        auto frame = resourceView.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        int width = frame->width();
        int height = frame->height();
        // auto glframe = CreateGLFrame(glcontext());
        auto glframe = createGLFrame(width, height, GLFrame::RGBA);
        switch (frame->pixelFormat()) {
            case PixelFormat::RGBA: {
                // Raw RGBA Data

                // Update it
                glBindTexture(GL_TEXTURE_2D, glframe->texture(0));
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, frame->data(0));
                glBindTexture(GL_TEXTURE_2D, 0);
                break;
            }
            // YUV
            case PixelFormat::NV12:
            case PixelFormat::NV21:
            case PixelFormat::YUV420P: {
                _convertYUV(glframe.get(), frame);
                break;
            }
            default : return Error::UnsupportedPixelFormat;
        }
        // Copy props
        glframe->setDuration(frame->duration());
        glframe->setTimestamp(frame->timestamp());
        return pushTo(mSrc, glframe);
    }
    void _initConvertShaders() {
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexCommonShader, nullptr);
        glCompileShader(vertexShader);

        // ---Begin
        if (auto program = _addShader(NV12Shader, vertexShader, fragShaderNV12); program) {
            // Bind NV12
            glUseProgram(program);
            glUniform1i(glGetUniformLocation(program, "yTexture"),  0);
            glUniform1i(glGetUniformLocation(program, "uvTexture"), 1);
        }
        if (auto program = _addShader(YUV420PShader, vertexShader, fragShaderYUV420P); program) {
            // Bind YUV420P
            glUseProgram(program);
            glUniform1i(glGetUniformLocation(program, "yTexture"), 0);
            glUniform1i(glGetUniformLocation(program, "uTexture"), 1);
            glUniform1i(glGetUniformLocation(program, "uTexture"), 2);
        }

        glUseProgram(0);
        // ---End
        glDeleteShader(vertexShader);
    }
    GLuint _addShader(int where, int vertexShader, const char *fragShaderCode) {
        NEKO_ASSERT(where >= 0 && where < NumOfShader);
        NEKO_ASSERT(mPrograms[where] == 0);
        
        GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(fragShader, 1, &fragShaderCode, nullptr);
        glCompileShader(fragShader);

        // Make Program
        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragShader);
        glLinkProgram(program);

        // Release shader
        glDeleteShader(fragShader);

        GLint status = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status == GL_FALSE) {
            glDeleteProgram(program);
            return 0;
        }
        mPrograms[where] = program;
        return program;
    }
    void _convertYUV(GLFrame *dstFrame, MediaFrame *srcFrame) {
        PixelFormat fmt = srcFrame->pixelFormat();
        int width = srcFrame->width();
        int height = srcFrame->height();

        _checkFrameBuffer(fmt, width, height);
        _updateYUVTexture(srcFrame);

        // Begin Convert
        glBindVertexArray(mVertexArray);
        glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffer);
        auto program = _selectProgram(fmt);
        glUseProgram(program);

        // Active texture unit
        for (int i = 0; i < mNumOfTextures; i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, mCvtTextures[i]);
        }

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, width, height);
        DispatchGLCompute(this);

        // GL Read Pixels for debug
        // Vec<uint8_t> pixels(width * height * 4);
        // glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        
        // Copy to tex
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindTexture(GL_TEXTURE_2D, dstFrame->texture(0));
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, width, height, 0);

        // Cleanup
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
    }
    void _clearFrameBuffer() {
        if (mFrameBuffer) {
            glDeleteFramebuffers(1, &mFrameBuffer);
            mFrameBuffer = 0;
        }
        if (mFrameBufferTexture) {
            glDeleteTextures(1, &mFrameBufferTexture);
            mFrameBufferTexture = 0;
        }
        if (mNumOfTextures) {
            glDeleteBuffers(mNumOfTextures, mCvtTextures);
            ::memset(mCvtTextures, 0, sizeof(mCvtTextures));
            mNumOfTextures = 0;
            mTextureWidth = 0;
            mTextureHeight = 0;
        }
        mPrevfFormat = PixelFormat::None;
    }
    void _checkFrameBuffer(PixelFormat fmt, int width, int height) {
        if (mPrevfFormat == fmt && mTextureWidth == width && mTextureHeight == height) {
            return;
        }
        _clearFrameBuffer();
        // Save
        GLint prevTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);

        // Make a new one
        glGenFramebuffers(1, &mFrameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffer);

        // Make a new framebuffer texture
        glGenTextures(1, &mFrameBufferTexture);
        glBindTexture(GL_TEXTURE_2D, mFrameBufferTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mFrameBufferTexture, 0);

        // Make cvt source texture
        auto setTexParam = [this]() {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        };

        switch (fmt) {
            case PixelFormat::YUV420P: {
                mNumOfTextures = 3;
                glGenTextures(3, mCvtTextures);

                glBindTexture(GL_TEXTURE_2D, mCvtTextures[0]);
                setTexParam();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

                glBindTexture(GL_TEXTURE_2D, mCvtTextures[1]);
                setTexParam();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

                glBindTexture(GL_TEXTURE_2D, mCvtTextures[2]);
                setTexParam();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
                break;
            }
            case PixelFormat::NV12: {
                mNumOfTextures = 2;
                glGenTextures(2, mCvtTextures);

                glBindTexture(GL_TEXTURE_2D, mCvtTextures[0]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
                setTexParam();

                glBindTexture(GL_TEXTURE_2D, mCvtTextures[1]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, width / 2, height / 2, 0, GL_RG, GL_UNSIGNED_BYTE, nullptr);
                setTexParam();
                break;
            }
            default: ::abort();
        }

        // Restore
        glBindTexture(GL_TEXTURE_2D, prevTexture);

        // Set Props
        mTextureWidth = width;
        mTextureHeight = height;
        mPrevfFormat = fmt;
    }
    void _updateYUVTexture(MediaFrame *frame) {
        int width = frame->width();
        int height = frame->height();
        switch (frame->pixelFormat()) {
            case PixelFormat::YUV420P: {
                glBindTexture(GL_TEXTURE_2D, mCvtTextures[0]);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize(0));
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, frame->data(0));

                // Vec<uint8_t> data(width * height);
                // glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, data.data());
                // NEKO_ASSERT(::memcmp(data.data(), frame->data(0), width * height) == 0);

                glBindTexture(GL_TEXTURE_2D, mCvtTextures[1]);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize(1));
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, frame->data(1));

                // glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, data.data());
                // NEKO_ASSERT(::memcmp(data.data(), frame->data(1), width * height / 4) == 0);

                glBindTexture(GL_TEXTURE_2D, mCvtTextures[2]);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->linesize(2));
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RED, GL_UNSIGNED_BYTE, frame->data(2));
                
                // glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, data.data());
                // NEKO_ASSERT(::memcmp(data.data(), frame->data(2), width * height / 4) == 0);

                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                break;
            }
            case PixelFormat::NV12: {
                glBindTexture(GL_TEXTURE_2D, mCvtTextures[0]);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, frame->data(0));
                glBindTexture(GL_TEXTURE_2D, mCvtTextures[1]);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height / 2, GL_RG, GL_UNSIGNED_BYTE, frame->data(1));
                break;
            }
            default: ::abort();
        }
    }
    GLuint _selectProgram(PixelFormat fmt) {
        switch (fmt) {
            case PixelFormat::YUV420P: return mPrograms[YUV420PShader];
            case PixelFormat::NV12: return mPrograms[NV12Shader];
            default: ::abort();
        }
    }
private:
    Pad *mSink;
    Pad *mSrc;
    
    GLuint mVertexArray = 0;
    GLuint mBuffer = 0;
    GLuint mPrograms[NumOfShader] {0}; //< All convert program

    // Convert Parts
    GLuint mFrameBuffer = 0;//< framebuffer for convert
    GLuint mFrameBufferTexture = 0; //< texture for framebuffer
    GLuint mCvtTextures[4] {0}; //< Textures used to convert YUV TO RGB
    GLuint mNumOfTextures = 0;
    GLint  mTextureWidth  = 0; //< Output texture width
    GLint  mTextureHeight = 0; //< Output texture height
    PixelFormat mPrevfFormat = PixelFormat::None; //< Prev cvt format

#ifdef _WIN32
    Box<DXGLInterop> mDxInterop;
#endif
};

NEKO_REGISTER_ELEMENT(GLUpload, GLUploadImpl);

NEKO_NS_END