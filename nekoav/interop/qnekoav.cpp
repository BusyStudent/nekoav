#ifndef _QNEKO_SOURCE
    #define _QNEKO_SOURCE
#endif

#include "qnekoav.hpp"
#include "../elements/videosink.hpp"
#include "../pipeline.hpp"
#include "../factory.hpp"
#include "../format.hpp"
#include "../media.hpp"

#if __has_include(<QOpenGLWidget>)
    #include <QOpenGLWidget>
#endif

#include <QPainter>
#include <QPixmap>

namespace QNekoAV {

using namespace NEKO_NAMESPACE;
using NEKO_NAMESPACE::Arc;

class MediaPlayerPrivate {
public:
    Arc<Pipeline> mPipeline;
};


MediaPlayer::MediaPlayer(QObject *parent) : QObject(parent) {
    d = new MediaPlayerPrivate;
}
MediaPlayer::~MediaPlayer() {
    delete d;
}
void MediaPlayer::play() {

}
void MediaPlayer::pause() {

}
void MediaPlayer::stop() {

}
void MediaPlayer::load() {

}

void MediaPlayer::setPosition(qreal position) {

}

void MediaPlayer::setPlaybackRate(qreal rate) {

}

void MediaPlayer::setSource(const QUrl &source) {

}
void MediaPlayer::setSourceDevice(QIODevice *device, const QUrl &sourceUrl) {
    
}


class VideoWidgetPrivate : public VideoRenderer {
public:
    virtual ~VideoWidgetPrivate() = default;
    virtual QWidget *widget() = 0;
    virtual QSize    videoSize() = 0;
};

class QPainterWidget final : public QWidget, public VideoWidgetPrivate {
public:
    QPainterWidget(QWidget *parent) : QWidget(parent) {}

    // VideoWidgetPrivate
    QWidget *widget() override {
        return this;
    }
    QSize videoSize() override {
        if (mPixmap.isNull()) {
            return QSize(1, 1);
        }
        return mPixmap.size();
    }

    // VideoRenderer
    Error setFrame(View<MediaFrame> frame) override {
        if (!frame) {
            // Null 
            mFrame = nullptr;
            QMetaObject::invokeMethod(this, [this]() {
                mPixmap = QPixmap();
                update();
            }, Qt::QueuedConnection);
            return Error::Ok;
        }
        mFrame = frame->shared_from_this<MediaFrame>();
        QMetaObject::invokeMethod(this, [f = Weak<MediaFrame>(mFrame), this]() {
            updateFrame(f);
        }, Qt::QueuedConnection);
        return Error::Ok;
    }
    Vec<PixelFormat> supportedFormats() override {
        Vec<PixelFormat> vec;
        vec.push_back(PixelFormat::RGBA);
        return vec;
    }
    void updateFrame(const Weak<MediaFrame> &weakFrame) {
        auto frame = weakFrame.lock();
        if (!frame) {
            return;
        }
        NEKO_ASSERT(frame->pixelFormat() == PixelFormat::RGBA);
        int width = frame->width();
        int height = frame->height();

        bool notifyRelayout = false;
        if (mPixmap.isNull()) {
            notifyRelayout = true;
        }

        QImage image(width, height, QImage::Format_RGBA8888);
        int srcPitch = frame->linesize(0);
        int dstPitch = image.bytesPerLine();
        if (srcPitch != dstPitch) {
            // Update it
            auto dst = image.bits();
            auto pixels = (uint8_t*) frame->data(0);
            for (int y = 0; y < image.height(); y++) {
                for (int x = 0; x < image.width(); x++) {
                    *((uint32_t*)   &dst[y * dstPitch + x * 4]) = *(
                        (uint32_t*) &pixels[y * srcPitch + x * 4]
                    );
                }
            }
        }
        else {
            ::memcpy(image.bits(), frame->data(0), width * height * 4);
        }
        mPixmap = QPixmap::fromImage(std::move(image));
        if (notifyRelayout) {
            static_cast<VideoWidget*>(parent())->internalRelayout();
        }
        update();
    }

    // QWidget
    void paintEvent(QPaintEvent *event) override {
        if (!mPixmap.isNull()) {
            QPainter painter(this);
            painter.drawPixmap(0, 0, width(), height(), mPixmap);
        }
    }

    Arc<MediaFrame> mFrame;
    QPixmap         mPixmap;
};

VideoWidget::VideoWidget(QWidget *parent) : QWidget(parent) {
    // Check env
    d = new QPainterWidget(this);

    // Setup
}
VideoWidget::~VideoWidget() {
    delete d;
}
void VideoWidget::internalRelayout() {
    auto [texWidth, texHeight] = d->videoSize();
    auto [winWidth, winHeight] = size();

    qreal x, y, w, h;

    if(texWidth * winHeight > texHeight * winWidth){
        w = winWidth;
        h = texHeight * winWidth / texWidth;
    }
    else{
        w = texWidth * winHeight / texHeight;
        h = winHeight;
    }
    x = (winWidth - w) / 2;
    y = (winHeight - h) / 2;

    auto dstRect = QRect(x, y, w, h);
    d->widget()->setGeometry(dstRect);
}
void VideoWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    internalRelayout();
}
void VideoWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.fillRect(0, 0, width(), height(), Qt::black);
}
void *VideoWidget::videoRenderer() {
    return static_cast<VideoRenderer*>(d);
}


}