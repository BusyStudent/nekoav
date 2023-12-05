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

class GLUploadImpl final : public OpenGLImpl<GLUpload, GLFunctions_2_1> {
public:
    GLUploadImpl() {
        mSink = addInput("sink");
        mSrc = addOutput("src");

        mSink->addProperty(Properties::SampleFormatList, {
            PixelFormat::RGBA,
            PixelFormat::NV12,
            PixelFormat::NV21,
            PixelFormat::YUV420P,
        });
    }
    Error onGLInitialize() override {
        makeCurrent();
        
        load(functionLoader());
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
        // // Update frame
        Arc<GLFrame> glframe;
        switch (frame->pixelFormat()) {
            case PixelFormat::RGBA: {
                // Raw RGBA Data
                glframe = CreateGLFrame(glcontext(), width, height, GLFrame::RGBA);

                GLuint tex;
                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);

                // Update it
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame->data(0));

                // End
                glBindTexture(GL_TEXTURE_2D, 0);

                glframe->addTexture(tex);
                glframe->setDuration(frame->duration());
                glframe->setTimestamp(frame->timestamp());
                break;
            }
            default : return Error::UnsupportedFormat;
        }
        mSrc->push(glframe);
        return Error::Ok;
    }
private:
    Pad *mSink;
    Pad *mSrc;

    GLuint mYUVCvtTextures[4] {0}; //< Textures used to convert YUV TO RGB
};

NEKO_REGISTER_ELEMENT(GLUpload, GLUploadImpl);

NEKO_NS_END