#include <thread>
#include <gtest/gtest.h>
#include "../nekoav/ffmpeg/factory.hpp"
#include "../nekoav/elements.hpp"
#include "../nekoav/property.hpp"
#include "../nekoav/media.hpp"
#include "../nekoav/log.hpp"

using namespace std::chrono_literals;
using namespace NekoAV;

class MyInterface {
public:
    virtual void call() = 0;
};

class TestResource : public Resource {
public:
    virtual void call() {
        NEKO_LOG("Call from {}", this);        
    }
};

class TestElementSource : public Element, public MyInterface {
public:
    TestElementSource() {
        setThreadPolicy(ThreadPolicy::SingleThread);
    }
    void call() override {

    }
    Error run() override {
        while (state() != State::Stopped) {
            // waitTask();
            std::this_thread::sleep_for(10ms);
            pad->send(make_shared<TestResource>());
            dispatchTask();
        }
        return Error::Ok;
    }
    void stateChanged(State newState) override {
        NEKO_LOG("State changed to {}", newState);
    }
    Error init() override {
        NEKO_DEBUG("Init");
        return Error::Ok;
    }
    Error teardown() override {
        NEKO_DEBUG("Teardown");
        return Error::Ok;
    }
private:
    Pad *pad {addOutput("src")};
};
class TestElementSink : public Element {
public:
    TestElementSink() {
        addInput("sink");
    }
    Error processInput(Pad &, View<Resource> resource) override {
        NEKO_DEBUG("Process input");
        resource.viewAs<TestResource>()->call();
        return Error::Ok;
    }
    void stateChanged(State newState) override {
        NEKO_LOG("State changed to {}", newState);
    }
    Error init() override {
        NEKO_DEBUG("Init");
        return Error::Ok;
    }
    Error teardown() override {
        NEKO_DEBUG("Teardown");
        return Error::Ok;
    }
};

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

TEST(CoreTest, Test1) {
    auto source = new TestElementSource();
    auto sink = new TestElementSink();
    Graph graph;
    graph.addElement(source);
    graph.addElement(sink);
    graph.registerInterface<MyInterface>(source);

    ASSERT_EQ(graph.hasCycle(), false);

    if (auto v = source->linkWith("src", sink, "sink"); !v) {
        // Failed 
        ASSERT_EQ(v, true);
    }

    Pipeline pipeline;
    pipeline.setGraph(&graph);
    pipeline.start();

    std::this_thread::sleep_for(100ms);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}