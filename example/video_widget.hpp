#pragma once

#include <nekoav/elements/video.hpp>
#include <QMetaObject>
#include <QRhiWidget>
#include <QPointer>
#include <QWidget>
#include <mutex>
#include <rhi/qrhi.h>

// For the video output
class VideoWidget final : public QRhiWidget {
public:
    VideoWidget(QWidget *parent = nullptr) : QRhiWidget(parent), mRenderer(std::make_shared<Proxy>()) {
        mRenderer->widget = this;
    }

    ~VideoWidget(){
        mRenderer->widget = nullptr;
    }

    auto renderer() -> nekoav::VideoRenderer::Ptr {
        return mRenderer;
    }
protected:
    class Proxy final : public nekoav::VideoRenderer {
    public:
        // VideoRenderer
        auto init() -> ilias::IoTask<void> override {
            co_return {};
        }

        auto shutdown() -> ilias::IoTask<void> override {
            {
                std::lock_guard locker{mtx};
                frame = {};
            }
            if (widget) {
                QMetaObject::invokeMethod(widget, &VideoWidget::submitFrame, Qt::QueuedConnection);
            }

            co_return {};
        }

        auto render(nekoav::VideoFrame f) -> ilias::IoTask<void> override {
            {
                std::lock_guard locker{mtx};
                frame = std::move(f);
            }
            if (widget) {
                QMetaObject::invokeMethod(widget, &VideoWidget::submitFrame, Qt::QueuedConnection);
            }
            co_return {};
        }

        auto pixelFormats() const -> std::vector<nekoav::PixelFormat> override {
            return { nekoav::PixelFormat::RGBA };
        }

        QPointer<VideoWidget> widget;
        nekoav::VideoFrame frame; // The current frame
        std::mutex mtx;
    };

    struct Vertex {
        float x;
        float y;
        float u;
        float v;
    };

    // QWidget
    auto resizeEvent(QResizeEvent *event) -> void override {
        mBufferDirty = true; // Recalc the vertex
        QRhiWidget::resizeEvent(event);
    }

    // QRhiWidget
    auto initialize(QRhiCommandBuffer *cb) -> void override {
        if (mRhi != rhi()) { // Query the rhi
            mRhi = rhi();
            releaseResources();
            qDebug() << "Initialize Rhi";
        }
        if (mRpDesc != renderTarget()->renderPassDescriptor()) { // Query the render pass
            mRpDesc = renderTarget()->renderPassDescriptor();
            mPipeline.reset();
            qDebug() << "Rebuild Rhi Pipeline";
        }
    }

    auto render(QRhiCommandBuffer *cb) -> void override {
        if (!mRhi) {
            return;
        }

        // Cache the update command
        QRhiResourceUpdateBatch *updates = nullptr;
        auto getUpdates = [&]() {
            if (!updates) {
                updates = mRhi->nextResourceUpdateBatch();
            }
            return updates;
        };

        // Check buffer
        if (!mVbuf || mBufferDirty) {
            ensureBuffer(getUpdates());
            mBufferDirty = false;
        }

        // Check Texture
        if (!mTexture || mTextureDirty) {
            ensureTexture(getUpdates());
            mTextureDirty = false;
        }

        // Check Pipeline
        if (!mPipeline && mSrb) {
            ensurePipeline();
        }

        // Begin draw
        cb->beginPass(renderTarget(), Qt::black, { 1.0f, 0 }, updates);
        if (mPipeline && mTexture) {
            const auto size = renderTarget()->pixelSize();

            cb->setGraphicsPipeline(mPipeline.get());
            cb->setViewport(QRhiViewport {
                0, 0, float(size.width()), float(size.height())
            });

            cb->setShaderResources(mSrb.get());

            const QRhiCommandBuffer::VertexInput vbufBinding {
                mVbuf.get(),
                0
            };

            cb->setVertexInput(0, 1, &vbufBinding);
            cb->draw(6);
        }
        cb->endPass();
    }

    auto ensureBuffer(QRhiResourceUpdateBatch *updates) -> void {
        std::array<Vertex, 6> vertices {};
        if (!mVbuf) {
            mVbuf.reset(mRhi->newBuffer(
                QRhiBuffer::Dynamic,
                QRhiBuffer::VertexBuffer,
                sizeof(vertices)
            ));
            if (!mVbuf->create()) {
                qWarning() << "Failed to create vertex buffer";
                mVbuf.reset();
                return;
            }
        }

        // Update vertex buffer
        if (!mFrame) {
            return;
        }

        auto screenSize = renderTarget()->pixelSize();
        auto scaledSize = QSize {
            mFrame.width(),
            mFrame.height()
        }.scaled(screenSize, mAspectRatio);

        QRect r { QPoint{0, 0}, scaledSize};
        r.moveCenter(QRect { QPoint{0, 0}, screenSize }.center());

        float sw = float(screenSize.width());
        float sh = float(screenSize.height());

        float x0 =  2.0f * float(r.left()) / sw - 1.0f;
        float x1 =  2.0f * float(r.left() + r.width()) / sw - 1.0f;

        float y0 = 1.0f - 2.0f * float(r.top() + r.height()) / sh;
        float y1 = 1.0f - 2.0f * float(r.top()) / sh;

        vertices = {{
            { x0, y0, 0.0f, 1.0f },
            { x1, y0, 1.0f, 1.0f },
            { x0, y1, 0.0f, 0.0f },

            { x1, y0, 1.0f, 1.0f },
            { x1, y1, 1.0f, 0.0f },
            { x0, y1, 0.0f, 0.0f },
        }};

        updates->updateDynamicBuffer(
            mVbuf.get(),
            0,
            sizeof(vertices),
            vertices.data()
        );
    }

