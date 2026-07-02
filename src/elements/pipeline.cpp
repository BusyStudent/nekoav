#include <nekoav/elements/pipeline.hpp>
#include <nekoav/elements/bin.hpp>
#include <nekoav/element.hpp>
#include <ilias/sync.hpp>
#include <algorithm>
#include <ranges>
#include "log.hpp"

namespace nekoav {

// MARK: Pipeline
struct Pipeline::Impl final : public Clock { // Impl the ClockSource
    // Clock interface
    auto time() const -> Timestamp override { 
        if (clockPaused) { // Paused, use the last time
            return clockTime;
        }
        return std::chrono::duration_cast<Timestamp>(std::chrono::steady_clock::now() - clockEpoch);
    }

    auto category() const -> ClockCategory override { 
        return ClockCategory::System;
    }

    // Bus
    auto watchBus(ilias::mpsc::Receiver<Message> receiver) -> Task<void>;

    // To Reference pipeline
    Pipeline                             *self = nullptr;

    // Clock field
    std::chrono::steady_clock::time_point clockEpoch {};
    std::chrono::nanoseconds              clockTime {};
    bool                                  clockPaused = true;
    ilias::WaitHandle<void>               clockTicking {}; // Used when the master clock is self, used to generate the clock update message

    // Bus field
    bool                                  seeking = false; // Did the pipeline handle seeking ?, use atomic?
    ilias::WaitHandle<void>               busMonitor {};
    ilias::mpsc::Sender<Message>          messageSender {}; // Use this field to send message to the (user)
    ilias::mpsc::Receiver<Message>        messageReceiver {};
};

Pipeline::Pipeline(std::string_view name) : Bin(name), d(std::make_unique<Impl>()), mContext(std::make_unique<Context>()) {
    // Initialize the self
    auto [sender, receiver] = ilias::mpsc::channel<Message>();
    d->messageSender = std::move(sender);
    d->messageReceiver = std::move(receiver);
    d->self = this;

    // Mark the typeinfo
    mIsPipeline = true;
}

Pipeline::~Pipeline() {

}

auto Pipeline::readMessage() -> Task<Message> {
    // This recv should never fail because we only close it when destroy the Pipeline
    while (true) {
        auto msg = (co_await d->messageReceiver.recv()).value();
        if (msg.isClockUpdate() & d->seeking) { // Discard out of date clock update message
            continue;
        }
        else if (msg.isSeekEnd()) { // Seek is done
            NEKOAV_INFO("[Pipeline] '{}', seek end", name());
            d->seeking = false;
        }
        co_return msg;
    }
}

auto Pipeline::sendEvent(Event event) -> IoTask<void> {
    if (event.isSeek()) {
        NEKOAV_INFO("[Pipeline] '{}' seek begin", name());
        d->seeking = true;
    }
    co_return co_await Bin::sendEvent(std::move(event));
}

auto Pipeline::onInitialize() -> IoTask<void> {
    // pipelineBus initialize onInitialize, cleanup onTeardown
    assert(!d->busMonitor);
    auto [sender, receiver] = ilias::mpsc::channel<Message>();
    d->busMonitor = ilias::spawn(d->watchBus(std::move(receiver)));
    d->seeking = false;
    setPipelineBus(sender);// Set the bus to self and all children
    co_return co_await Bin::onInitialize();
}

auto Pipeline::onTeardown() -> IoTask<void> {
    assert(d->busMonitor);
    d->busMonitor.stop();
    co_await std::exchange(d->busMonitor, {});
    setPipelineBus({}); // Clear the bus
    co_return co_await Bin::onTeardown();
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
                if (child->mIsBin) {
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
        setClock(mClocks.front());
    }
    if (mClocks.front().get() == d.get()) { // Use system as clock
        NEKOAV_INFO("[Pipeline] '{}' use system clock", name());
        d->clockTicking = ilias::spawn([this] -> Task<void> {
            while (true) {
                auto _ = co_await pipelineBus().send(Message::ClockUpdate {
                    .clock = clock(),
                    .time = d->time(),
                });
                co_await ilias::sleep(std::chrono::seconds {1});
            }
        });
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
    if (d->clockTicking) { // Stop the ticking 
        d->clockTicking.stop();
        co_await std::exchange(d->clockTicking, {});
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
    setClock({});
    co_return res;
}

auto Pipeline::Impl::watchBus(ilias::mpsc::Receiver<Message> receiver) -> Task<void> {
    while (auto res = co_await receiver.recv()) {
        auto &event = *res;
        // Handle it...
        // NEKOAV_INFO("[Pipeline] '{}' received event: {}", self->name(), event);
// #if !defined(NDEBUG)
//         if (event.isClockUpdate()) {
//             for (auto &clock : self->mClocks) {
//                 NEKOAV_INFO("[Pipeline] '{}' clock: {}", self->name(), clock->time());
//             }
//         }
// #endif

        // Put it to the user
        auto _ = co_await messageSender.send(std::move(event));
    }
}

} // namespace nekoav