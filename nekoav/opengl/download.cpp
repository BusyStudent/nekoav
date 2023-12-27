#define _NEKO_SOURCE
#include "../media/frame.hpp"
#include "../factory.hpp"
#include "../format.hpp"

#include "opengl_func.hpp"
#include "template.hpp"
#include "download.hpp"

NEKO_NS_BEGIN

class GLDownloadImpl final : public OpenGLImpl<GLDownload, GLFunctions_2_0> {
public:
    GLDownloadImpl() {
        
    }
    ~GLDownloadImpl() {

    }

    Error onSinkPush(View<Pad>, View<Resource> resource) override {
        auto frame = resource.viewAs<GLFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        auto swframe = CreateVideoFrame(PixelFormat::RGBA, frame->width(), frame->height());
        swframe->setTimestamp(frame->timestamp());
        swframe->setDuration(frame->duration());
        swframe->makeWritable();
        // Copy back
        glBindTexture(GL_TEXTURE_2D, frame->texture(0));
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, swframe->data(0));
        glBindTexture(GL_TEXTURE_2D, 0);

        // Push
        return pushTo(mSource, swframe);
    }
private:
    Pad *mSink = addInput("sink");
    Pad *mSource = addOutput("src");
};

NEKO_REGISTER_ELEMENT(GLDownload, GLDownloadImpl);

NEKO_NS_END