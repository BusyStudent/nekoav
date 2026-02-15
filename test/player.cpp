#include <nekoav/elements/playbin.hpp>
#include <nekoav/elements/video.hpp>
#include <nekoav/element.hpp>
#include <ilias/platform/qt.hpp>
#include <ilias/signal.hpp>
#include <QApplication>
#include <QMainWindow>
#include <QFileDialog>
#include "ui_player.h"

#if defined(_MSC_VER)
    #pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

using std::literals::operator""s;

class Player final : public QMainWindow, public nekoav::VideoRenderer, public std::enable_shared_from_this<Player> {
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
    }

    ~Player() {
        if (mHandle) {
            mHandle.stop();
            mHandle.wait();
        }
    }

    // VideoRenderer
    auto init() -> ilias::IoTask<void> override {
        co_return {};
    }

    auto shutdown() -> ilias::IoTask<void> override {
        co_return {};
    }

    auto render(nekoav::Frame frame) -> ilias::IoTask<void> override {
        // auto image = QImage::fromData(frame.data(0), );
        auto image = QImage {
            reinterpret_cast<const uchar *>(frame.data(0)),
            frame.width(),
            frame.height(),
            frame.linesize(0),
            QImage::Format_RGBA8888
        };
        auto pixmap = QPixmap::fromImage(image);
        ui.videoLabel->setPixmap(pixmap);
        co_return {};
    }

    auto pixelFormats() const -> std::vector<nekoav::PixelFormat> override {
        return { nekoav::PixelFormat::RGBA };
    }
private:
    auto mediaTask(QString url) -> ilias::Task<void> {
        auto pipeline = std::make_shared<nekoav::Pipeline>();
        auto playbin = std::make_shared<nekoav::PlayBin>("PlayBin");

        // Prepare it
        pipeline->addElement(playbin);
        playbin->setUrl(url.toStdString());
        playbin->setRenderer(shared_from_this());

        // Then, start it
        auto main = [&]() -> ilias::Task<void> {
            if (co_await pipeline->setState(nekoav::State::Running)) {
                co_await ilias::sleep(1000s);
            }
        };
        auto cleanup = [&]() -> ilias::Task<void> {
            co_await pipeline->setState(nekoav::State::Null);            
        };
        co_await (main() | ilias::finally(cleanup));
    }

    Ui::MainWindow          ui;
    ilias::WaitHandle<void> mHandle;
};

auto main(int argc, char** argv) -> int {
    QApplication app(argc, argv);
    ilias::QIoContext ctxt {};
    ctxt.install();

    auto player = std::make_shared<Player>();
    player->show();
    return app.exec();
}