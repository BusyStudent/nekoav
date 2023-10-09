#include <gtest/gtest.h>
#include "../nekoav/elements.hpp"
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
        addOutput(pad);
        setThreadPolicy(ThreadPolicy::SingleThread);
    }
    void call() override {

    }
    Error run() override {
        while (state() != State::Stopped) {
            // waitTask();
            std::this_thread::sleep_for(10ms);
            pad->write(make_shared<TestResource>());
            dispatchTask();
        }
        return Error::Ok;
    }
    void stateChanged(State newState) {
        NEKO_LOG("State changed to {}", newState);
    }
private:
    NekoAV::Arc<Pad> pad {Pad::NewOutput()};
};
class TestElementSink : public Element {
public:
    TestElementSink() {
        addInput(Pad::NewInput());
    }
    Error processInput(Pad *pad, ResourceView resource) override {
        NEKO_DEBUG("Process input");
        resource.viewAs<TestResource>()->call();
        return Error::Ok;
    }
    void stateChanged(State newState) {
        NEKO_LOG("State changed to {}", newState);
    }
};

TEST(CoreTest, Test1) {
    auto source = make_shared<TestElementSource>();
    auto sink = make_shared<TestElementSink>();
    Graph graph;
    graph.addElement(source);
    graph.addElement(sink);
    graph.registerInterface<MyInterface>(source.get());

    ASSERT_EQ(graph.hasCycle(), false);

    source->outputs()[0]->connect(sink->inputs()[0]);

    Pipeline pipeline;
    pipeline.setGraph(&graph);
    pipeline.start();

    std::this_thread::sleep_for(100ms);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}