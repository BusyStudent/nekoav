#include <nekoav/elements/queue.hpp>
#include <nekoav/elements/url_source.hpp>
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
    auto queue = std::make_shared<Queue>("MyQueue");
    auto print = std::make_shared<PrintElement>();
    source->setUrl("https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm");

    bin->addElement(source);
    bin->addElement(queue);
    bin->addElement(print);

    // Make the source loaded
    EXPECT_TRUE(co_await bin->setState(State::Paused));
    
    // Do connect here
    EXPECT_TRUE(linkElement(*source, source->videoOutputs().at(0)->name(), *queue, "in"));
    EXPECT_TRUE(linkElement(*queue, "out", *print, "in"));

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