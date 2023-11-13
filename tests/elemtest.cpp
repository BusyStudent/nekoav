#include <gtest/gtest.h>
#include "../nekoav/elements/wavsrc.hpp"
#include "../nekoav/elements/demuxer.hpp"
#include "../nekoav/elements/decoder.hpp"
#include "../nekoav/elements/audiosink.hpp"
#include "../nekoav/elements/audiocvt.hpp"
#include "../nekoav/elements/videocvt.hpp"
#include "../nekoav/elements/videosink.hpp"
#include "../nekoav/elements/mediaqueue.hpp"
#include "../nekoav/detail/template.hpp"
#include "../nekoav/pipeline.hpp"
#include "../nekoav/factory.hpp"
#include "../nekoav/libc.hpp"
#include "../nekoav/pad.hpp"

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
        addInput("sink")->setCallback([](View<Resource> resource) {
            puts(libc::typenameof(typeid(*resource)));
            return Error::Ok;
        });
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

TEST(ElemTest, TestDemuxer) {
    auto factory = GetElementFactory();
    auto pipeline = factory->createElement<Pipeline>();
    auto demuxer = factory->createElement<Demuxer>();
    auto audiodecoder = factory->createElement<Decoder>();
    auto audiocvt = factory->createElement<AudioConverter>();
    // auto sink = make_shared<TinySink>();
    auto audioqueue = factory->createElement<MediaQueue>();

    auto sink =  factory->createElement<AudioSink>();

    auto videodecoder = factory->createElement<Decoder>();
    auto videocvt = factory->createElement<VideoConverter>();
    auto videosink = factory->createElement<TestVideoSink>();
    auto videoqueue = factory->createElement<MediaQueue>();
    videosink->setTitle("NekoAV TestVideoSink Window");

    pipeline->addElements(demuxer, audioqueue, audiodecoder, audiocvt, sink);
    pipeline->addElements(videoqueue, videodecoder, videocvt, videosink);

    // demuxer->setUrl(video);
    auto err = pipeline->setState(State::Ready);
    ASSERT_EQ(err, Error::Ok);

    for (auto out : demuxer->outputs()) {
        if (out->name().starts_with("audio")) {
            auto err = LinkElement(demuxer, out->name(), audioqueue, "sink");
            ASSERT_EQ(err, Error::Ok);
            break;
        }
    }
    for (auto out : demuxer->outputs()) {
        if (out->name().starts_with("video")) {
            auto err = LinkElement(demuxer, out->name(), videoqueue, "sink");
            ASSERT_EQ(err, Error::Ok);
            break;
        }
    }


    LinkElements(audioqueue, audiodecoder, audiocvt, sink);
    LinkElements(videoqueue, videodecoder, videocvt, videosink);
    puts(DumpTopology(pipeline).c_str());

    pipeline->setState(State::Running);
    SleepFor(100000);
}
TEST(ElemTest, TestWAVSource) {
    auto wavsrc = CreateElement<WavSource>();
    wavsrc->setState(State::Running);
    wavsrc->setState(State::Null);
}
// TEST(ElemTest, VideoSinkTest) {
//     auto sink = CreateElement<TestVideoSink>();
//     if (!sink) {
//         return;
//     }
//     sink->setState(State::Running);
//     SleepFor(1000000000);
//     sink->setState(State::Null);
// }


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}