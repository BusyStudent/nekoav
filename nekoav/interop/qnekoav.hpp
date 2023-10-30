#pragma once

#include "../video/renderer.hpp"
#include "../format.hpp"
#include "../media.hpp"
#include <QMetaObject>
#include <QPainter>
#include <QWidget>
#include <QDebug>
#include <latch>

NEKO_NS_BEGIN

/**
 * @brief A Display widget for Qt
 * 
 */
class QNekoVideoWidget final : public QWidget, public VideoRenderer {
public:
    QNekoVideoWidget(QWidget *parent = nullptr) : QWidget(parent) {

    }
    ~QNekoVideoWidget() {

    }
private:
    Error init() override {
        return Error::Ok;
    }
    Error teardown() override {
        return Error::Ok;
    }
    Error sendFrame(View<MediaFrame> frame) override {
        if (mCancel) {
            // Cancel prev frames
            *mCancel = true;
        }
        mCancel = make_shared<Atomic<bool> > (false);
        mFrame = frame->shared_from_this<MediaFrame>();
        
        QMetaObject::invokeMethod(
            this,
            [this, cancel = mCancel]() {
                if (*cancel) {
                    qDebug() << "QNekoVideoWidget drop a video frame";
                    return;
                }
                updateImage(mFrame.load());
        }, Qt::QueuedConnection);
        return Error::Ok;
    }
    std::vector<PixelFormat> supportedFormats() const override {
        return {
            PixelFormat::RGBA
        };
    }
    void updateImage(View<MediaFrame> frame) {
        if (frame.empty()) {
            mPixmap = QPixmap();
            update();
            return;
        }
        int width = frame->width();
        int height = frame->height();
        // if (mImage.isNull() || mImage.width() != width || mImage.height() != height) {
        //     // Create a new Image
        //     mImage = QImage(width, height, QImage::Format_RGB32);
        // }

        // // Copy pixels into it
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

        update();
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        // painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.fillRect(0, 0, width(), height(), Qt::black);
        if (mPixmap.isNull()) {
            return;
        }
        qreal texWidth = mPixmap.width();
        qreal texHeight = mPixmap.height();

        qreal winWidth = width();
        qreal winHeight = height();

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

        QRectF r(x, y, w, h);

        painter.drawPixmap(r, mPixmap, QRectF(0, 0, texWidth, texHeight));
    }

    QPixmap mPixmap; //< Current Image
    Arc<Atomic<bool> > mCancel;
    Atomic<Arc<MediaFrame> > mFrame; //< Current media frame
};
class QNekoMediaPlayer : public QObject {
public:
    enum PlaybackState {
        StoppedState,
        PlayingState,
        PausedState
    };
    Q_ENUM(PlaybackState)

    enum MediaStatus {
        NoMedia,
        LoadingMedia,
        LoadedMedia,
        StalledMedia,
        BufferingMedia,
        BufferedMedia,
        EndOfMedia,
        InvalidMedia
    };
    Q_ENUM(MediaStatus)

    enum Error {
        NoError,
        ResourceError,
        FormatError,
        NetworkError,
        AccessDeniedError,
        UnknownError
    };
    Q_ENUM(Error)

    enum Loops {
        Infinite = -1,
        Once = 1
    };
    Q_ENUM(Loops)
    QNekoMediaPlayer(QObject *parent) : QObject(parent) {
        mPipeline.setGraph(&mGraph);
    }
    ~QNekoMediaPlayer() {
        
    }
private:
    Graph         mGraph;
    MediaPipeline mPipeline;
};

NEKO_NS_END

using NEKO_NAMESPACE::QNekoVideoWidget;