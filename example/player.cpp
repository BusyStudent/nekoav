#include <nekoav/elements/playbin.hpp>
#include <nekoav/elements/video.hpp>
#include <nekoav/element.hpp>
#include <nekoav/event.hpp>
#include <ilias/platform/qt.hpp>
#include <ilias/console.hpp>
#include <ilias/signal.hpp>
#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>
#include <QFileDialog>
#include <QKeyEvent>
#include "ui_player.h"

#if defined(_MSC_VER)
    #pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

using std::literals::operator""s;

class Player final : public QMainWindow {
public:
    Player() {
        ui.setupUi(this);

        // When the open clicked
        connect(ui.actionOpen, &QAction::triggered, [this] {
            auto file = QFileDialog::getOpenFileName(this, "Open media", "", "Media Files (*.mp4 *.mkv *.avi *.mp3 *.flac)");
            if (file.isEmpty()) {
                return;
            }
            if (mHandle) {
                mHandle.stop();
            }
            mHandle = ilias::spawn(mediaTask(file));
        });

        // When the play button clicked
        connect(ui.playButton, &QPushButton::clicked, [this] {
            if (!mPipeline) {
                QMessageBox::critical(this, "Error", "No media loaded");
                return;
            }
            if (mPipeline->state() == nekoav::State::Running) {
                mPipeline->setState(nekoav::State::Paused).wait();
                ui.playButton->setText("Play");
            }
            else {
                mPipeline->setState(nekoav::State::Running).wait();
                ui.playButton->setText("Pause");
            }
        });

        // When the slider moved
        connect(ui.progressSlider, &QSlider::sliderPressed, [this]() {
            mSliderPressing = true;
        });

        connect(ui.progressSlider, &QSlider::sliderReleased, [this]() {
            mSliderPressing = false;
            if (!mPipeline) {
                return;
            }
            int value = ui.progressSlider->value();
            std::println("Slider to {}", value);
            mPipeline->sendEvent(nekoav::Event::Seek {
                .timestamp = std::chrono::milliseconds {value}
            }).wait();
        });
    }

    ~Player() {
        if (mHandle) {
            mHandle.stop();
            mHandle.wait();
        }
    }
protected:
    auto keyPressEvent(QKeyEvent *event) -> void override {
        if (event->key() == Qt::Key_F11) {
            if (windowState() == Qt::WindowFullScreen) {
                setWindowState(Qt::WindowNoState);
            }
            else {
                setWindowState(Qt::WindowFullScreen);
            }
        }
    }
private:
    auto mediaTask(QString url) -> ilias::Task<void> {
        auto pipeline = std::make_shared<nekoav::Pipeline>();
        auto playbin = std::make_shared<nekoav::PlayBin>("PlayBin");

        // Prepare it
        pipeline->addElement(playbin);
        playbin->setUrl(url.toStdString());
        playbin->setRenderer(ui.videoWidget->renderer());

        // Then, start it
        auto cleanup = [&]() -> ilias::Task<void> {
            co_await pipeline->setState(nekoav::State::Null);
        };
        co_await ilias::finally(
            ilias::whenAll(main(pipeline), watchEvent(pipeline)),
            cleanup
        );
    }

    auto main(nekoav::Pipeline::Ptr pipeline) -> ilias::Task<void> {
        if (co_await pipeline->setState(nekoav::State::Running)) {
            mPipeline = pipeline;
            ui.playButton->setText("Pause");
            co_await ilias::sleep(1000s);
        }
    }
    
    auto watchEvent(nekoav::Pipeline::Ptr pipeline) -> ilias::Task<void> {
        while (true) {
            auto event = co_await pipeline->readEvent();
            if (event.isClockUpdate()) {
                auto clock = event.toClockUpdate();
                auto s = std::chrono::duration_cast<std::chrono::milliseconds>(clock.time);
                if (!mSliderPressing) {
                    ui.progressSlider->setValue(s.count());
                }
            }
            if (event.isMediaLoaded()) {
                auto media = event.toMediaLoaded();
                auto startTime = std::chrono::duration_cast<std::chrono::milliseconds>(media.startTime);
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(media.duration);
                ui.progressSlider->setRange(startTime.count(), duration.count());
            }
            if (event.isError()) {
                auto error = event.toError();
                ui.statusbar->showMessage(error.message.c_str());
            }
        }
    }

    Ui::MainWindow          ui;
    ilias::WaitHandle<void> mHandle;
    nekoav::Pipeline::Ptr   mPipeline;
    bool                    mSliderPressing = false;
};

auto main(int argc, char** argv) -> int {
    QApplication app(argc, argv);
    ilias::QIoContext ctxt {};
    ilias::TracingWebUi webui {"127.0.0.1:8066"};
    ctxt.install();
    webui.install();

    Player player;
    player.show();
    return app.exec();
}