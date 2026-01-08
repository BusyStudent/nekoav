#include <nekoav/elements/queue.hpp>
#include <nekoav/elements/url_source.hpp>
#include <nekoav/elements/decoder.hpp>
#include <ilias/platform.hpp>
#include <ilias/testing.hpp>
#include "testing_element.hpp"

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
    auto bin = std::make_shared<Bin>("MyBin");
    auto source = std::make_shared<UrlSource>("MySource");
    auto videoQueue = std::make_shared<Queue>("VideoQueue");
    auto videoDecoder = std::make_shared<Decoder>("VideoDecoder");
    auto videoPrint = std::make_shared<PrintElement>("VideoPrint");

    auto audioDecoder = std::make_shared<Decoder>("AudioDecoder");
    auto audioQueue = std::make_shared<Queue>("AudioQueue");
    auto audioPrint = std::make_shared<PrintElement>("AudioPrint");

    source->setUrl("https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm");

    bin->addElement(source);
    bin->addElement(videoQueue);
    bin->addElement(videoDecoder);
    bin->addElement(videoPrint);
    
    bin->addElement(audioQueue);
    bin->addElement(audioDecoder);
    bin->addElement(audioPrint);

    // Make the source loaded
    EXPECT_TRUE(co_await bin->setState(State::Paused));
    
    // Do connect here
    if (!source->videoOutputs().empty()) {
        EXPECT_TRUE(linkElement(*source, source->videoOutputs().at(0)->name(), *videoQueue, "in"));
        EXPECT_TRUE(linkElement(*videoQueue, *videoDecoder));
        EXPECT_TRUE(linkElement(*videoDecoder,  *videoPrint));
    }
    if (!source->audioOutputs().empty()) {
        EXPECT_TRUE(linkElement(*source, source->audioOutputs().at(0)->name(), *audioQueue, "in"));
        EXPECT_TRUE(linkElement(*audioQueue, *audioDecoder));
        EXPECT_TRUE(linkElement(*audioDecoder, *audioPrint));
    }

    // Run 
    EXPECT_TRUE(co_await bin->setState(State::Running));
    bin->dumpInfo();
    co_await ilias::sleep(2s);
    
    EXPECT_TRUE(co_await bin->setState(State::Null));
    co_return;
}

auto main(int argc, char **argv) -> int {
    ::ilias::PlatformContext ctxt;
    ::testing::InitGoogleTest(&argc, argv);
    ctxt.install();
    return RUN_ALL_TESTS();
}