    auto ensureTexture(QRhiResourceUpdateBatch *updates) -> void {
        if (!mFrame) {
            return;
        }
        if (mTexture && mTexture->pixelSize() != QSize {mFrame.width(), mFrame.height()}) { // Texture size changed. rebuild it
            mTexture.reset();
            mSampler.reset();
            mSrb.reset();
            mPipeline.reset();
        }
        if (!mTexture) {
            QSize size {
                mFrame.width(),
                mFrame.height()
            };

            mTexture.reset(mRhi->newTexture(
                QRhiTexture::RGBA8,
                size
            ));

            if (!mTexture->create()) {
                qWarning() << "Failed to create texture";
                mTexture.reset();
                return;
            }
        }

        if (!mSampler) {
            mSampler.reset(mRhi->newSampler(
                QRhiSampler::Linear,
                QRhiSampler::Linear,
                QRhiSampler::None,
                QRhiSampler::ClampToEdge,
                QRhiSampler::ClampToEdge
            ));

            if (!mSampler->create()) {
                qWarning() << "Failed to create sampler";
                mSampler.reset();
                mTexture.reset();
                return;
            }
        }

        if (!mSrb) {
            mSrb.reset(mRhi->newShaderResourceBindings());
            mSrb->setBindings({
                QRhiShaderResourceBinding::sampledTexture(
                    0,
                    QRhiShaderResourceBinding::FragmentStage,
                    mTexture.get(),
                    mSampler.get()
                )
            });

            if (!mSrb->create()) {
                qWarning() << "Failed to create shader resource bindings";
                mSrb.reset();
                mTexture.reset();
                return;
            }
        }

        // Update the texture
        QImage image {
            (uchar*) mFrame.data(0),
            mFrame.width(),
            mFrame.height(),
            mFrame.linesize(0),
            QImage::Format_RGBA8888
        };
        updates->uploadTexture(mTexture.get(), image);
    }

    auto ensurePipeline() -> void {
        if (mPipeline) {
            return;
        }


        const auto vs = loadShader(QStringLiteral(":/shaders/vert.qsb"));
        const auto fs = loadShader(QStringLiteral(":/shaders/frag.qsb"));

        if (!vs.isValid() || !fs.isValid()) {
            return;
        }

        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            QRhiVertexInputBinding(sizeof(Vertex))
        });
        inputLayout.setAttributes({
            QRhiVertexInputAttribute(
                0,
                0,
                QRhiVertexInputAttribute::Float2,
                offsetof(Vertex, x)
            ),
            QRhiVertexInputAttribute(
                0,
                1,
                QRhiVertexInputAttribute::Float2,
                offsetof(Vertex, u)
            )
        });

        mPipeline.reset(mRhi->newGraphicsPipeline());
        mPipeline->setShaderStages({
            QRhiShaderStage(QRhiShaderStage::Vertex, vs),
            QRhiShaderStage(QRhiShaderStage::Fragment, fs)
        });
        mPipeline->setVertexInputLayout(inputLayout);
        mPipeline->setShaderResourceBindings(mSrb.get());
        mPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        mPipeline->setSampleCount(renderTarget()->sampleCount());

        if (!mPipeline->create()) {
            mPipeline.reset();
            qWarning() << "Failed to create pipeline";
            return;
        }

        return;
    }

    auto releaseResources() -> void override {
        mPipeline.reset();
        mTexture.reset();
        mSampler.reset();
        mSrb.reset();
        mVbuf.reset();
        mTextureDirty = true;
        QRhiWidget::releaseResources();
    }

    auto submitFrame() -> void {
        {
            std::lock_guard locker {mRenderer->mtx};
            mFrame = std::exchange(mRenderer->frame, {});
        }
        mTextureDirty = true;
        mBufferDirty = true;
        repaint();
    }


    auto loadShader(const QString &fileName) -> QShader {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open shader file:" << fileName;
            return {};
        }
        return QShader::fromSerialized(file.readAll());
    }
private:
    QRhi *mRhi = nullptr;
    const QRhiRenderPassDescriptor *mRpDesc = nullptr; // To which we render

    std::unique_ptr<QRhiBuffer> mVbuf;
    std::unique_ptr<QRhiTexture> mTexture;
    std::unique_ptr<QRhiSampler> mSampler;
    std::unique_ptr<QRhiShaderResourceBindings> mSrb;
    std::unique_ptr<QRhiGraphicsPipeline> mPipeline;

    // Frame cache
    Qt::AspectRatioMode mAspectRatio = Qt::KeepAspectRatio;
    bool mTextureDirty = true;
    bool mBufferDirty = true;
    nekoav::VideoFrame mFrame;

    // Renderer
    std::shared_ptr<Proxy> mRenderer;
};
