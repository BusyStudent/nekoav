#include <nekoav/elements/url_source.hpp>
#include <nekoav/elements/playbin.hpp>
#include <nekoav/elements/decoder.hpp>
#include <nekoav/elements/queue.hpp>
#include <nekoav/elements/audio.hpp>
#include <nekoav/elements/video.hpp>
#include <ilias/task.hpp> // finally
#include "internal.hpp"

namespace nekoav {
    
struct PlayBin::Impl {
    std::shared_ptr<UrlSource> source;  
    std::shared_ptr<VideoSink> videoSink;
    std::shared_ptr<AudioSink> audioSink;
};

PlayBin::PlayBin(std::string_view name) : Bin(name) {

}

PlayBin::~PlayBin() {
    assert(!d);
}

auto PlayBin::onPrepare() -> IoTask<void> {
    auto impl = std::make_unique<Impl>();
    auto handler = [&]() -> Task<void> { // Cleanup handler
        if (!impl) {
            co_return;
        }
        // Clear all the children
        auto _ = co_await this->clear();
    };

    // Start the source and then connect the sink by detected media type
    auto main = [&]() -> IoTask<void> {
        impl->source = std::make_shared<UrlSource>("PlayBin::UrlSource");
        impl->source->setUrl(mUrl);
        if (auto res = co_await impl->source->setState(State::Paused); !res) {
            logger::error("[PlayBin] '{}' Failed to initialize the urlSource", name());
            co_return res;
        }
        addElement(impl->source);

        // Create the video sink if needed
        if (!impl->source->videoOutputs().empty()) {
            auto queue = std::make_shared<Queue>("PlayBin::VideoQueue");
            auto decoder = std::make_shared<Decoder>("PlayBin::VideoDecoder");
            auto converter = std::make_shared<VideoConverter>("PlayBin::VideoConverter");
            auto sink = std::make_shared<VideoSink>("PlayBin::VideoSink");
            addElements(queue, decoder, converter, sink);
            linkChain(*queue, *decoder, *converter, *sink);

            // Link the video output to it
            if (!linkElement(*impl->source, impl->source->videoOutputs().front()->name(), *queue, "in")) {
                // ???
                assert(false);
            }
            impl->videoSink = sink;
        }
        if (!impl->source->audioOutputs().empty()) {
            auto queue = std::make_shared<Queue>("PlayBin::AudioQueue");
            auto decoder = std::make_shared<Decoder>("PlayBin::AudioDecoder");
            // auto converter = std::make_shared<AudioConverter>("PlayBin::AudioConverter");
            auto sink = std::make_shared<AudioSink>("PlayBin::AudioSink");
            addElements(queue, decoder, sink);
            linkChain(*queue, *decoder, *sink);

            // Link the audio output to it
            if (!linkElement(*impl->source, impl->source->audioOutputs().front()->name(), *queue, "in")) {
                // ???
                assert(false);
            }
            impl->audioSink = sink;
        }
        if (!impl->audioSink && !impl->videoSink) {
            logger::error("[PlayBin] '{}' No audio or video stream found", name());
            co_return Err(Error::NoStream);
        }
        // Done
        d.swap(impl);
        co_return {};
    };
    
    if (auto res = co_await (main() | ilias::finally(handler)); !res) {
        co_return res;
    }
    // Sync the state here (need check)
    if (auto res = co_await Bin::onPrepare(); !res) {
        co_return res;
    }
    assert(d->source->state() == State::Paused);
    co_return {};
}

auto PlayBin::onStop() -> IoTask<void> {
    auto res = co_await Bin::onStop();
    auto _ = co_await clear();
    d.reset();
    co_return res;
}

auto PlayBin::setUrl(std::string_view url) -> void {
    mUrl = url;
}

} // namespace nekoav