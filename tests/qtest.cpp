#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QTextEdit>
#include <QSlider>
#include <QLabel>
#include <iostream>

#include "../nekoav/elements/demuxer.hpp"
#include "../nekoav/elements/decoder.hpp"
#include "../nekoav/elements/audiosink.hpp"
#include "../nekoav/elements/audiocvt.hpp"
#include "../nekoav/elements/videocvt.hpp"
#include "../nekoav/elements/videosink.hpp"
#include "../nekoav/elements/mediaqueue.hpp"
#include "../nekoav/interop/qnekoav.hpp"
#include "../nekoav/backtrace.hpp"
#include "../nekoav/pipeline.hpp"
#include "../nekoav/factory.hpp"
#include "../nekoav/format.hpp"
#include "../nekoav/event.hpp"
#include "../nekoav/media.hpp"
#include "../nekoav/pad.hpp"
#include "../nekoav/log.hpp"

#ifdef _MSC_VER
    #pragma comment(linker, "/subsystem:console")
#endif

using namespace NEKO_NAMESPACE;

int main(int argc, char **argv) {
    using NEKO_NAMESPACE::Arc;

    InstallCrashHandler();
    
    QApplication app(argc, argv);
    QMainWindow win;

    auto widget = new QWidget(&win);
    win.setCentralWidget(widget);

    auto layout = new QVBoxLayout(widget);
    auto sublay = new QHBoxLayout(widget);
    auto btn = new QPushButton("OpenFile");
    auto pauseBtn = new QPushButton("Pause");
    auto progressBar = new QSlider(Qt::Horizontal);
    auto videoWidget = new QNekoAV::VideoWidget;

    auto videoRenderer = static_cast<VideoRenderer*>(videoWidget->videoRenderer());

    layout->addLayout(sublay);
    sublay->addWidget(btn);
    sublay->addWidget(pauseBtn);
    sublay->addWidget(progressBar);
    layout->addWidget(videoWidget);

    pauseBtn->setEnabled(false);

    // Init Graph
    auto factory = GetElementFactory();

    // Build pipeline
    Arc<Pipeline> pipeline;
    Arc<Demuxer> demuxer;
    Arc<MediaQueue> aqueue;
    Arc<MediaQueue> vqueue;

    auto buildPipeline = [&]() {
        pipeline = factory->createElement<Pipeline>();
        demuxer = factory->createElement<Demuxer>();

        aqueue = factory->createElement<MediaQueue>();
        vqueue = factory->createElement<MediaQueue>();

        auto adecoder = factory->createElement<Decoder>();
        auto aconverter = factory->createElement<AudioConverter>();
        auto apresenter = factory->createElement<AudioSink>();

        auto vdecoder = factory->createElement<Decoder>();
        auto vconverter = factory->createElement<VideoConverter>();
        auto vpresenter = factory->createElement<VideoSink>();

        aqueue->setName("AudioQueue");
        vqueue->setName("VideoQueue");

        pipeline->addElements(demuxer, aqueue, adecoder, aconverter, apresenter, vqueue, vdecoder, vconverter, vpresenter);

        LinkElements(aqueue, adecoder, aconverter, apresenter);
        LinkElements(vqueue, vdecoder, vconverter, vpresenter);

        pipeline->setEventCallback([&](View<Event> event) {
            if (event->type() == Event::ClockUpdated) {
                auto time = event.viewAs<ClockEvent>()->clock()->position();
                QMetaObject::invokeMethod(&win, [t = time, &win, &progressBar]() {
                    win.setWindowTitle(QString::number(t));
                    progressBar->setValue(t);
                }, Qt::QueuedConnection);
            }
        });

        vpresenter->setRenderer(videoRenderer);
    };

    QObject::connect(btn, &QPushButton::clicked, [&](bool) {
        auto url = QFileDialog::getOpenFileUrl(nullptr, "Open File");
        if (url.isValid()) {
            Error err;
            if (pipeline) {
                err = pipeline->setState(State::Null);
            }
            // Rebuild it
            buildPipeline();
            demuxer->setUrl(url.toLocalFile().toUtf8().constData());
            err = pipeline->setState(State::Paused);

            if (err == Error::Ok) {
                for (auto out : demuxer->outputs()) {
                    if (out->name().starts_with("audio")) {
                        auto err = LinkElement(demuxer, out->name(), aqueue, "sink");
                        break;
                    }
                }
                for (auto out : demuxer->outputs()) {
                    if (out->name().starts_with("video")) {
                        auto err = LinkElement(demuxer, out->name(), vqueue, "sink");
                        break;
                    }
                }
                err = pipeline->setState(State::Running);
            }
            if (err == Error::Ok) {
                progressBar->setRange(0, demuxer->duration());
                pauseBtn->setEnabled(true);
            }
        }
    });
    QObject::connect(pauseBtn, &QPushButton::clicked, [&](bool) {
        if (pipeline->state() == State::Paused) {
            pipeline->setState(State::Running);
        }
        else if (pipeline->state() == State::Running) {
            pipeline->setState(State::Paused);
        }
    });


    win.show();
    return app.exec();
}