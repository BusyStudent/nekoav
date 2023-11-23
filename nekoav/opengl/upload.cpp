#define _NEKO_SOURCE
#include "../threading.hpp"
#include "../factory.hpp"
#include "../format.hpp"
#include "../media.hpp"
#include "../pad.hpp"

#include "opengl_func.hpp"
#include "template.hpp"
#include "upload.hpp"

NEKO_NS_BEGIN

class GLUploadImpl final : public Template::GetGLImpl<GLUpload, GLFunctions_2_1> {
public:
    GLUploadImpl() {
        mSink = addInput("sink");
        mSrc = addOutput("src");

        // Just forward 
        mSink->setEventCallback(std::bind(&Pad::pushEvent, mSrc, std::placeholders::_1));
        mSink->setCallback([this](View<Resource> resourceView) -> Error {
            Error ret;
            thread()->sendTask([this, resourceView, &ret]() {
                ret = onSink(resourceView);
            });
            return ret;
        });
        mSink->addProperty(Properties::SampleFormatList, {
            PixelFormat::RGBA,
            PixelFormat::NV12,
            PixelFormat::NV21,
            PixelFormat::YUV420P,
        });
    }
    Error onInitialize() override {
        load([this](const char *name) {
            return glcontext()->getProcAddress(name);
        });
        return Error::Ok;
    }
    Error onTeardown() override {
        return Error::Ok;
    }
    Error onSink(View<Resource> resourceView) {
        glcontext()->makeCurrent();

        auto frame = resourceView.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        int width = frame->width();
        int height = frame->height();
        auto glframe = CreateGLFrame(glcontext(), this, width, height, frame->pixelFormat());
        // Update frame
        switch (frame->pixelFormat()) {
            case PixelFormat::RGBA: {
                NEKO_ASSERT(glframe->pixelFormat() == PixelFormat::RGBA);
                NEKO_ASSERT(glframe->textureCount() == 1);

                glBindTexture(GL_TEXTURE_2D, glframe->texture(0));
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame->data(0));
                glBindTexture(GL_TEXTURE_2D, 0);

                break;
            }
        }
        mSrc->push(glframe);
        return Error::Ok;
    }
private:
    Pad *mSink;
    Pad *mSrc;
};

NEKO_REGISTER_ELEMENT(GLUpload, GLUploadImpl);

NEKO_NS_END