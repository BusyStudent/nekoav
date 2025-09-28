#include <nekoav/element.hpp>
#include <nekoav/sample.hpp>
#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include <ilias/testing.hpp>
#include "../src/internal.hpp"

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

// Test the data flow between two elements
struct TestData : public Sample {
    TestData(int v) : value(v) {}
    int value;
};

struct FirstElement : Element {
    FirstElement() {

    }

    auto onRun() -> IoTask<void> override {
        mHandle = spawn(worker());
        co_return {};
    }

    auto onPause() -> IoTask<void> override {
        assert(mHandle); // It should be set
        co_await std::exchange(mHandle, nullptr);
        co_return {};
    }

    auto worker() -> Task<void> {
        for (int i = 0; i < 10; ++i) {
            auto sample = std::make_shared<TestData>(i);
            EXPECT_TRUE(co_await mOut.push(std::move(sample)));
        }
    }

    Pad             &mOut = createOutputPad("out");
    WaitHandle<void> mHandle;
};

struct SecondElement : Element {
    SecondElement() {
        auto &in = createInputPad("in");
        in.setPushCallback<&SecondElement::onPush>(this);
    }

    auto onPush(Sample::Ptr sample) -> IoTask<void> {
        auto ptr = std::dynamic_pointer_cast<TestData>(sample);
        EXPECT_TRUE(ptr);
        EXPECT_TRUE(ptr->value >= 0 && ptr->value < 10);
        std::cout << "Data arrive " << ptr->value << std::endl;
        co_return {};
    }
};

ILIAS_TEST(Core, PadLink) {
    auto first = std::make_shared<FirstElement>();
    auto second = std::make_shared<SecondElement>();

    auto &outPad = first->outputs().front();
    auto &inPad = second->inputs().front();
    inPad.link(outPad);

    // Try to start it
    EXPECT_TRUE(co_await first->setState(State::Running));
    EXPECT_TRUE(co_await second->setState(State::Running));

    // Then back to null
    EXPECT_TRUE(co_await first->setState(State::Null));
    EXPECT_TRUE(co_await second->setState(State::Null));

    co_return;
}

auto main(int argc, char **argv) -> int {
    ::ilias::PlatformContext ctxt;
    ::testing::InitGoogleTest(&argc, argv);
    ctxt.install();
    return RUN_ALL_TESTS();
}