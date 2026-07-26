/**
 * @file runtime.cpp
 * @brief Unit tests for Element / Pad / Bin / Pipeline runtime behaviour.
 *
 * All graphs use ProbeElement only (no FFmpeg, no media files). Covers:
 * - single-element lifecycle order
 * - injected state-change failures
 * - pad push / event / query delivery and error propagation
 * - Bin topology-ordered state changes
 * - Pipeline context + clock injection while Running
 */

#include <nekoav/elements/bin.hpp>
#include <nekoav/elements/pipeline.hpp>
#include <ilias/testing.hpp>
#include <gtest/gtest.h>
#include "support/probe_element.hpp"

using namespace std::literals;
using namespace nekoav;
using namespace nekoav::testing;

// MARK: Element state

ILIAS_TEST(ElementStateTest, CallsEachLifecycleHookInOrder) {
    // Null → Running walks Initialize → Prepare → Run;
    // Running → Null walks Pause → Stop → Teardown.
    auto trace = std::make_shared<StateTrace>();
    auto element = std::make_shared<ProbeElement>(ElementType::Other, "element", trace);

    EXPECT_TRUE(co_await element->setState(State::Running));
    EXPECT_EQ(element->state(), State::Running);
    EXPECT_TRUE(co_await element->setState(State::Null));
    EXPECT_EQ(element->state(), State::Null);

    EXPECT_EQ(trace->snapshot(), (std::vector<StateTraceEntry> {
        {"element", StateChange::Initialize},
        {"element", StateChange::Prepare},
        {"element", StateChange::Run},
        {"element", StateChange::Pause},
        {"element", StateChange::Stop},
        {"element", StateChange::Teardown},
    }));
    co_return;
}

ILIAS_TEST(ElementStateTest, RollbackOnFailure) {
    // If state change fails, it should rollback to the original state.
    auto element = std::make_shared<ProbeElement>(ElementType::Other, "element");
    element->failStateChange(StateChange::Prepare, Error::Internal);

    auto origin = element->state();
    auto result = co_await element->setState(State::Running);
    EXPECT_EQ(origin, State::Null);
    EXPECT_EQ(element->state(), origin);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error(), Error::Internal);
    EXPECT_TRUE(element->stateFailureTriggered());
    EXPECT_EQ(element->state(), State::Null);

    EXPECT_TRUE(co_await element->setState(State::Null));
    EXPECT_EQ(element->state(), State::Null);
    co_return;
}

// MARK: Pad

ILIAS_TEST(PadTest, DeliversPushEventAndQueryToPeer) {
    // After linkElement, source out → sink in carries sample, event, and query.
    auto source = std::make_shared<ProbeElement>(ElementType::Source, "source");
    auto sink = std::make_shared<ProbeElement>(ElementType::Sink, "sink");
    sink->setQueryReply(Reply::Duration {42ms});

    EXPECT_TRUE(linkElement(*source, *sink));
    EXPECT_TRUE(co_await source->push(Sample {}));
    EXPECT_TRUE(co_await source->pushEvent(Event::FlushBegin {}));

    auto reply = source->queryPeer(Query::Duration {});

    EXPECT_EQ(sink->sampleCount(), 1);
    EXPECT_EQ(sink->eventCount(), 1);
    EXPECT_EQ(sink->queryCount(), 1);
    EXPECT_EQ(reply, std::optional<Reply> {Reply::Duration {42ms}});

    source->output()->unlink();
    EXPECT_FALSE(source->output()->isLinked());
    EXPECT_FALSE(sink->input()->isLinked());
    co_return;
}

ILIAS_TEST(PadTest, PropagatesInjectedPushAndEventFailures) {
    // Downstream pad errors must surface as IoTask Err on the source side.
    auto source = std::make_shared<ProbeElement>(ElementType::Source, "source");
    auto sink = std::make_shared<ProbeElement>(ElementType::Sink, "sink");
    EXPECT_TRUE(linkElement(*source, *sink));

    sink->failOperation(Operation::Push, Error::Internal);
    auto pushResult = co_await source->push(Sample {});
    EXPECT_FALSE(pushResult);
    EXPECT_EQ(pushResult.error(), make_error_code(Error::Internal));
    EXPECT_TRUE(sink->operationFailureTriggered());

    sink->failOperation(Operation::Event, Error::InvalidState);
    auto eventResult = co_await source->pushEvent(Event::FlushBegin {});
    EXPECT_FALSE(eventResult);
    EXPECT_EQ(eventResult.error(), make_error_code(Error::InvalidState));

    source->output()->unlink();
    co_return;
}

// MARK: Bin

ILIAS_TEST(BinStateTest, ChangesChildrenInTopologyOrder) {
    // Upward (to Running): sink first, then transform, then source.
    // Downward (to Null): reverse order so sources stop before sinks tear down.
    auto trace = std::make_shared<StateTrace>();
    auto bin = std::make_shared<Bin>("bin");
    auto source = std::make_shared<ProbeElement>(ElementType::Source, "source", trace);
    auto transform = std::make_shared<ProbeElement>(ElementType::Transform, "transform", trace);
    auto sink = std::make_shared<ProbeElement>(ElementType::Sink, "sink", trace);

    bin->addElements(source, transform, sink);
    EXPECT_TRUE(linkChain(*source, *transform, *sink));

    EXPECT_TRUE(co_await bin->setState(State::Running));
    EXPECT_TRUE(co_await bin->setState(State::Null));

    EXPECT_EQ(trace->snapshot(), (std::vector<StateTraceEntry> {
        {"sink", StateChange::Initialize},
        {"transform", StateChange::Initialize},
        {"source", StateChange::Initialize},
        {"sink", StateChange::Prepare},
        {"transform", StateChange::Prepare},
        {"source", StateChange::Prepare},
        {"sink", StateChange::Run},
        {"transform", StateChange::Run},
        {"source", StateChange::Run},
        {"source", StateChange::Pause},
        {"transform", StateChange::Pause},
        {"sink", StateChange::Pause},
        {"source", StateChange::Stop},
        {"transform", StateChange::Stop},
        {"sink", StateChange::Stop},
        {"source", StateChange::Teardown},
        {"transform", StateChange::Teardown},
        {"sink", StateChange::Teardown},
    }));
    co_return;
}

// MARK: Pipeline

ILIAS_TEST(PipelineStateTest, SuppliesContextAndClockWhileRunning) {
    // Pipeline owns Context and Clock; children see them only after Running.
    auto pipeline = std::make_shared<Pipeline>("pipeline");
    auto source = std::make_shared<ProbeElement>(ElementType::Source, "source");
    auto sink = std::make_shared<ProbeElement>(ElementType::Sink, "sink");

    pipeline->addElements(source, sink);
    EXPECT_TRUE(linkElement(*source, *sink));
    EXPECT_TRUE(co_await pipeline->setState(State::Running));

    EXPECT_EQ(source->context(), pipeline->context());
    EXPECT_EQ(sink->context(), pipeline->context());
    EXPECT_EQ(source->clock(), pipeline->clock());
    EXPECT_EQ(sink->clock(), pipeline->clock());
    EXPECT_NE(pipeline->context(), nullptr);
    EXPECT_NE(pipeline->clock(), nullptr);

    EXPECT_TRUE(co_await pipeline->setState(State::Null));
    EXPECT_EQ(source->state(), State::Null);
    EXPECT_EQ(sink->state(), State::Null);
    co_return;
}
