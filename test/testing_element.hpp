// Element used to testing
#pragma once
#include <nekoav/element.hpp>
#include <ilias/task.hpp>

using namespace nekoav;
using namespace std::literals;

// Test the data flow between two elements
struct TestData : public Sample {
    TestData(int v) : value(v) {}
    int value;
};

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
            auto sample = std::make_shared<TestData>(i);
            EXPECT_TRUE(co_await mOut.push(std::move(sample)));
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

    auto onPush(Pad &, Sample::Ptr sample) -> IoTask<void> {
        auto ptr = std::dynamic_pointer_cast<TestData>(sample);
        EXPECT_TRUE(ptr);
        EXPECT_TRUE(ptr->value >= 0 && ptr->value < 10);
        std::cout << "Data arrive " << ptr->value << std::endl;
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
    PrintElement() {
        auto &in = createInputPad("in");
        in.mutableCaps() = Caps::makeAny();
        in.setPushCallback<&PrintElement::onPush>(this);
    }

    auto onPush(Pad &, Sample::Ptr sample) -> IoTask<void> {
        if (!sample) {
            std::cout << "EOF arrive" << std::endl;
            co_return {};
        }
        std::cout << "Data arrive pts: " << sample->pts() << " dts: " << sample->dts() << std::endl;
        co_return {};
    }
};