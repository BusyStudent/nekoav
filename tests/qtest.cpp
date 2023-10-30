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
#include "../nekoav/interop/qnekoav.hpp"
#include "../nekoav/backtrace.hpp"
#include "../nekoav/message.hpp"
#include "../nekoav/format.hpp"
#include "../nekoav/media.hpp"
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
    auto canvas = new QNekoVideoWidget;
    layout->addWidget(btn);
    layout->addWidget(canvas);

    Graph graph;
    MediaPipeline pipeline;
    pipeline.setGraph(&graph);

    // Init Graph
    auto factory = GetMediaFactory();
    auto demuxer = factory->createElement<Demuxer>().release();

    auto aqueue = CreateMediaQueue().release();
    auto vqueue = CreateMediaQueue().release();

    auto adecoder = factory->createElement<Decoder>().release();
    auto aconverter = factory->createElement<AudioConverter>().release();
    auto apresenter = factory->createElement<AudioPresenter>().release();

    auto vdecoder = factory->createElement<Decoder>().release();
    auto vconverter = factory->createElement<VideoConverter>().release();
    auto vpresenter = factory->createElement<VideoPresenter>().release();

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
    vconverter->linkWith("src", vpresenter, "sink");


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
    graph.addElements(demuxer, vdecoder, vconverter, vpresenter, adecoder, aconverter, apresenter);
    graph.addElements(aqueue, vqueue);

    aqueue->setName("AudioQueue");
    vqueue->setName("VideoQueue");

    vpresenter->setRenderer(canvas);

    std::cout << graph.toDocoument() << std::endl;

    QObject::connect(btn, &QPushButton::clicked, [&](bool) {
        auto url = QFileDialog::getOpenFileUrl(nullptr, "Open File");
        if (url.isValid()) {
            demuxer->setSource(url.toLocalFile().toUtf8().constData());
            pipeline.setState(State::Stopped);
            pipeline.setState(State::Running);
        }
    });
    pipeline.setMessageCallback([&](View<Message> message) {
        if (message->type() == Message::ClockUpdated) {
            QMetaObject::invokeMethod(&win, [&]() {
                win.setWindowTitle(QString::number(pipeline.masterClock()->position()));
            }, Qt::QueuedConnection);
        }
    });

    win.show();
    return app.exec();
}