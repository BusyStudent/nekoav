#pragma once

/**
 * @file probe_element.hpp
 * @brief Test double for Element: records lifecycle, samples, events, and queries.
 *
 * ProbeElement is the main fixture used by unit and integration tests. It can
 * act as Source / Transform / Sink (or bare Other), inject one-shot failures,
 * and wait asynchronously for samples or EOS.
 */

#include <nekoav/element.hpp>
#include <nekoav/error.hpp>
#include <ilias/sync.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace nekoav::testing {

// MARK: StateTrace

/**
 * @brief One observed state-change callback (element name + StateChange kind).
 */
struct StateTraceEntry {
    std::string element;
    StateChange change;

    auto operator <=>(const StateTraceEntry &) const = default;
};

/**
 * @brief Shared recorder of lifecycle hooks across one or more ProbeElements.
 *
 * Pass the same StateTrace into several probes to assert Bin/Pipeline
 * topology order (sink→source on start, source→sink on stop).
 */
class StateTrace final {
public:
    auto push(std::string_view element, StateChange change) -> void {
        mEntries.push_back({std::string(element), change});
    }

    auto snapshot() const -> std::vector<StateTraceEntry> {
        return mEntries;
    }

private:
    std::vector<StateTraceEntry> mEntries;
};

// MARK: Operation / SampleObservation

/**
 * @brief Which pad/element path to fail when testing error propagation.
 */
enum class Operation {
    Push,
    Event,
    Query,
};

/**
 * @brief Lightweight snapshot of a Sample (avoids holding FFmpeg buffers in asserts).
 */
struct SampleObservation {
    bool isNull = false;
    bool isPacket = false;
    bool isAudioFrame = false;
    bool isVideoFrame = false;
    std::optional<Timestamp> pts;

    auto operator <=>(const SampleObservation &) const = default;
};

// MARK: ProbeElement

/**
 * @brief Configurable Element double for pad/state/pipeline tests.
 *
 * Pad layout by ElementType:
 * - Sink / Transform → input pad "in"
 * - Source / Transform → output pad "out"
 * - Other → no pads
 *
 * Caps on created pads accept Caps::Any so linkElement always succeeds unless
 * the peer rejects.
 *
 * Failure injection (failStateChange / failOperation):
 * - Fires on the N-th matching call (default N=1), then marks *Triggered().
 * - Used to verify Result error paths without real broken media.
 */
class ProbeElement final : public Element {
public:
    explicit ProbeElement(
        ElementType type = ElementType::Other,
        std::string_view name = {},
        std::shared_ptr<StateTrace> trace = {}
    ) : Element(type, name), mTrace(std::move(trace)) {
        if (type == ElementType::Sink || type == ElementType::Transform) {
            mInput = &createInputPad("in");
            mInput->mutableCaps().insertOrAssign(Caps::Any, Value::Any {});
            mInput->setPushCallback<&ProbeElement::onInputPush>(this);
            mInput->setEventCallback<&ProbeElement::onInputEvent>(this);
            mInput->setQueryCallback<&ProbeElement::onInputQuery>(this);
        }
        if (type == ElementType::Source || type == ElementType::Transform) {
            mOutput = &createOutputPad("out");
            mOutput->mutableCaps().insertOrAssign(Caps::Any, Value::Any {});
        }
    }

    auto input() const -> Pad * {
        return mInput;
    }

    auto output() const -> Pad * {
        return mOutput;
    }

    /** @brief Push a sample out of this Source/Transform (requires output pad). */
    auto push(Sample sample) -> IoTask<void> {
        if (!mOutput) {
            co_return Err(Error::InvalidState);
        }
        co_return co_await mOutput->push(std::move(sample));
    }

    /** @brief Push an event out of this Source/Transform. */
    auto pushEvent(Event event) -> IoTask<void> {
        if (!mOutput) {
            co_return Err(Error::InvalidState);
        }
        co_return co_await mOutput->pushEvent(std::move(event));
    }

    /** @brief Send a query downstream via the output pad. */
    auto queryPeer(Query query) -> std::optional<Reply> {
        if (!mOutput) {
            return std::nullopt;
        }
        return mOutput->sendQuery(std::move(query));
    }

    /**
     * @brief Fail the N-th occurrence of a lifecycle hook with @p error.
     * @param occurrence 1-based; default fails the first time.
     */
    auto failStateChange(
        StateChange change,
        std::error_code error = Error::Internal,
        size_t occurrence = 1
    ) -> void {
        mStateFailure = StateFailure {
            .change = change,
            .error = error,
            .occurrence = occurrence,
        };
    }

    /**
     * @brief Fail the N-th Push/Event/Query with @p error.
     * @param occurrence 1-based; default fails the first time.
     */
    auto failOperation(
        Operation operation,
        std::error_code error = Error::Internal,
        size_t occurrence = 1
    ) -> void {
        mOperationFailure = OperationFailure {
            .operation = operation,
            .error = error,
            .occurrence = occurrence,
        };
    }

    auto clearFailures() -> void {
        mStateFailure.reset();
        mOperationFailure.reset();
    }

    auto stateFailureTriggered() const -> bool {
        return mStateFailure && mStateFailure->triggered;
    }

    auto operationFailureTriggered() const -> bool {
        return mOperationFailure && mOperationFailure->triggered;
    }

