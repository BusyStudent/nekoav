/**
 * @file queue.cpp
 * @brief Integration: Queue element forwards samples in order (no real media).
 *
 * Graph: ProbeSource → Queue → ProbeSink inside a Bin.
 * Pushes three synthetic packets; sink must observe PTS 1ms, 2ms, 3ms.
 */

#include <nekoav/elements/bin.hpp>
#include <nekoav/elements/queue.hpp>
#include <ilias/testing.hpp>
#include <gtest/gtest.h>
#include "support/probe_element.hpp"
#include "support/sample_factory.hpp"

using namespace std::literals;
using namespace nekoav;
using nekoav::testing::ProbeElement;
using nekoav::testing::makePacketSample;

ILIAS_TEST(QueueIntegrationTest, SendSample) {
    auto bin = std::make_shared<Bin>("bin");
    auto source = std::make_shared<ProbeElement>(ElementType::Source, "source");
    auto queue = std::make_shared<Queue>("queue");
    auto sink = std::make_shared<ProbeElement>(ElementType::Sink, "sink");

    bin->addElements(source, queue, sink);
    EXPECT_TRUE(linkChain(*source, *queue, *sink));
    EXPECT_TRUE(co_await bin->setState(State::Running));

    EXPECT_TRUE(co_await source->push(makePacketSample(1ms)));
    EXPECT_TRUE(co_await source->push(makePacketSample(2ms)));
    EXPECT_TRUE(co_await source->push(makePacketSample(3ms)));

    // Queue may hop the executor; wait with a tight timeout (offline, no IO).
    auto completed = co_await ilias::timeout(sink->waitForSamples(3), 2s);
    EXPECT_TRUE(completed);

    auto samples = sink->samples();
    EXPECT_EQ(samples.size(), 3);
    if (samples.size() == 3) {
        EXPECT_TRUE(samples[0].isPacket);
        EXPECT_TRUE(samples[1].isPacket);
        EXPECT_TRUE(samples[2].isPacket);
        EXPECT_EQ(samples[0].pts, 1ms);
        EXPECT_EQ(samples[1].pts, 2ms);
        EXPECT_EQ(samples[2].pts, 3ms);
    }

    EXPECT_TRUE(co_await bin->setState(State::Null));
    co_return;
}
