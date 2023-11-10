#include <gtest/gtest.h>
#include "../nekoav/elements/demuxer.hpp"
#include "../nekoav/elements/decoder.hpp"
#include "../nekoav/detail/template.hpp"
#include "../nekoav/pipeline.hpp"
#include "../nekoav/factory.hpp"

using namespace NEKO_NAMESPACE;

class Fail final : public Template::GetImpl<Element> {
public:
    Error onInitialize() {
        return Error::Unknown;
    }
};
class TinySource final : public Template::GetImpl<Element> {
public:
    TinySource() {
        mPad = addOutput("src");
    }
private:
    Pad *mPad;
};
class TinySink final : public Template::GetImpl<Element> {
public:
    TinySink() {
        addInput("sink");
    }
};
class TinyMiddle final : public Template::GetImpl<Element> {
public:
    TinyMiddle() {
        auto out = addOutput("src");
        auto in = addInput("sink");
    }
};

TEST(ElemTest, TestPipeline) {
    auto factory = GetElementFactory();
    auto pipeline = factory->createElement<Pipeline>();

    auto src = make_shared<TinySource>();
    auto middle1 = make_shared<TinyMiddle>();
    auto middle2 = make_shared<TinyMiddle>();
    auto sink = make_shared<TinySink>();

    pipeline->addElement(make_shared<Fail>());
    pipeline->addElements(src, middle1, middle2, sink);


    auto lerr = LinkElements(src, middle1, middle2, sink);
    ASSERT_EQ(lerr, Error::Ok);

    auto err = pipeline->setState(State::Paused);
    ASSERT_NE(err, Error::Ok);
    ASSERT_EQ(pipeline->state(), State::Null);

    err = pipeline->setState(State::Null);
    ASSERT_EQ(err, Error::Ok);


    puts(DumpTopology(pipeline).c_str());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}