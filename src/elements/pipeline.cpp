#include <nekoav/elements/pipeline.hpp>
#include <nekoav/elements/bin.hpp>
#include <nekoav/element.hpp>
#include <ilias/sync.hpp>
#include <algorithm>
#include <optional>
#include <ranges>
#include <set>
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

    // To Reference pipeline
    Pipeline                             *self = nullptr;

    // Clock field
    std::chrono::steady_clock::time_point clockEpoch {};
    std::chrono::nanoseconds              clockTime {};
    bool                                  clockPaused = true;
    ilias::WaitHandle<void>               clockTicking {}; // Used when the master clock is self, used to generate the clock update message

    // Bus field
    ilias::mpsc::Sender<Message>          messageSender {}; // Use this field to send message to the (user)
    ilias::mpsc::Receiver<Message>        messageReceiver {};

    // Children message filed
    ilias::WaitHandle<void>               childrenMessageWatcher {}; // Used to watch the children message
    ilias::mpsc::Sender<Message>          childrenMessageSender {}; // Used to send message to the watcher

    // State
    std::optional<std::set<Element::Ptr> > sinksNotEos; // Sink element, when all of them eos, pipeline eos, nullopt on not initialized
    bool                                   seeking = false; // Did the pipeline handle seeking ?, use atomic?

    // Method
    auto watchChildrenMessage(ilias::mpsc::Receiver<Message> receiver) -> Task<void>;
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
        d->sinksNotEos = std::nullopt;
    }
    co_return co_await Bin::sendEvent(std::move(event));
}

auto Pipeline::onInitialize() -> IoTask<void> {
    // Initialize the children channel
    assert(!d->childrenMessageWatcher && !d->childrenMessageSender);
    auto [sender, receiver] = ilias::mpsc::channel<Message>();
    d->childrenMessageSender = std::move(sender);
    d->childrenMessageWatcher = ilias::spawn(d->watchChildrenMessage(std::move(receiver)));
    
    // Check if user set the context
    if (!context()) { // Create our own context
        setContext(std::make_shared<Context>());
    }

    d->seeking = false;

    // Initialize the children
    auto res = co_await Bin::onInitialize();
    if (!res) {
        NEKOAV_WARN("[Pipeline] '{}' initialize failed", name());
        // Rollback
        d->childrenMessageWatcher.stop();
        co_await std::exchange(d->childrenMessageWatcher, {});
        d->childrenMessageSender = {};
    }
    co_return res;
}

auto Pipeline::onTeardown() -> IoTask<void> {
    assert(d->childrenMessageWatcher && d->childrenMessageSender);
    auto res = co_await Bin::onTeardown();
    
    // Join the watcher
    d->childrenMessageWatcher.stop();
    co_await std::exchange(d->childrenMessageWatcher, {});
    d->childrenMessageSender = {};
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
        setClock(mClocks.front());
    }
    if (mClocks.front().get() == d.get()) { // Use system as clock
        NEKOAV_INFO("[Pipeline] '{}' use system clock", name());
        d->clockTicking = ilias::spawn([this] -> Task<void> {
            while (true) {
                onChildMessage(Message::ClockUpdate {
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

auto Pipeline::onTopologyChange() -> void {
    d->sinksNotEos = std::nullopt; // Clear the cached sinks
}

auto Pipeline::onChildMessage(Message message) -> void {
    // Post the children message to the watcher
    auto _ = d->childrenMessageSender.trySend(std::move(message));
}

auto Pipeline::Impl::watchChildrenMessage(ilias::mpsc::Receiver<Message> receiver) -> Task<void> {
    // Handle it...
    while (auto res = co_await receiver.recv()) {
        auto &message = *res;
        // NEKOAV_INFO("[Pipeline] '{}' received message: {}", self->name(), message);
#if !defined(NDEBUG)
        // if (message.isClockUpdate()) {
        //     for (auto &clock : self->mClocks) {
        //         NEKOAV_INFO("[Pipeline] '{}' clock: {}", self->name(), clock->time());
        //     }
        // }
#endif
        // Collection all sinks's eos message, if all eos message received, then send eos to bus
        if (message.isEndOfStream()) {
            // TODO:
            auto [element] = message.toEndOfStream();
            if (!sinksNotEos) { // Lazy initialize the sinks
                auto s = self->sinks(); // Collect it
                sinksNotEos.emplace(s.begin(), s.end());
            }
            sinksNotEos->erase(element);
            NEKOAV_INFO("[Pipeline] '{}' received '{}' EOS, {} sinks left", self->name(), element->name(), sinksNotEos->size());
            if (!sinksNotEos->empty()) {
                continue;
            }
            sinksNotEos.reset();
            NEKOAV_INFO("[Pipeline] '{}' all sinks received EOS, send EOS to bus", self->name());
            auto _ =  co_await messageSender.send(Message::EndOfStream {
                .element = self->shared_from_this()
            });
            continue;
        }

        // Put it to the user
        auto _ = co_await messageSender.send(std::move(message));
    }
    NEKOAV_WARN("The channel broken?");
}

} // namespace nekoav