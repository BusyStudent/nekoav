#include <nekoav/elements/queue.hpp>
#include <ilias/task.hpp>
#include <ilias/sync.hpp>
#include <deque>
#include "internal.hpp"

namespace nekoav {

struct Queue::Impl {
    // Data
    std::deque<Sample> queue;
    size_t queueCapacity = 100;

    // Sync
    ilias::Event queueHasSpace {ilias::Event::AutoClear};
    ilias::Event queueHasSample;
    ilias::Event pauseRequested;

    ilias::WaitHandle<void> pullWorker;

    // Pads
    Pad *in = nullptr;
    Pad *out = nullptr;
};

Queue::Queue(std::string_view name) : Element(name), d(std::make_unique<Impl>()) {
    
    auto &in = createInputPad("in");
    auto &out = createOutputPad("out");

    // Bind it
    in.setPushCallback<&Queue::onPush>(this);
    in.setEventCallback<&Queue::onEvent>(this);

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

auto Queue::onPush(Pad &, Sample sample) -> IoTask<void> {
    while (d->queue.size() >= d->queueCapacity) { // Wait for space
        co_await d->queueHasSpace;
    }
    d->queue.emplace_back(std::move(sample));
    d->queueHasSample.set();
    co_return {};
}

auto Queue::onEvent(Pad &, Event event) -> IoTask<void> {
    // TODO: Maybe we need mutex with push?
    if (event.isFlushBegin()) {
        NEKOAV_INFO("[Queue] '{}' flush begin", name());
        d->queue.clear();
    }
    if (event.isFlushEnd()) {
        NEKOAV_INFO("[Queue] '{}' flush end", name());
        d->queueHasSpace.set();
    }
    co_return co_await d->out->pushEvent(event); // Forward the event
}

auto Queue::onStop() -> IoTask<void> {
    d->queue.clear();
    d->queueHasSpace.set();
    co_return {};
}

auto Queue::doPull() -> Task<void> {
    // Maek the queue name
    co_await ilias::this_coro::setName(name());
    
    while (true) {
        if (d->pauseRequested.isSet()) {
            break;
        }
        while (d->queue.empty()) { // Wait for sample
            d->queueHasSample.clear();
            auto [_, pause] = co_await ilias::whenAny(d->queueHasSample.wait(), d->pauseRequested.wait());
            if (pause) { // The queue is paused or something, return immediately
                NEKOAV_INFO("[Queue] '{}' paused got, pull worker quiting", name());
                co_return;
            }
        }
        auto sample = std::move(d->queue.front());
        d->queue.pop_front();
        d->queueHasSpace.set();

        // Push it to the pad
        if (auto res = co_await d->out->push(std::move(sample)); !res) {
            NEKOAV_INFO("[Queue] '{}' failed to push sample to the pad => {}", name(), res.error().message());
            setErrorState(res.error());
            co_return;
        }
    }
}


} // namespace nekoav