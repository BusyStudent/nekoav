#include <nekoav/elements/queue.hpp>
#include <nekoav/elements/url_source.hpp>
#include <nekoav/elements/decoder.hpp>
#include <nekoav/elements/video.hpp>
#include <nekoav/elements/audio.hpp>
#include <ilias/platform.hpp>
#include <ilias/testing.hpp>
#include "testing_element.hpp"

using namespace std::literals;
using namespace nekoav;

ILIAS_TEST(Core, Queue) {
    auto bin = std::make_shared<Bin>("MyBin");
    auto first = std::make_shared<FirstElement>();
    auto queue = std::make_shared<Queue>("MyQueue");
    auto second = std::make_shared<SecondElement>();

    bin->addElement(first);
    bin->addElement(queue);
    bin->addElement(second);
    EXPECT_TRUE(linkElement(*first, "out", *queue, "in"));
    EXPECT_TRUE(linkElement(*queue, "out", *second, "in"));

    // Try to start it
    EXPECT_TRUE(co_await bin->setState(State::Running));

    co_await ilias::sleep(100ms);

    // Then back to null
    EXPECT_TRUE(co_await bin->setState(State::Null));

    bin->dumpInfo();
    co_return;
}

ILIAS_TEST(Core, UrlSource) {
    auto pipeline = std::make_shared<Pipeline>("MyPipeline");
    auto source = std::make_shared<UrlSource>("MySource");
    auto videoQueue = std::make_shared<Queue>("VideoQueue");
    auto videoDecoder = std::make_shared<Decoder>("VideoDecoder");
    auto videoConverter = std::make_shared<VideoConverter>("VideoConverter");
    auto videoSink = std::make_shared<VideoSink>("videoSink");

    auto audioDecoder = std::make_shared<Decoder>("AudioDecoder");
    auto audioQueue = std::make_shared<Queue>("AudioQueue");
    auto audioPrint = std::make_shared<AudioSink>("AudioPrint");

    // Configure
    source->setUrl("https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm");

    // videoSink->setRenderer(std::make_shared<NullVideoRenderer>());

    pipeline->addElement(source);
    pipeline->addElements(videoQueue, videoDecoder, videoConverter, videoSink);
    pipeline->addElements(audioQueue, audioDecoder, audioPrint);

    // Make the source loaded
    EXPECT_TRUE(co_await pipeline->setState(State::Paused));
    
    // Do connect here
    if (!source->videoOutputs().empty()) {
        EXPECT_TRUE(linkElement(*source, source->videoOutputs().at(0)->name(), *videoQueue, "in"));
        EXPECT_TRUE(linkChain(*videoQueue, *videoDecoder, *videoConverter, *videoSink));
    }
    if (!source->audioOutputs().empty()) {
        EXPECT_TRUE(linkElement(*source, source->audioOutputs().at(0)->name(), *audioQueue, "in"));
        EXPECT_TRUE(linkChain(*audioQueue, *audioDecoder, *audioPrint));
    }

    // Run 
    EXPECT_TRUE(co_await pipeline->setState(State::Running));
    pipeline->dumpInfo();
    co_await ilias::sleep(15s);
    
    EXPECT_TRUE(co_await pipeline->setState(State::Null));
    co_return;
}

auto main(int argc, char **argv) -> int {
    ::ilias::PlatformContext ctxt;
    ::testing::InitGoogleTest(&argc, argv);
    ctxt.install();
    return RUN_ALL_TESTS();
}