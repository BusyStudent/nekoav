// Element used to testing
#pragma once
#include <nekoav/element.hpp>
#include <ilias/task.hpp>

using namespace nekoav;

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
        for (int i = 0; i < 10; ++i) {
            auto sample = std::make_shared<TestData>(i);
            EXPECT_TRUE(co_await mOut.push(std::move(sample)));
        }
    }

    Pad                    &mOut = createOutputPad("out");
    ilias::WaitHandle<void> mHandle;
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

struct PrintElement : Element {
    PrintElement() {
        auto &in = createInputPad("in");
        in.mutableCaps() = Caps::makeAny();
        in.setPushCallback<&PrintElement::onPush>(this);
    }

    auto onPush(Sample::Ptr sample) -> IoTask<void> {
        if (!sample) {
            std::cout << "EOF arrive" << std::endl;
            co_return {};
        }
        std::cout << "Data arrive pts: " << sample->pts() << " dts: " << sample->dts() << std::endl;
        co_return {};
    }
};