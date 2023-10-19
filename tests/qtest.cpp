#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QTextEdit>
#include <QLabel>
#include "../nekoav/ffmpeg/factory.hpp"
#include "../nekoav/backtrace.hpp"
#include "../nekoav/format.hpp"
#include "../nekoav/media.hpp"

using namespace NekoAV;

class ImageSink : public MediaElement {
public:
    ImageSink(QLabel *label) : mLabel(label) {
        auto pad = addInput("sink");
        pad->property("pixfmts") = Property::newList();
        pad->property("pixfmts").push_back(PixelFormat::RGBA);
    }
    Error processInput(Pad&, ResourceView view) override {
        QMetaObject::invokeMethod(mLabel, [this, object = view->shared_from_this<MediaFrame>()]() {
            auto mediaFrame = object;

            if (!mediaFrame) {
                return;
            }
            if (mediaFrame->pixfmt() != PixelFormat::RGBA) {
                return;
            }
            int width = mediaFrame->width();
            int height = mediaFrame->height();
            int pitch = mediaFrame->linesize(0);
            void *data = mediaFrame->data(0);

            // Update it
            
            QImage image(width, height, QImage::Format_RGBA8888);

            auto dst = (uint32_t*) image.bits();
            auto pixels = (uint32_t*) data;
            auto dstPitch = image.bytesPerLine();

            memcpy(dst,  pixels, width * height * 4);

            mLabel->setPixmap(QPixmap::fromImage(image));
            mLabel->adjustSize();
            mLabel->update();
        }, Qt::BlockingQueuedConnection);
        return Error::Ok;
    }
    QLabel *mLabel;
};

int main(int argc, char **argv) {
    InstallCrashHandler();
    
    QApplication app(argc, argv);
    QMainWindow win;

    auto widget = new QWidget(&win);
    win.setCentralWidget(widget);

    auto layout = new QVBoxLayout(widget);
    auto btn = new QPushButton("OpenFile");
    auto ltb = new QLabel();
    layout->addWidget(btn);
    layout->addWidget(ltb);

    Graph graph;
    MediaPipeline pipeline;
    pipeline.setGraph(&graph);

    // Init Graph
    auto factory = GetFFmpegFactory();
    auto demuxer = factory->createElement<Demuxer>().release();
    auto decoder = factory->createElement<Decoder>().release();
    auto converter = factory->createElement<VideoConverter>().release();
    auto sink = new ImageSink(ltb);

    demuxer->setLoadedCallback([&]() {
        for (auto pad : demuxer->outputs()) {
            if (pad->name().find("video") == 0) {
                demuxer->linkWith(pad->name().data(), decoder, "sink");
            }
        }
    });

    decoder->linkWith("src", converter, "sink");
    converter->linkWith("src", sink, "sink");

    graph.addElement(demuxer);
    graph.addElement(decoder);
    graph.addElement(converter);
    graph.addElement(sink);

    QObject::connect(btn, &QPushButton::clicked, [&](bool) {
        auto url = QFileDialog::getOpenFileUrl(nullptr, "Open File");
        if (url.isValid()) {
            demuxer->setSource(url.toLocalFile().toUtf8().constData());
            pipeline.setState(State::Stopped);
            pipeline.setState(State::Running);
        }
    });

    win.show();
    return app.exec();
}