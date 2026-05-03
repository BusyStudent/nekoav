#pragma once

#include <nekoav/elements/video.hpp>
#include <QRhiWidget>
#include <QPainter>
#include <QWidget>
#include <rhi/qrhi.h>

// For the video output
class VideoWidget final : public QRhiWidget, public nekoav::VideoRenderer {
public:
    VideoWidget(QWidget *parent = nullptr) : QRhiWidget(parent) {
        
    }

    auto renderer() -> nekoav::VideoRenderer::Ptr {
        return { // Not shared, take care of the ownership
            this, [](auto *) {}
        };
    }
protected:
    struct Vertex {
        float x;
        float y;
        float u;
        float v;
    };

    // VideoRenderer
    auto init() -> ilias::IoTask<void> override {
        co_return {};
    }

    auto shutdown() -> ilias::IoTask<void> override {
        mFrame = {};
        co_return {};
    }

    auto render(nekoav::Frame frame) -> ilias::IoTask<void> override {
        mFrame = std::move(frame);
        mTextureDirty = true;
        update();
        co_return {};
    }

    auto pixelFormats() const -> std::vector<nekoav::PixelFormat> override {
        return { nekoav::PixelFormat::RGBA };
    }

    // QRhiWidget
    auto initialize(QRhiCommandBuffer *cb) -> void override {
        if (mRhi != rhi()) { // Query the rhi
            mRhi = rhi();
            releaseResources();
        }
        if (mRpDesc != renderTarget()->renderPassDescriptor()) { // Query the render pass
            mRpDesc = renderTarget()->renderPassDescriptor();
            mPipeline.reset();
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

    // Pipeline
    auto ensureBuffer(QRhiResourceUpdateBatch *updates) -> void {
        if (mVbuf) {
            return;
        }
    
        // 直接铺满整个 widget。
        // 如果画面上下反了，把 v 的 0/1 对调即可。
        constexpr std::array<Vertex, 6> vertices {{
            { -1.0f, -1.0f, 0.0f, 1.0f },
            {  1.0f, -1.0f, 1.0f, 1.0f },
            { -1.0f,  1.0f, 0.0f, 0.0f },

            {  1.0f, -1.0f, 1.0f, 1.0f },
            {  1.0f,  1.0f, 1.0f, 0.0f },
            { -1.0f,  1.0f, 0.0f, 0.0f },
        }};

        mVbuf.reset(mRhi->newBuffer(
            QRhiBuffer::Immutable,
            QRhiBuffer::VertexBuffer,
            int(sizeof(vertices))
        ));

        if (!mVbuf->create()) {
            qWarning() << "Failed to create vertex buffer";
            mVbuf.reset();
            return;
        }

        updates->uploadStaticBuffer(mVbuf.get(), vertices.data());
    }

    auto ensureTexture(QRhiResourceUpdateBatch *updates) -> void {
        if (mFrame == nekoav::Frame {}) {
            return;
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

    bool mTextureDirty = true;
    bool mBufferDirty = true;
    nekoav::Frame mFrame;
};
