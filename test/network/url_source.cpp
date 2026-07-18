/**
 * @file url_source.cpp
 * @brief Optional network smoke: UrlSource against a public WebM trailer.
 *
 * Not registered unless `xmake f --network_tests=y`. Uses GStreamer project
 * media (sintel trailer). Links the first video pad to a ProbeSink and waits
 * up to 60s for EOS — flaky under poor connectivity by design (smoke only).
 */

#include <nekoav/elements/pipeline.hpp>
#include <nekoav/elements/url_source.hpp>
#include <ilias/testing.hpp>
#include <gtest/gtest.h>
#include "support/probe_element.hpp"

using namespace std::literals;
using namespace nekoav::testing;
using namespace nekoav;

ILIAS_TEST(UrlSource, ReadFromNetwork) {
    auto pipeline = std::make_shared<Pipeline>("pipeline");
    auto source = std::make_shared<UrlSource>("source");
    auto sink = std::make_shared<ProbeElement>(ElementType::Sink, "sink");

    source->setUrl("https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm");
    pipeline->addElements(source, sink);

    auto prepared = co_await pipeline->setState(State::Paused);
    EXPECT_TRUE(prepared);
    if (!prepared) {
        auto cleanup = co_await pipeline->setState(State::Null);
        EXPECT_TRUE(cleanup);
        co_return;
    }

    auto outputs = source->videoOutputs();
    EXPECT_FALSE(outputs.empty());
    if (outputs.empty()) {
        auto cleanup = co_await pipeline->setState(State::Null);
        EXPECT_TRUE(cleanup);
        co_return;
    }

    // Raw packets to sink (no decoder) — only checks demux + network read path.
    EXPECT_TRUE(linkElement(*source, outputs.front()->name(), *sink, "in"));
    auto running = co_await pipeline->setState(State::Running);
    EXPECT_TRUE(running);
    if (running) {
        auto eos = co_await ilias::timeout(sink->waitForEndOfStream(), 60s);
        EXPECT_TRUE(eos);
    }

    EXPECT_TRUE(co_await pipeline->setState(State::Null));
    co_return;
}
