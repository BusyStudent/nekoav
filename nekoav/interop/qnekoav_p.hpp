#pragma once
#include "../elements/videosink.hpp"
#include "../format.hpp"
#include "../media.hpp"
#include "qnekoav.hpp"
#include <QMetaObject>
#include <QWidget>

namespace QNekoAV {

using namespace NEKO_NAMESPACE;
using NEKO_NAMESPACE::Arc;

class VideoWidgetPrivate : public NEKO_NAMESPACE::VideoRenderer {
public:
    virtual ~VideoWidgetPrivate() = default;
    virtual QWidget *widget() = 0;
    inline QSize videoSize() const noexcept {
        return mSize;
    }
protected:
    virtual void updateFrame(const Arc<MediaFrame> &frame) = 0;
    virtual void clearFrame() = 0;
    
    Error setContext(Context *ctxt) override {
        return Error::Ok;
    }

    Vec<PixelFormat> mPixelFormats {PixelFormat::RGBA};
private:
    Error setFrame(View<MediaFrame> frame) override final {
        if (!frame) {
            // Null 
            mFrame = nullptr;
            mSize = QSize(1, 1);
            QMetaObject::invokeMethod(widget(), [this]() {
                clearFrame();
            }, Qt::QueuedConnection);
            return Error::Ok;
        }
        mFrame = frame->shared_from_this<MediaFrame>();
        mSize = QSize(mFrame->width(), mFrame->height());
        QMetaObject::invokeMethod(widget(), [f = Weak<MediaFrame>(mFrame), this]() {
            auto frame = f.lock();
            if (!frame) {
                return;
            }
            updateFrame(frame);
        }, Qt::QueuedConnection);
        return Error::Ok;
    }
    PixelFormatList supportedFormats() override {
        return mPixelFormats;
    }

    Arc<MediaFrame> mFrame;
    Atomic<QSize>   mSize { QSize(1, 1) };
};

extern VideoWidgetPrivate *CreateVideoWidgetPrivate(VideoWidget *parent);

}