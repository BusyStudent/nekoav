#include <nekoav/elements/pipeline.hpp>
#include <nekoav/elements/bin.hpp>
#include <nekoav/element.hpp>
#include <ilias/sync.hpp> // mpsc
#include <algorithm>
#include <optional>
#include <ranges>
#include <mutex>
#include <set>
#include "log.hpp"

namespace nekoav {

// MARK: Pipeline
struct Pipeline::Impl final : public Clock { // Impl the ClockSource
    // Clock interface
    auto time() const -> Timestamp override { // TODO: MT-Safe
        if (clockPaused) { // Paused, use the last time
            return clockTime;
        }
        return std::chrono::duration_cast<Timestamp>(std::chrono::steady_clock::now() - clockEpoch);
    }

    auto category() const -> ClockCategory override { 
        return ClockCategory::System;
    }

    // To Reference pipeline
    Pipeline                             *self = nullptr;

    // Clock field
    std::chrono::steady_clock::time_point clockEpoch {};
    std::chrono::nanoseconds              clockTime {};
    bool                                  clockPaused = true;

    // Bus field
    ilias::mpsc::Sender<Message>          messageSender {}; // Use this field to send message to the (user)
    ilias::mpsc::Receiver<Message>        messageReceiver {};

    // State (MT-access)
    std::optional<std::set<Element::Ptr> > sinksNotEos; // Sink element, when all of them eos, pipeline eos, nullopt on not initialized
    std::mutex                             mutex; // Used to protect the state
};

Pipeline::Pipeline(std::string_view name) : Bin(name), d(std::make_unique<Impl>()) {
    // Initialize the self
    auto [sender, receiver] = ilias::mpsc::channel<Message>();
    d->messageSender = std::move(sender);
    d->messageReceiver = std::move(receiver);
    d->self = this;

    // Mark the typeinfo
    this->mIsPipeline = true;
}

Pipeline::~Pipeline() {

}

auto Pipeline::readMessage() -> Task<Message> {
    // This recv should never fail because we only close it when destroy the Pipeline
    auto msg = (co_await d->messageReceiver.recv()).value();
    co_return msg;
}

auto Pipeline::position() const -> std::optional<Timestamp> {
    if (auto c = mMasterClock.load(); c) {
        return c->time();
    }
    return std::nullopt;
}

auto Pipeline::sendEvent(Event event) -> IoTask<void> {
    if (event.isSeek()) {
        NEKOAV_INFO("[Pipeline] '{}' seek begin", name());
        std::lock_guard locker{d->mutex};
        d->sinksNotEos = std::nullopt;
    }
    co_return co_await Bin::sendEvent(std::move(event));
}

auto Pipeline::onInitialize() -> IoTask<void> {    
    // Check if user set the context
    if (!context()) { // Create our own context
        setContext(std::make_shared<Context>());
    }

    // Initialize the children
    auto res = co_await Bin::onInitialize();
    if (!res) {
        NEKOAV_WARN("[Pipeline] '{}' initialize failed", name());
    }
    co_return res;
}

auto Pipeline::onTeardown() -> IoTask<void> {
    auto res = co_await Bin::onTeardown();
    co_return res;
}

auto Pipeline::onRun() -> IoTask<void> {
    if (!mSorted) { // Topological changed, the clock may change
        mClocks.clear();
    }
    if (mClocks.empty()) {
        // Get all children who provide clock
        auto forEach = [&](auto self, Bin *bin) -> void {
            for (auto &child : bin->mChildren) {
                if (auto res = child->sendQuery(Query::ClockSource {}); res) {
                    auto [clock] = res->toClockSource();
                    assert(clock);
                    mClocks.emplace_back(std::move(clock));
                }
                if (child->isBin()) {
                    self(self, static_cast<Bin*>(child.get()));
                }
            }
        };
        forEach(forEach, this);

        // Add self's clock, using alias
        Clock::Ptr self {shared_from_this(), d.get()};
        mClocks.emplace_back(std::move(self));

        // Sort it by category
        std::ranges::sort(mClocks, [](const auto &lhs, const auto &rhs) { return std::to_underlying(lhs->category()) < std::to_underlying(rhs->category()); });

        // Set clock to the children
        mMasterClock.store(mClocks.front());
        setClock(mClocks.front());
    }
    if (mClocks.front().get() == d.get()) { // Use system as clock
        NEKOAV_INFO("[Pipeline] '{}' use system clock", name());
    }

    // Ok, start the bin
    if (auto res = co_await Bin::onRun(); !res) {
        co_return Err(res.error());
    }

    // Update the self clock
    auto time = d->clockTime;
    d->clockEpoch = std::chrono::steady_clock::now() - time;
    d->clockPaused = false;
    NEKOAV_INFO("[Pipeline] '{}' started, master clock: {}, num clocks source: {}", name(), mClocks.front()->time(), mClocks.size());
    co_return {};
}

auto Pipeline::onPause() -> IoTask<void> {
    auto time = d->clockTime;
    if (!mClocks.empty() && mClocks.front().get() != d.get()) {
        time = mClocks.front()->time(); // Sync the clock to master （if master is not self)
    }
    d->clockEpoch = {};
    d->clockTime = time;
    d->clockPaused = true;
    NEKOAV_INFO("[Pipeline] '{}' paused", name());
    co_return co_await Bin::onPause();
}

auto Pipeline::onStop() -> IoTask<void> {
    auto res = co_await Bin::onStop();
    // Clear the clock
    d->clockTime = {};
    d->clockEpoch = {};
    d->clockPaused = true;
    NEKOAV_INFO("[Pipeline] '{}' stopped", name());
    mClocks.clear();
    mMasterClock.store({});
    setClock({});
    co_return res;
}

auto Pipeline::onTopologyChange() -> void {
    std::lock_guard locker{d->mutex};
    d->sinksNotEos = std::nullopt; // Clear the cached sinks
}

auto Pipeline::onChildMessage(Message message) -> void {
    // Collection all sinks's eos message, if all eos message received, then send eos to bus
    if (message.isEos()) {
        auto [element] = message.toEos();
        std::lock_guard locker{d->mutex}; // TODO: Did it is UNSAFE to call this->sinks() in another thread?

        if (!d->sinksNotEos) { // Lazy initialize the sinks
            auto s = this->sinks(); // Collect it
            d->sinksNotEos.emplace(s.begin(), s.end());
        }
        d->sinksNotEos->erase(element);
        NEKOAV_INFO("[Pipeline] '{}' received '{}' EOS, {} sinks left", name(), element->name(), d->sinksNotEos->size());
        if (!d->sinksNotEos->empty()) {
            return;
        }
        d->sinksNotEos.reset();
        NEKOAV_INFO("[Pipeline] '{}' all sinks received EOS, send EOS to bus", name());
        auto _ =  d->messageSender.trySend(Message::Eos {
            .element = shared_from_this()
        });
        return;
    }

    // Put it to the user
    auto _ = d->messageSender.trySend(std::move(message));
}

} // namespace nekoav