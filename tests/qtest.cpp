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

using namespace NekoAV;

int main(int argc, char **argv) {
    InstallCrashHandler();
    
    QApplication app(argc, argv);
    QMainWindow win;

    auto widget = new QWidget(&win);
    win.setCentralWidget(widget);

    auto layout = new QVBoxLayout(widget);
    auto btn = new QPushButton("OpenFile");
    auto pauseBtn = new QPushButton("Pause");
    layout->addWidget(btn);
    layout->addWidget(pauseBtn);

    pauseBtn->setEnabled(false);

    // Init Graph
    auto factory = GetElementFactory();
    auto pipeline = factory->createElement<Pipeline>();
    auto demuxer = factory->createElement<Demuxer>();

    auto aqueue = factory->createElement<MediaQueue>();
    auto vqueue = factory->createElement<MediaQueue>();

    auto adecoder = factory->createElement<Decoder>();
    auto aconverter = factory->createElement<AudioConverter>();
    auto apresenter = factory->createElement<AudioSink>();

    auto vdecoder = factory->createElement<Decoder>();
    auto vconverter = factory->createElement<VideoConverter>();
    auto vpresenter = factory->createElement<TestVideoSink>();

    aqueue->setName("AudioQueue");
    vqueue->setName("VideoQueue");

    pipeline->addElements(demuxer, aqueue, adecoder, aconverter, apresenter, vqueue, vdecoder, vconverter, vpresenter);

    LinkElements(aqueue, adecoder, aconverter, apresenter);
    LinkElements(vqueue, vdecoder, vconverter, vpresenter);

    pipeline->setEventCallback([&](View<Event> event) {
        if (event->type() == Event::ClockUpdated) {
            auto str = QString("Time: %1").arg(event.viewAs<ClockEvent>()->clock()->position());
            QMetaObject::invokeMethod(&win, [s = std::move(str), &win]() {
                win.setWindowTitle(s);
            }, Qt::QueuedConnection);
        }
    });

    QObject::connect(btn, &QPushButton::clicked, [&](bool) {
        auto url = QFileDialog::getOpenFileUrl(nullptr, "Open File");
        if (url.isValid()) {
            demuxer->setUrl(url.toLocalFile().toUtf8().constData());
            auto err = pipeline->setState(State::Null);
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