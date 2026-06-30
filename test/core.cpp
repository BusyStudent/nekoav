#include <nekoav/elements/pipeline.hpp>
#include <nekoav/elements/bin.hpp>
#include <nekoav/element.hpp>
#include <nekoav/context.hpp>
#include <nekoav/sample.hpp>
#include <nekoav/format.hpp>
#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include <ilias/testing.hpp>
#include "testing_element.hpp"
#include "../src/ffmpeg.hpp"

using namespace std::literals;
using namespace nekoav;
using namespace ilias;

TEST(Internal, Time) {
    Rational timeBase {1, 1000}; // ms
    AVRational ffTimeBase {timeBase.num, timeBase.den};

    auto ns = std::chrono::nanoseconds(1'000'000); // 1ms
    auto ts = time::toFFmpeg(ns, ffTimeBase);
    EXPECT_EQ(ts, 1);

    auto ns2 = time::fromFFmpeg(ts, ffTimeBase);
    EXPECT_EQ(ns2.count(), ns.count());
}

TEST(Core, Format) {
    EXPECT_EQ(toString(PixelFormat::YUV420P), "yuv420p"); // as same as ffmpeg
}

TEST(Core, Value) {
    auto str = Value {"HelloWorld" };
    auto list = Value::fromList({ 1, 2, 3, "String" });
    auto map = Value::fromMap({
        { "Key", 1 }
    });
    EXPECT_NE(str, Value {});
    EXPECT_TRUE(list.isList());
    EXPECT_TRUE(map.isMap());
    
    // Try access map
    EXPECT_EQ(map["Key"], 1);
    EXPECT_TRUE(map["Not a Key"].isNull());
}

ILIAS_TEST(Core, Element) {
    struct MyElement : Element {

    };
    auto element = std::make_shared<MyElement>();

    EXPECT_TRUE(co_await element->setState(State::Running));
    EXPECT_EQ(element->state(), State::Running);

    EXPECT_TRUE(co_await element->setState(State::Null));
    EXPECT_EQ(element->state(), State::Null);
    co_return;
}

ILIAS_TEST(Core, PadLink) {
    auto first = std::make_shared<FirstElement>();
    auto second = std::make_shared<SecondElement>();
    EXPECT_TRUE(linkElement(*first, "out", *second, "in"));

    // Try to start it
    EXPECT_TRUE(co_await first->setState(State::Running));
    EXPECT_TRUE(co_await second->setState(State::Running));

    // Then back to null
    EXPECT_TRUE(co_await first->setState(State::Null));
    EXPECT_TRUE(co_await second->setState(State::Null));

    first->dumpInfo();
    co_return;
}

ILIAS_TEST(Core, Bin) {
    auto bin = std::make_shared<Bin>("MyBin");
    auto first = std::make_shared<FirstElement>();
    auto second = std::make_shared<SecondElement>();
    bin->addElement(first);
    bin->addElement(second);
    EXPECT_TRUE(linkElement(*first, "out", *second, "in"));

    // Try to start it
    EXPECT_TRUE(co_await bin->setState(State::Running));

    // Then back to null
    EXPECT_TRUE(co_await bin->setState(State::Null));

    bin->dumpInfo();
    co_return;
}

ILIAS_TEST(Core, Pipeline) {
    auto pipeline = std::make_shared<Pipeline>("MyPipeline");
    auto first = std::make_shared<FirstElement>();
    auto second = std::make_shared<SecondElement>();
    pipeline->addElement(first);
    pipeline->addElement(second);
    EXPECT_TRUE(linkElement(*first, "out", *second, "in"));

    // Try to start it
    EXPECT_TRUE(co_await pipeline->setState(State::Running));

    // Then back to null
    EXPECT_TRUE(co_await pipeline->setState(State::Null));

    pipeline->dumpInfo();
    co_return;
}


auto main(int argc, char **argv) -> int {
    ::ilias::PlatformContext ctxt;
    ::testing::InitGoogleTest(&argc, argv);
    ctxt.install();
    return RUN_ALL_TESTS();
}