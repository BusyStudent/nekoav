#include <thread>
#include <gtest/gtest.h>
#include "../nekoav/detail/template.hpp"
#include "../nekoav/backtrace.hpp"
#include "../nekoav/elements.hpp"
#include "../nekoav/container.hpp"
#include "../nekoav/property.hpp"
#include "../nekoav/format.hpp"
#include "../nekoav/enum.hpp"
#include "../nekoav/time.hpp"
#include "../nekoav/log.hpp"
#include "../nekoav/pad.hpp"

using namespace std::chrono_literals;
using namespace NEKO_NAMESPACE;

class TinyElement final : public Template::GetImpl<Element> {
public:
    TinyElement() {

    }
    Error onInitialize() override {
        Backtrace();
        return Error::Ok;
    }
    Error onTeardown() override {
        Backtrace();
        return Error::Ok;
    }
};
class TinyThreadElement final : public Template::GetThreadImpl<Element> {
public:
    TinyThreadElement() {
        auto src = addInput("sink");
        src->setCallback([&](View<Resource> resourceView) -> Error {
            return Error::Ok;
        });
    }
    Error onInitialize() override {
        Backtrace();
        return Error::Ok;
    }
    Error onTeardown() override {
        Backtrace();
        return Error::Ok;
    }
};


class TinyResource final : public Resource {

};
class TinySource final : public Template::GetImpl<Element> {
public:
    TinySource() {
        mPad = addOutput("src");
    }
    void push() {
        mPad->push(make_shared<TinyResource>());
    }
private:
    Pad *mPad;
};
class TinySink final : public Template::GetImpl<Element> {
public:
    TinySink() {
        addInput("sink")->setCallback(std::bind(&TinySink::process, this, std::placeholders::_1));
    }
    Error process(View<Resource> resourceView) {
        // Backtrace();
        NEKO_DEBUG(typeid(*resourceView));
        return Error::Ok;
    }
};
class TinyMiddle final : public Template::GetImpl<Element> {
public:
    TinyMiddle() {
        auto out = addOutput("src");
        auto in = addInput("sink");

        in->setCallback(std::bind(&Pad::push, out, std::placeholders::_1));
    }
};

TEST(CoreTest, TimeTest) {
    int64_t offset;
    int64_t ms = GetTimeCostFor([&]() {
        offset = SleepFor(10);
    });
    NEKO_DEBUG(ms);
    NEKO_DEBUG(offset);
    NEKO_DEBUG(GetTimeCostFor(SleepFor, 10));

    NEKO_TRACE_TIME(duration) {
        SleepFor(10);
    }
}

TEST(CoreTest, PropertyTest) {
    Property prop(1);
    ASSERT_EQ(prop.toInt(), 1);
    prop = 114514.f;
    ASSERT_EQ(prop.toDouble(), 114514);
    ASSERT_EQ(prop.isDouble(), true);

    prop = {};
    ASSERT_EQ(prop.isNull(), true);

    prop = Property::newMap();
    ASSERT_EQ(prop.isMap(), true);
    prop["A"] = 1;
    prop["C"] = Property::newMap();
    prop["C"]["Number"] = 114514;
    prop["C"]["String"] = "Test String";
    prop["Vec"] = Property::newList();
    prop["Vec"].push_back(1);
    prop["Vec"].push_back("WTF");
    prop["Vec"].push_back(114514);
    prop["Vec"].push_back(Property());

    NEKO_DEBUG(prop);
    ASSERT_EQ(prop == prop.clone(), true);
    ASSERT_EQ(prop.containsKey("A"), true);

    enum class AEnum {
        VA
    };
    enum BEnum {
        VB
    };

    prop = AEnum::VA;
    ASSERT_EQ(prop.toEnum<AEnum>(), AEnum::VA);
    prop = BEnum::VB;
    ASSERT_EQ(prop.toEnum<BEnum>(), BEnum::VB);
}

TEST(CoreTest, StateEnum) {
    NEKO_DEBUG(ComputeStateChanges(State::Null, State::Running));
    NEKO_DEBUG(ComputeStateChanges(State::Running, State::Null));
    
    ASSERT_EQ(ComputeStateChanges(State::Null, State::Error).empty(), true);
    ASSERT_EQ(ComputeStateChanges(State::Null, State::Null).empty(), true);
    ASSERT_EQ(GetTargetState(StateChange::Invalid), State::Error);
}

TEST(CoreTest, Format) {
    constexpr auto sfmt = GetAltSampleFormat(SampleFormat::FLTP);
    constexpr auto sfmt1 = GetPackedSampleFormat(SampleFormat::FLT);
}

TEST(CoreTest, Elem) {
    TinyElement elem;
    NEKO_DEBUG(elem.name());

    elem.setState(State::Running);
    elem.setState(State::Null);

    TinyThreadElement thelem;
    thelem.setState(State::Running);
    thelem.setState(State::Null);

    TinySource src;
    TinyMiddle middle;
    TinyMiddle middle2;
    TinySink sink;

    LinkElements(&src, &middle, &middle2, &sink);
    src.push();
    src.push();
    src.push();

};

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}