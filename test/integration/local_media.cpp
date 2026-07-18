/**
 * @file local_media.cpp
 * @brief Integration: demux + decode repository fixture av_1s.mkv offline.
 *
 * Graph (manual pad link after Paused discovers streams):
 *
 *   UrlSource ─┬─ videoOutputs[0] → Decoder(SW) → ProbeSink
 *              └─ audioOutputs[0] → Decoder(SW) → ProbeSink
 *
 * Asserts: one video + one audio stream, both reach EOS, and at least one
 * real video frame and one audio frame were observed.
 *
 * Requires FFmpeg and test/fixtures/av_1s.mkv. No network / audio device / GUI.
 */

#include <nekoav/elements/decoder.hpp>
#include <nekoav/elements/pipeline.hpp>
#include <nekoav/elements/url_source.hpp>
#include <ilias/testing.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include "support/probe_element.hpp"

using namespace std::literals;
using namespace nekoav::testing;
using namespace nekoav;

ILIAS_TEST(LocalMedia, DiscoversAndDecodes) {
    auto fixture = std::filesystem::absolute("test/fixtures/av_1s.mkv");
    EXPECT_TRUE(std::filesystem::is_regular_file(fixture));

    auto pipeline = std::make_shared<Pipeline>("pipeline");
    auto source = std::make_shared<UrlSource>("source");
    auto videoDecoder = std::make_shared<Decoder>("video-decoder");
    auto videoSink = std::make_shared<ProbeElement>(ElementType::Sink, "video-sink");
    auto audioDecoder = std::make_shared<Decoder>("audio-decoder");
    auto audioSink = std::make_shared<ProbeElement>(ElementType::Sink, "audio-sink");

    // Software-only keeps CI deterministic (no GPU / vendor decoder quirks).
    videoDecoder->setPolicy(Decoder::SoftwareOnly);
    audioDecoder->setPolicy(Decoder::SoftwareOnly);
    source->setUrl(fixture.string());
    pipeline->addElements(source, videoDecoder, videoSink, audioDecoder, audioSink);

    // Paused opens the container and creates dynamic stream pads; do not link yet.
    auto prepared = co_await pipeline->setState(State::Paused);
    EXPECT_TRUE(prepared);
    if (!prepared) {
        auto cleanup = co_await pipeline->setState(State::Null);
        EXPECT_TRUE(cleanup);
        co_return;
    }

    auto videoOutputs = source->videoOutputs();
    auto audioOutputs = source->audioOutputs();
    EXPECT_EQ(videoOutputs.size(), 1);
    EXPECT_EQ(audioOutputs.size(), 1);
    if (videoOutputs.empty() || audioOutputs.empty()) {
        auto cleanup = co_await pipeline->setState(State::Null);
        EXPECT_TRUE(cleanup);
        co_return;
    }

    EXPECT_TRUE(linkElement(*source, videoOutputs.front()->name(), *videoDecoder, "in"));
    EXPECT_TRUE(linkElement(*videoDecoder, *videoSink));
    EXPECT_TRUE(linkElement(*source, audioOutputs.front()->name(), *audioDecoder, "in"));
    EXPECT_TRUE(linkElement(*audioDecoder, *audioSink));

    auto running = co_await pipeline->setState(State::Running);
    EXPECT_TRUE(running);
    if (running) {
        // 1s fixture; 5s budget absorbs demux/decode startup on slow CI hosts.
        auto videoEos = co_await ilias::timeout(videoSink->waitForEndOfStream(), 5s);
        auto audioEos = co_await ilias::timeout(audioSink->waitForEndOfStream(), 5s);
        EXPECT_TRUE(videoEos);
        EXPECT_TRUE(audioEos);
    }

    auto videoSamples = videoSink->samples();
    auto audioSamples = audioSink->samples();
    EXPECT_TRUE(std::ranges::any_of(videoSamples, &SampleObservation::isVideoFrame));
    EXPECT_TRUE(std::ranges::any_of(audioSamples, &SampleObservation::isAudioFrame));

    EXPECT_TRUE(co_await pipeline->setState(State::Null));
    co_return;
}
