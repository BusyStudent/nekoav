#include <nekoav/queue.hpp>
#include <ilias/task.hpp>
#include <ilias/sync.hpp>
#include <deque>
#include "internal.hpp"

namespace nekoav {

struct Queue::Impl {
    // Data
    std::deque<Sample::Ptr> queue;
    size_t queueCapacity = 10;

    // Sync
    ilias::Event queueHasSample;
    ilias::Event queueHasSpace;
    ilias::Event pauseRequested;

    ilias::WaitHandle<void> pullWorker;

    // Pads
    Pad *in = nullptr;
    Pad *out = nullptr;

    Impl() { // Configure event
        queueHasSpace.setAutoClear(true);
    }
};

Queue::Queue(std::string_view name) : Element(name), d(std::make_unique<Impl>()) {
    
    auto &in = createInputPad("in");
    auto &out = createOutputPad("out");

    // Configure, we accept anything
    in.mutableCaps() = Caps::makeAny();
    out.mutableCaps() = Caps::makeAny();

    // Bind it
    in.setPushCallback<&Queue::onPush>(this);

    d->in = &in;
    d->out = &out;
}

Queue::~Queue() {
    assert(!d->pullWorker); // Must be waited
}

auto Queue::onRun() -> IoTask<void> {
    assert(!d->pullWorker); // Must be waited or not started
    d->pauseRequested.clear();
    d->pullWorker = ilias::spawn(doPull());
    co_return {};
}

auto Queue::onPause() -> IoTask<void> {
    d->pauseRequested.set();
    co_await std::exchange(d->pullWorker, {});
    co_return {};
}

auto Queue::onPush(Sample::Ptr sample) -> IoTask<void> {
    while (d->queue.size() >= d->queueCapacity) { // Wait for space
        co_await d->queueHasSpace;
    }
    d->queue.push_back(std::move(sample));
    d->queueHasSample.set();
    co_return {};
}

auto Queue::doPull() -> Task<void> {
    while (!d->pauseRequested.isSet()) {
        while (d->queue.empty()) { // Wait for sample
            d->queueHasSample.clear();
            auto [_, pause] = co_await ilias::whenAny(d->queueHasSample.wait(), d->pauseRequested.wait());
            if (pause) { // The queue is paused or something, return immediately
                logger::info("[Queue] paused got, pull worker quiting");
                co_return;
            }
        }
        auto sample = std::move(d->queue.front());
        d->queue.pop_front();
        d->queueHasSpace.set();

        // Push it to the pad
        if (auto res = co_await ilias::unstoppable(d->out->push(std::move(sample))); !res) {
            // Failed to push, what shout i do?
            logger::error("[Queue] failed to push sample to the pad => {}", res.error().message());
            co_return;
        }
    }
}


} // namespace nekoav