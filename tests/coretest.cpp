#include <thread>
#include <gtest/gtest.h>
#include "../nekoav/detail/template.hpp"
#include "../nekoav/backtrace.hpp"
#include "../nekoav/elements.hpp"
#include "../nekoav/property.hpp"
#include "../nekoav/format.hpp"
#include "../nekoav/enum.hpp"
#include "../nekoav/time.hpp"
#include "../nekoav/log.hpp"

using namespace std::chrono_literals;
using namespace NekoAV;

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

// class TestElementSource : public Element, public MyInterface {
// public:
//     TestElementSource() {
//         setThreadPolicy(ThreadPolicy::SingleThread);
//     }
//     void call() override {

//     }
//     Error run() override {
//         while (state() != State::Stopped) {
//             // waitTask();
//             std::this_thread::sleep_for(10ms);
//             pad->send(make_shared<TestResource>());
//             dispatchTask();
//         }
//         return Error::Ok;
//     }
//     void stateChanged(State newState) override {
//         NEKO_LOG("State changed to {}", newState);
//     }
//     Error init() override {
//         NEKO_DEBUG("Init");
//         return Error::Ok;
//     }
//     Error teardown() override {
//         NEKO_DEBUG("Teardown");
//         return Error::Ok;
//     }
// private:
//     Pad *pad {addOutput("src")};
// };
// class TestElementSink : public Element {
// public:
//     TestElementSink() {
//         addInput("sink");
//     }
//     Error processInput(Pad &, View<Resource> resource) override {
//         NEKO_DEBUG("Process input");
//         resource.viewAs<TestResource>()->call();
//         return Error::Ok;
//     }
//     void stateChanged(State newState) override {
//         NEKO_LOG("State changed to {}", newState);
//     }
//     Error init() override {
//         NEKO_DEBUG("Init");
//         return Error::Ok;
//     }
//     Error teardown() override {
//         NEKO_DEBUG("Teardown");
//         return Error::Ok;
//     }
// };

TEST(CoreTest, TimeTest) {
    int64_t offset;
    int64_t ms = GetTimeCostFor([&]() {
        offset = SleepFor(10);
    });
    NEKO_DEBUG(ms);
    NEKO_DEBUG(offset);
    NEKO_DEBUG(GetTimeCostFor(SleepFor, 10));

    NEKO_TRACE_TIME {
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

};

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}