    /** @brief Fixed Reply returned from sendQuery / pad query callbacks. */
    auto setQueryReply(std::optional<Reply> reply) -> void {
        mQueryReply.reset();
        if (reply) {
            mQueryReply.emplace(std::move(*reply));
        }
    }

    auto sampleCount() const -> size_t {
        return mSampleCount;
    }

    auto eventCount() const -> size_t {
        return mEventCount;
    }

    auto queryCount() const -> size_t {
        return mQueryCount;
    }

    auto samples() const -> std::vector<SampleObservation> {
        return mSamples;
    }

    /** @brief Suspend until at least @p count samples have been observed on input. */
    auto waitForSamples(size_t count) -> Task<void> {
        while (sampleCount() < count) {
            co_await mSampleArrived;
        }
    }

    /**
     * @brief Suspend until a null Sample (EOS) is pushed to the input pad.
     * @note Null sample is the pipeline end-of-stream convention used here.
     */
    auto waitForEndOfStream() -> Task<void> {
        while (!mEndOfStream) {
            co_await mEndOfStreamArrived;
        }
    }

    auto sendQuery(Query query) -> std::optional<Reply> override {
        mQueryCount += 1;
        if (shouldFail(Operation::Query)) {
            return std::nullopt;
        }
        return mQueryReply;
    }

    auto sendEvent(Event event) -> IoTask<void> override {
        mEventCount += 1;
        if (shouldFail(Operation::Event)) {
            co_return Err(mOperationFailure->error);
        }
        co_return {};
    }

protected:
    auto onInitialize() -> IoTask<void> override {
        return onStateChange(StateChange::Initialize);
    }

    auto onPrepare() -> IoTask<void> override {
        return onStateChange(StateChange::Prepare);
    }

    auto onRun() -> IoTask<void> override {
        return onStateChange(StateChange::Run);
    }

    auto onPause() -> IoTask<void> override {
        return onStateChange(StateChange::Pause);
    }

    auto onStop() -> IoTask<void> override {
        return onStateChange(StateChange::Stop);
    }

    auto onTeardown() -> IoTask<void> override {
        return onStateChange(StateChange::Teardown);
    }

private:
    struct StateFailure {
        StateChange change;
        std::error_code error;
        size_t occurrence = 1;
        size_t seen = 0;
        bool triggered = false;
    };

    struct OperationFailure {
        Operation operation;
        std::error_code error;
        size_t occurrence = 1;
        size_t seen = 0;
        bool triggered = false;
    };

    auto onStateChange(StateChange change) -> IoTask<void> {
        if (mTrace) {
            mTrace->push(name(), change);
        }
        if (mStateFailure && mStateFailure->change == change) {
            mStateFailure->seen += 1;
            if (mStateFailure->seen == mStateFailure->occurrence) {
                mStateFailure->triggered = true;
                co_return Err(mStateFailure->error);
            }
        }
        co_return {};
    }

    auto shouldFail(Operation operation) -> bool {
        if (!mOperationFailure || mOperationFailure->operation != operation) {
            return false;
        }
        mOperationFailure->seen += 1;
        if (mOperationFailure->seen != mOperationFailure->occurrence) {
            return false;
        }
        mOperationFailure->triggered = true;
        return true;
    }

    auto onInputPush(Pad &, Sample sample) -> IoTask<void> {
        mSampleCount += 1;
        auto isEndOfStream = sample.isNull();
        {
            mSamples.push_back({
                .isNull = sample.isNull(),
                .isPacket = sample.isPacket(),
                .isAudioFrame = sample.isAudioFrame(),
                .isVideoFrame = sample.isVideoFrame(),
                .pts = sample.pts(),
            });
        }
        mSampleArrived.set();
        if (isEndOfStream) {
            mEndOfStream = true;
            mEndOfStreamArrived.set();
        }
        if (shouldFail(Operation::Push)) {
            co_return Err(mOperationFailure->error);
        }
        // Transform: forward to output when linked; Sink: drop after recording.
        if (mOutput && mOutput->isLinked()) {
            co_return co_await mOutput->push(std::move(sample));
        }
        co_return {};
    }

    auto onInputEvent(Pad &, Event event) -> IoTask<void> {
        mEventCount += 1;
        if (shouldFail(Operation::Event)) {
            co_return Err(mOperationFailure->error);
        }
        if (mOutput && mOutput->isLinked()) {
            co_return co_await mOutput->pushEvent(std::move(event));
        }
        co_return {};
    }

    auto onInputQuery(Pad &, Query &query) -> std::optional<Reply> {
        mQueryCount += 1;
        if (shouldFail(Operation::Query)) {
            return std::nullopt;
        }
        return mQueryReply;
    }

    Pad *mInput = nullptr;
    Pad *mOutput = nullptr;
    std::shared_ptr<StateTrace> mTrace;
    std::optional<StateFailure> mStateFailure;
    std::optional<OperationFailure> mOperationFailure;

    std::optional<Reply> mQueryReply;
    std::vector<SampleObservation> mSamples;
    size_t mSampleCount = 0;
    size_t mEventCount = 0;
    size_t mQueryCount = 0;
    bool mEndOfStream = false;
    ilias::Event mSampleArrived {ilias::Event::AutoClear};
    ilias::Event mEndOfStreamArrived {ilias::Event::AutoClear};
};

} // namespace nekoav::testing
