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
    auto sublay = new QHBoxLayout();
    auto btn = new QPushButton("OpenFile");
    auto pauseBtn = new QPushButton("Pause");
    auto progressBar = new QSlider(Qt::Horizontal);
    auto videoWidget = new QNekoAV::VideoWidget;


    layout->addLayout(sublay);
    sublay->addWidget(btn);
    sublay->addWidget(pauseBtn);
    sublay->addWidget(progressBar);
    layout->addWidget(videoWidget);

    pauseBtn->setEnabled(false);

    QNekoMediaPlayer player;
    player.setVideoOutput(videoWidget);

    QObject::connect( &player, &QNekoMediaPlayer::positionChanged, [&](double v) {
        if (!progressBar->isSliderDown()) {
            progressBar->setValue(v);
        }
        win.setWindowTitle(QString::number(v));
    });
    QObject::connect(&player, &QNekoMediaPlayer::playbackStateChanged, [&](QNekoMediaPlayer::PlaybackState newState) {
        NEKO_DEBUG(newState);
        if (newState == QNekoMediaPlayer::PlayingState) {
            progressBar->setRange(0, player.duration());
            pauseBtn->setEnabled(true);
            QMetaObject::invokeMethod(pauseBtn, "setEnabled", Q_ARG(bool, true));
        }
        else if (newState == QNekoMediaPlayer::StoppedState) {
            pauseBtn->setEnabled(false);
        }
    });

    QObject::connect(btn, &QPushButton::clicked, [&](bool) {
        auto url = QFileDialog::getOpenFileUrl(nullptr, "Open File");
        if (url.isValid()) {
            Error err;
            player.setSource(url);
            player.play();
        }
    });
    QObject::connect(pauseBtn, &QPushButton::clicked, [&](bool) {
        if (player.playbackState() == QNekoMediaPlayer::PlayingState) {
            player.pause();
        }
        else if (player.playbackState() == QNekoMediaPlayer::PausedState) {
            player.play();
        }
    });
    int position = 0;
    QObject::connect(progressBar, &QSlider::sliderMoved, [&](int newPosition) {
        position = newPosition;
    });
    QObject::connect(progressBar, &QSlider::sliderReleased, [&]() {
        player.setPosition(position);
    });


    win.show();
    return app.exec();
}