#pragma once

/**
 * @file probe_element.hpp
 * @brief Test double for Element: records lifecycle, samples, events, and queries.
 *
 * ProbeElement can act as Source / Transform / Sink (or bare Other),
 * inject one-shot failures, and wait asynchronously for samples or EOS.
 */

#include <nekoav/element.hpp>
#include <nekoav/error.hpp>
#include <ilias/sync.hpp>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
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

    auto clear() -> void {
        mEntries.clear();
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
 * Caps on created pads accept Caps::Any so linkElement always succeeds.
 *
 * Failure injection (failOn):
 * - Fires on the first matching call, then marks failureTriggered().
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

    // -- Data operations (require output pad) --

    /** @brief Push a sample out of this Source/Transform. */
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

    // -- Failure injection --

    /** @brief Inject a one-shot failure on a specific state change. */
    auto failOn(StateChange change, std::error_code error = Error::Internal) -> void {
        mFailure = Failure { .target = change, .error = error };
    }

    /** @brief Inject a one-shot failure on a specific pad operation. */
    auto failOn(Operation op, std::error_code error = Error::Internal) -> void {
        mFailure = Failure { .target = op, .error = error };
    }

    auto failureTriggered() const -> bool {
        return mFailure && mFailure->triggered;
    }

    auto clearFailure() -> void {
        mFailure.reset();
    }

    // -- Observation --

    /** @brief Fixed Reply returned from query callbacks. */
    auto setQueryReply(std::optional<Reply> reply) -> void {
        mQueryReply.reset();
        if (reply) {
            mQueryReply.emplace(std::move(*reply));
        }
    }

    auto samples() const -> const std::vector<SampleObservation> & {
        return mSamples;
    }

    auto eventCount() const -> size_t {
        return mEventCount;
    }

    auto queryCount() const -> size_t {
        return mQueryCount;
    }

    // -- Async waiting --

    /** @brief Suspend until at least @p count samples have been observed on input. */
    auto waitForSamples(size_t count) -> Task<void> {
        while (mSamples.size() < count) {
            co_await mSampleArrived;
        }
    }

    /** @brief Suspend until an EosEvent is received on the input pad. */
    auto waitForEndOfStream() -> Task<void> {
        while (!mEndOfStream) {
            co_await mEndOfStreamArrived;
        }
    }

    // -- Element overrides for direct query/event --

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
            co_return Err(mFailure->error);
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
    /**
     * @brief Unified failure descriptor for both state changes and pad operations.
     */
    struct Failure {
        std::variant<StateChange, Operation> target;
        std::error_code error;
        bool triggered = false;
    };

    auto onStateChange(StateChange change) -> IoTask<void> {
        if (mTrace) {
            mTrace->push(name(), change);
        }
        if (shouldFail(change)) {
            co_return Err(mFailure->error);
        }
        co_return {};
    }

    /** @brief Check if the failure should fire for a state change. */
    auto shouldFail(StateChange change) -> bool {
        if (!mFailure || mFailure->triggered) {
            return false;
        }
        auto *target = std::get_if<StateChange>(&mFailure->target);
        if (!target || *target != change) {
            return false;
        }
        mFailure->triggered = true;
        return true;
    }

    /** @brief Check if the failure should fire for a pad operation. */
    auto shouldFail(Operation operation) -> bool {
        if (!mFailure || mFailure->triggered) {
            return false;
        }
        auto *target = std::get_if<Operation>(&mFailure->target);
        if (!target || *target != operation) {
            return false;
        }
        mFailure->triggered = true;
        return true;
    }

    // -- Pad callbacks --

    auto onInputPush(Pad &, Sample sample) -> IoTask<void> {
        // Watch sample arrive
        mSamples.push_back({
            .isPacket = sample.isPacket(),
            .isAudioFrame = sample.isAudioFrame(),
            .isVideoFrame = sample.isVideoFrame(),
            .pts = sample.pts(),
        });
        mSampleArrived.set();

        if (shouldFail(Operation::Push)) {
            co_return Err(mFailure->error);
        }
        // Transform: forward downstream when linked
        if (mOutput && mOutput->isLinked()) {
            co_return co_await mOutput->push(std::move(sample));
        }
        co_return {};
    }

    auto onInputEvent(Pad &, Event event) -> IoTask<void> {
        mEventCount += 1;

        // EOF by event
        if (event.isEos()) {
            mEndOfStream = true;
            mEndOfStreamArrived.set();
        }

        if (shouldFail(Operation::Event)) {
            co_return Err(mFailure->error);
        }
        // Forward downstream when linked
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

    // -- Members --
    Pad *mInput = nullptr;
    Pad *mOutput = nullptr;
    std::shared_ptr<StateTrace> mTrace;
    std::optional<Failure> mFailure;

    std::optional<Reply> mQueryReply;
    std::vector<SampleObservation> mSamples;
    size_t mEventCount = 0;
    size_t mQueryCount = 0;
    bool mEndOfStream = false;
    ilias::Event mSampleArrived {ilias::Event::AutoClear};
    ilias::Event mEndOfStreamArrived {ilias::Event::AutoClear};
};

} // namespace nekoav::testing
