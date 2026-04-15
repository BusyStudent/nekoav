// Element used to testing
#pragma once
#include <nekoav/element.hpp>
#include <ilias/task.hpp>

using namespace nekoav;
using namespace std::literals;

struct FirstElement : Element {
    FirstElement() {
        mOut.mutableCaps().insert("shit/test_data", Value {114514});
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
        // Try qeury
        EXPECT_EQ(mOut.sendQuery(Query::Duration {}), Reply::Duration {10ms});

        // Try send data
        for (int i = 0; i < 10; ++i) {
            EXPECT_TRUE(co_await mOut.push(nullptr));
        }

        // Try send event
        EXPECT_TRUE(co_await mOut.pushEvent(EndOfStreamEvent {
            .streamIndex = 0
        }));
    }

    Pad                    &mOut = createOutputPad("out");
    ilias::WaitHandle<void> mHandle;
};

struct SecondElement : Element {
    SecondElement() {
        auto &in = createInputPad("in");
        in.setPushCallback<&SecondElement::onPush>(this);
        in.setEventCallback<&SecondElement::onEvent>(this);
        in.setQueryCallback<&SecondElement::onQuery>(this);
    }

    auto onPush(Pad &, Sample sample) -> IoTask<void> {
        EXPECT_TRUE(sample.isNull()); // Is nullptr
        co_return {};
    }


    auto onEvent(Pad &, Event &event) -> IoTask<void> {
        std::cout << "Event arrive: " << &event << std::endl;
        co_return {};
    }

    auto onQuery(Pad &, Query &query) -> std::optional<Reply> {
        EXPECT_EQ(query, Query::Duration {});
        std::cout << "Query arrive: " << &query << std::endl;
        return Reply::Duration {10ms};
    }
};

struct PrintElement : Element {
    PrintElement(std::string_view name = {}) : Element(name) {
        auto &in = createInputPad("in");
        in.setPushCallback<&PrintElement::onPush>(this);
    }

    auto onPush(Pad &, Sample sample) -> IoTask<void> {
        if (!sample) {
            std::cout << name() << " EOF arrive" << std::endl;
            co_return {};
        }
        std::cout << name() << " Data arrive pts: " << sample.pts().value_or(0ms) << " dts: " << sample.dts().value_or(0ms) << std::endl;
        co_return {};
    }
};