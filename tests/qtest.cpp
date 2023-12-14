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
#include "../nekoav/player.hpp"
#include "../nekoav/log.hpp"

#ifdef _MSC_VER
    #pragma comment(linker, "/subsystem:console")
#endif

using namespace NEKO_NAMESPACE;

int main(int argc, char **argv) {
    using NEKO_NAMESPACE::Arc;

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

    Player player;
    player.setVideoRenderer(videoRenderer);
    player.setPositionCallback([&](double v) {
        QMetaObject::invokeMethod(progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, v));
        QMetaObject::invokeMethod(&win, [&win, v]() {
            win.setWindowTitle(QString::number(v));
        }, Qt::QueuedConnection);
    });
    player.setStateChangedCallback([&](State newState) {
        NEKO_DEBUG(newState);
        if (newState == State::Ready) {
            QMetaObject::invokeMethod(progressBar, [&, duration = player.duration()]() {
                progressBar->setRange(0, duration);
                progressBar->setValue(0);
            }, Qt::QueuedConnection);
            QMetaObject::invokeMethod(pauseBtn, "setEnabled", Qt::QueuedConnection, Q_ARG(bool, true));
        }
        else if (newState == State::Null) {
            QMetaObject::invokeMethod(pauseBtn, "setEnabled", Qt::QueuedConnection, Q_ARG(bool, false));
        }
    });

    QObject::connect(btn, &QPushButton::clicked, [&](bool) {
        auto url = QFileDialog::getOpenFileUrl(nullptr, "Open File");
        if (url.isValid()) {
            Error err;
            player.setUrl(url.toLocalFile().toUtf8().constData());
            player.play();
        }
    });
    QObject::connect(pauseBtn, &QPushButton::clicked, [&](bool) {
        if (player.state() == State::Running) {
            player.pause();
        }
        else if (player.state() == State::Paused) {
            player.play();
        }
    });
    QObject::connect(progressBar, &QSlider::sliderReleased, [&]() {
        player.setPosition(progressBar->value());
    });


    win.show();
    return app.exec();
}