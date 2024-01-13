#include <string>

#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeyEvent>
#include <QTextEdit>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSlider>
#include <QLabel>
#include <iostream>

#include "../nekoav/interop/qnekoav.hpp"
#include "../nekoav/elements/filters.hpp"
#include "../nekoav/player.hpp"
#include "../nekoav/log.hpp"

#ifdef _MSC_VER
    #pragma comment(linker, "/subsystem:console")
#endif

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        auto bar = new QMenuBar(this);
        setMenuBar(bar);
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() != Qt::Key_F11) {
            return;
        }
        if (isFullScreen()) {
            showNormal();
        }
        else {
            showFullScreen();
        }
    }
};

int main(int argc, char **argv) {
    fprintf(stderr, "std::vector<PixelFormat> sizeof(%d)\n", int(sizeof(std::vector<NekoAV::PixelFormat>)));
    fprintf(stderr, "std::string sizeof(%d)\n", int(sizeof(std::string)));
    fprintf(stderr, "Filter: sizeof(%d)\n", int(sizeof(NekoAV::Filter)));

    QApplication app(argc, argv);
    MainWindow win;

    auto widget = new QWidget(&win);
    win.setCentralWidget(widget);

    auto layout = new QVBoxLayout(widget);
    auto sublay = new QHBoxLayout();
    auto btn = new QPushButton("OpenFile");
    auto openUrlBtn = new QPushButton("OpenURL");
    auto pauseBtn = new QPushButton("Pause");
    auto progressBar = new QSlider(Qt::Horizontal);
    auto videoWidget = new QNekoAV::VideoWidget;


    layout->addLayout(sublay);
    sublay->addWidget(btn);
    sublay->addWidget(openUrlBtn);
    sublay->addWidget(pauseBtn);
    sublay->addWidget(progressBar);
    layout->addWidget(videoWidget);

    layout->setContentsMargins(0, 0, 0, 0);
    sublay->setContentsMargins(0, 0, 0, 0);

    pauseBtn->setEnabled(false);

    QNekoMediaPlayer player;
    player.setVideoOutput(videoWidget);

    auto nekoPlayer = static_cast<NekoAV::Player*>(player.nekoPlayer());

    auto bar = win.menuBar();
    auto filter = bar->addMenu("Filter");

#if 1
    auto edgeDetect = filter->addAction("EdgeDetect");
    edgeDetect->setCheckable(true);
    QObject::connect(edgeDetect, &QAction::triggered, [&, mark = (void*) nullptr]() mutable {
        if (edgeDetect->isChecked())  {
            NekoAV::Filter filter(typeid(NekoAV::KernelFilter), [](auto &element) {
                static_cast<NekoAV::KernelFilter&>(element).setEdgeDetectKernel();
            });
            mark = nekoPlayer->addFilter(filter);
        }
        else {
            nekoPlayer->removeFilter(mark);
            mark = nullptr;
        }
    });

    auto sharpen = filter->addAction("Sharpen");
    sharpen->setCheckable(true);
    QObject::connect(sharpen, &QAction::triggered, [&, mark = (void*) nullptr]() mutable {
        if (sharpen->isChecked())  {
            NekoAV::Filter filter(typeid(NekoAV::KernelFilter), [](auto &element) {
                static_cast<NekoAV::KernelFilter&>(element).setSharpenKernel();
            });
            mark = nekoPlayer->addFilter(filter);
        }
        else {
            nekoPlayer->removeFilter(mark);
            mark = nullptr;
        }
    });

    auto blur = filter->addAction("Blur");
    blur->setCheckable(true);
    QObject::connect(blur, &QAction::triggered, [&, mark = (void*) nullptr]() mutable {
        if (blur->isChecked())  {
            NekoAV::Filter filter(typeid(NekoAV::KernelFilter), [](auto &element) {
                const double kernel [3][3] = {
                    {1.0 / 9, 1.0 / 9, 1.0 / 9},
                    {1.0 / 9, 1.0 / 9, 1.0 / 9},
                    {1.0 / 9, 1.0 / 9, 1.0 / 9}
                };
                static_cast<NekoAV::KernelFilter&>(element).setKernel(kernel);
            });
            mark = nekoPlayer->addFilter(filter);
        }
        else {
            nekoPlayer->removeFilter(mark);
            mark = nullptr;
        }
    });
#endif

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
            pauseBtn->setEnabled(player.isSeekable());
            QMetaObject::invokeMethod(pauseBtn, "setEnabled", Q_ARG(bool, true));
        }
        else if (newState == QNekoMediaPlayer::StoppedState) {
            pauseBtn->setEnabled(false);
        }
    });

    QObject::connect(btn, &QPushButton::clicked, [&](bool) {
        auto url = QFileDialog::getOpenFileUrl(nullptr, "Open File");
        if (url.isValid()) {
            player.setSource(url);
            player.play();
        }
    });
    QObject::connect(openUrlBtn, &QPushButton::clicked, [&](bool) {
        auto url = QInputDialog::getText(nullptr, "Open URL", "URL Here");
        if (!url.isNull()) {
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
    // int position = 0;
    // QObject::connect(progressBar, &QSlider::sliderMoved, [&](int newPosition) {
    //     position = newPosition;
    // });
    // QObject::connect(progressBar, &QSlider::sliderReleased, [&]() {
    //     player.setPosition(position);
    // });
    QObject::connect(progressBar, &QSlider::sliderMoved, [&](int newPosition) {
        player.setPosition(newPosition);
    });


    win.show();
    return app.exec();
}