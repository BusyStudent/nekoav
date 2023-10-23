#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QTextEdit>
#include <QLabel>
#include <iostream>
#include "../nekoav/ffmpeg/factory.hpp"
#include "../nekoav/backtrace.hpp"
#include "../nekoav/format.hpp"
#include "../nekoav/media.hpp"
#include "../nekoav/log.hpp"

#ifdef _MSC_VER
    #pragma comment(linker, "/subsystem:console")
#endif

using namespace NekoAV;

class ImageSink : public MediaElement {
public:
    ImageSink(QLabel *label) : mLabel(label) {
        auto pad = addInput("sink");
        pad->property(MediaProperty::PixelFormatList) = Property::newList();
        pad->property(MediaProperty::PixelFormatList).push_back(PixelFormat::RGBA);
    }
    Error init() override {
        mController = graph()->queryInterface<MediaController>();
        return Error::Ok;
    }
    Error processInput(Pad&, ResourceView view) override {
        QMetaObject::invokeMethod(mLabel, [this, object = view->shared_from_this<MediaFrame>()]() {
            auto mediaFrame = object;

            if (!mediaFrame) {
                return;
            }
            if (mediaFrame->pixelFormat() != PixelFormat::RGBA) {
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

            if (mController) {
                mLabel->window()->setWindowTitle(QString::number(mController->masterClock()->position()));
            }
        }, Qt::BlockingQueuedConnection);
        return Error::Ok;
    }
    MediaController *mController;
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

    auto aqueue = CreateMediaQueue().release();
    auto vqueue = CreateMediaQueue().release();

    auto adecoder = factory->createElement<Decoder>().release();
    auto aconverter = factory->createElement<AudioConverter>().release();
    auto apresenter = factory->createElement<AudioPresenter>().release();

    auto vdecoder = factory->createElement<Decoder>().release();
    auto vconverter = factory->createElement<VideoConverter>().release();
    auto sink = new ImageSink(ltb);

    demuxer->setLoadedCallback([&]() {
        NEKO_DEBUG(*demuxer);
        for (auto pad : demuxer->outputs()) {
            if (pad->isVideo()) {
                demuxer->linkWith(pad->name(), vqueue, "sink");
                break;
            }
        }
        for (auto pad : demuxer->outputs()) {
            if (pad->isAudio()) {
                demuxer->linkWith(pad->name(), aqueue, "sink");
                break;
            }
        }
    });


    vqueue->linkWith("src", vdecoder, "sink");
    vdecoder->linkWith("src", vconverter, "sink");
    vconverter->linkWith("src", sink, "sink");


    aqueue->linkWith("src", adecoder, "sink");
    adecoder->linkWith("src", aconverter, "sink");
    aconverter->linkWith("src", apresenter, "sink");

    // graph.addElement(demuxer);

    // graph.addElement(vdecoder);
    // graph.addElement(vconverter);
    // graph.addElement(sink);

    // graph.addElement(adecoder);
    // graph.addElement(aconverter);
    // graph.addElement(apresenter);
    graph.addElements(demuxer, vdecoder, vconverter, sink, adecoder, aconverter, apresenter);
    graph.addElements(aqueue, vqueue);

    std::cout << graph.toDocoument() << std::endl;

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