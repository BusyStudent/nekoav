#pragma once

#include <nekoav/elements/video.hpp>
#include <QPainter>
#include <QWidget>

// For the video output
class VideoWidget final : public QWidget, public nekoav::VideoRenderer {
public:
    VideoWidget(QWidget *parent = nullptr) : QWidget(parent) {
        
    }

    // VideoRenderer
    auto init() -> ilias::IoTask<void> override {
        co_return {};
    }

    auto shutdown() -> ilias::IoTask<void> override {
        mPixmap = {};
        repaint();
        co_return {};
    }

    auto render(nekoav::Frame frame) -> ilias::IoTask<void> override {
        // auto image = QImage::fromData(frame.data(0), );
        auto image = QImage {
            reinterpret_cast<const uchar *>(frame.data(0)),
            frame.width(),
            frame.height(),
            frame.linesize(0),
            QImage::Format_RGBA8888
        };
        mPixmap = QPixmap::fromImage(image);
        repaint();
        co_return {};
    }

    auto pixelFormats() const -> std::vector<nekoav::PixelFormat> override {
        return { nekoav::PixelFormat::RGBA };
    }

    auto renderer() -> nekoav::VideoRenderer::Ptr {
        return { // Not shared, take care of the ownership
            this, [](auto *) {}
        };
    }
protected:
    auto paintEvent(QPaintEvent *event) -> void override {
        QPainter painter {this};
        painter.fillRect(rect(), Qt::black);

        if (mPixmap.isNull()) {
            return;
        }
        // Calculate the rect of the video
        QSize scaledSize = mPixmap.size().scaled(this->size(), Qt::KeepAspectRatio);
        QRect r {
            QPoint {0, 0}, scaledSize
        };
        r.moveCenter(this->rect().center());
        
        // Draw it
        painter.drawPixmap(r, mPixmap);
    }
private:
    QPixmap mPixmap;
};
