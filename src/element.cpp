#include <nekoav/element.hpp>
#include <nekoav/error.hpp>
#include <ilias/task.hpp>
#include <unordered_map>
#include <algorithm>
#include <ranges>
#include <queue>
#include <print>
#include "internal.hpp"

namespace nekoav {

// Some internals
namespace {
    auto stateChangeOf(State cur, State target) -> StateChange {
        if (cur == State::Null && target == State::Ready) {
            return StateChange::Initialize;
        }
        else if (cur == State::Ready && target == State::Paused) {
            return StateChange::Prepare;
        }
        else if (cur == State::Paused && target == State::Running) {
            return StateChange::Run;
        }
        else if (cur == State::Running && target == State::Paused) {
            return StateChange::Pause;
        }
        else if (cur == State::Paused && target == State::Ready) {
            return StateChange::Stop;
        }
        else if (cur == State::Ready && target == State::Null) {
            return StateChange::Teardown;
        }
        logger::error("Invalid state transition from {} to {}", cur, target);
        ::abort(); // Invalid state transition
    }
} // namespace

// MARK: Pad
auto Pad::unlink() -> void {
    if (!mPeer) {
        return;
    }
    mPeer->mPeer = nullptr;
    mPeer = nullptr;

    // Mark Topology is changed
    if (mElement.mParent) {
        mElement.mParent->mSorted = false;
    }
    if (mElement.mState == State::Running) {
        logger::error("[Pad] Topology changed while element '{}' is running, this may cause undefined behavior", mElement.name());
    }
}

auto Pad::link(Pad &peer) -> bool {
    if (isLinked() || peer.isLinked()) {
        return false;
    }
    if (mType == peer.mType) {
        return false;
    }
    mPeer = &peer;
    peer.mPeer = this;
    
    // Mark Topology is changed
    if (mElement.mParent) {
        mElement.mParent->mSorted = false;
    }
    if (mElement.mState == State::Running) {
        logger::error("[Pad] Topology changed while element '{}' is running, this may cause undefined behavior", mElement.name());
    }
    return true;
}

auto Pad::push(Sample sample) -> IoTask<void> {
    if (!isLinked()) {
        co_return Err(Error::NotLinked);
    }
    if (!mPeer->mPushCallback) {
        logger::debug("No push callback set on pad '{}'", mPeer->name());
        co_return Err(Error::NoPushCallback);
    }
    co_return co_await mPeer->mPushCallback(*mPeer, std::move(sample));
}

auto Pad::pushEvent(Event event) -> IoTask<void> {
    auto walkToUp = mType == PadType::Input; // If self is input pad, we walk upstream
    auto cur = peer();
    if (!cur) {
        co_return Err(Error::NotLinked);
    }
    auto &element = cur->mElement;
    logger::info("[Pad] push event '{}' to element '{}', pad '{}'", event, element.name(), cur->name());
    if (cur->mEventCallback) {
        if (auto res = co_await cur->mEventCallback(*cur, event); !res) {
            logger::error("Failed to push event to pad '{}': {}", cur->name(), res.error().message());
            co_return Err(res.error());
        }
        co_return {};
    }
    // Continue walk to find an handler
    if (walkToUp) {
        for (auto &pad : element.inputs()) {
            if (auto res = co_await pad.pushEvent(event); !res) {
                co_return Err(res.error());
            }
        }
    }
    else {
        for (auto &pad : element.outputs()) {
            if (auto res = co_await pad.pushEvent(event); !res) {
                co_return Err(res.error());
            }
        }
    }
    co_return {};
}

auto Pad::sendQuery(Query query) -> std::optional<Reply> {
    auto walkToUp = mType == PadType::Input; // If self is input pad, we walk upstream
    auto cur = peer();
    if (cur) {
        auto &element = cur->mElement;
        logger::info("[Pad] send query '{}' to element '{}', pad '{}'", query, element.name(), cur->name());
        if (cur->mQueryCallback) {
            auto res = cur->mQueryCallback(*cur, query);
            if (res) {
                logger::info("[Pad] query '{}' is handled by pad '{}' => {}", query, cur->name(), *res);
            }
            return res;
        }
        // Continue to find an handler
        if (walkToUp) {
            for (auto &pad : element.inputs()) {
                if (auto res = pad.sendQuery(query); res) {
                    return res;
                }
            }
        }
        else {
            for (auto &pad : element.outputs()) {
                if (auto res = pad.sendQuery(query); res) {
                    return res;
                }
            }
        }
    }
    return std::nullopt;
}

// MARK: Element
Element::Element(std::string_view name) : mName(name) {
    if (mName.empty()) {
        mName = "#Element " + std::to_string(std::bit_cast<uintptr_t>(this));
    }
}

Element::~Element() {
    // We could only destroy the Element when it is in null
    if (mState != State::Null) {
        ::fprintf(stderr, "Invalid state on element '%s', element could only be destroyed when it state is null", mName.c_str());
        ::abort();
    }
}

auto Element::setState(State targetState) -> IoTask<void> {
    if (targetState == mState) { // Same state, no-op
        co_return {};
    }

    // Check
    if (mStateChanging) {
        co_return Err(Error::InBusy);
    }
    struct Guard {
        ~Guard() {
            element.mStateChanging = false;
        }

        Element &element;
    } guard {*this};
    mStateChanging = true;
    
    // Do transations
    // Check is forward (Null -> Running)
    // Backward is (Running -> NUll)
    auto isForward = toUnderlying(targetState) > toUnderlying(mState);
    auto nextState = [&](State state) {
        auto value = toUnderlying(state);
        if (isForward) {
            value += 1;
        }
        else {
            value -= 1;
        }
        return State {value};
    };

    // Check is error state
    if (mError) {
        if (isForward) { // Only allow backward to teardown
            co_return Err(Error::InvalidState);
        }
    }

    // Do transation
    for (auto cur = mState; cur != targetState; cur = nextState(cur)) {
        auto change = stateChangeOf(cur, nextState(cur));
        auto task = IoTask<void> {};

        switch (change) {
            case StateChange::Initialize: task = onInitialize(); break;
            case StateChange::Prepare:    task = onPrepare(); break;
            case StateChange::Run:        task = onRun(); break;
            case StateChange::Pause:      task = onPause(); break;
            case StateChange::Stop:       task = onStop(); break; // Clear any clock and bus
            case StateChange::Teardown:   task = onTeardown(); break;
        }
        logger::info("[Element] '{}' Change state from '{}' to '{}'", mName, cur, nextState(cur));
        if (auto res = co_await std::move(task); !res && isForward) { // FORWARD, FAILED!!!
            mError = res.error();
            co_return Err(res.error());
        }
        else if (!res) { // BACKWARD, FAILED!!!
            mError = res.error();
            logger::warn("[Element] '{}' Failed to backward to state '{}': {}, ignore it", mName, nextState(cur), res.error().message());
        }

        // Done transation
        mState = nextState(cur);
    }

    // Done
    mState = targetState;
    if (mState == State::Null) {
        mError.clear();
    }
    co_return {};
}

auto Element::setName(std::string_view name) -> void {
    mName = name;
    if (mName.empty()) {
        mName = "#Element " + std::to_string(std::bit_cast<uintptr_t>(this));
    }
}

auto Element::sendQuery(Query query) -> std::optional<Reply> {
    return std::nullopt;
}

auto Element::onInitialize() -> IoTask<void> { 
    co_return {}; 
}

auto Element::onPrepare() -> IoTask<void> { 
    co_return {}; 
}

auto Element::onRun() -> IoTask<void> { 
    co_return {}; 
}

auto Element::onPause() -> IoTask<void> { 
    co_return {}; 
}

auto Element::onStop() -> IoTask<void> { 
    co_return {}; 
}

auto Element::onTeardown() -> IoTask<void> { 
    co_return {}; 
}

auto Element::createInputPad(std::string_view name) -> Pad & {
    return mInputs.emplace_back(*this, PadType::Input, name);
}

auto Element::createOutputPad(std::string_view name) -> Pad & {
    return mOutputs.emplace_back(*this, PadType::Output, name);
}

auto Element::setErrorState(std::error_code errc) -> void {
    logger::error("[Element] set error state: {}", errc.message());
    mError = errc;
    if (mPipelineBus) {
        auto _ = mPipelineBus.trySend(Event::Error {errc});
    }
}

auto Element::pipeline() const -> const Pipeline * {
    auto cur = this;
    while (cur) {
        if (cur->mIsPipeline) {
            return static_cast<const Pipeline *>(cur);
        }
        cur = cur->mParent;
    }
    return nullptr;
}

auto Element::context() const -> Context * {
    if (auto pipeline = this->pipeline(); pipeline) {
        return pipeline->mContext.get();
    }
    return nullptr;
}

// MARK: Set Clock
auto Element::setClock(Clock::Ptr clock) -> void {
    mClock = clock;
    if (mIsBin) {
        for (auto &child : static_cast<Bin *>(this)->mChildren) {
            child->setClock(clock);
        }
    }
}

auto Element::setPipelineBus(ilias::mpsc::Sender<Event> bus) -> void {
    mPipelineBus = bus;
    if (mIsBin) {
        for (auto &child : static_cast<Bin *>(this)->mChildren) {
            child->setPipelineBus(bus);
        }
    }
}

auto Element::dumpInfoInternal(FILE *where, int level) -> void {
    auto indent = [level](int extra = 0) {
        return std::string(level + extra, ' '); 
    };

    auto dumpCaps = [&](const Caps &caps, int lv) {
        if (caps.empty()) return;
        for (const auto &[name, value] : caps) {
            if (value.isNull()) {
                std::print(where, "{:{}}• {}\n", "", lv, name);
            }
            else {
                std::print(where, "{:{}}• {}: {}\n", "", lv, name, value);
            }
        }
    };

    auto dumpPad = [&](Pad &pad, int lv, bool isInput) {
        std::string_view arrow = isInput ? "<-" : "->";
        std::string_view linkState = pad.isLinked() ? "[Linked]" : "[Unlinked]";
        
        std::print(where, "{:{}}{} '{}' {}\n", "", lv, arrow, pad.name(), linkState);
        dumpCaps(pad.caps(), lv + 3); 
    };

    // Element ： [State] Name
    //   Clock
    //   Bus
    std::print(where, "{:{}}[{}] {}\n", "", level, mState, mName);
    std::print(where, "{:{}}Clock: {}\n", "", level + 2, static_cast<const void*>(mClock.get()));
    std::print(where, "{:{}}Bus: {}\n", "", level + 2, mPipelineBus ? "Have" : "None");

    //   Caps
    if (!mInputs.empty()) {
        std::print(where, "{:{}}Inputs:\n", "", level + 2);
        for (auto &pad : mInputs) {
            dumpPad(pad, level + 4, true);
        }
    }

    if (!mOutputs.empty()) {
        std::print(where, "{:{}}Outputs:\n", "", level + 2);
        for (auto &pad : mOutputs) {
            dumpPad(pad, level + 4, false);
        }
    }
}

// MARK: Bin
Bin::Bin(std::string_view name) : Element(name) {
    mIsBin = true;
}

Bin::~Bin() {

}

// Emm? maybe we should make setState to virtual ?
auto Bin::addElement(Element::Ptr element) -> void {
    if (!element) {
        return;
    }
    assert(!element->mIsPipeline); // Can't add an pipeline to a bin
    // Set the member belong the bin
    element->mParent = this;
    element->setClock(clock());
    element->setPipelineBus(pipelineBus());
    mChildren.emplace_back(std::move(element));
    mSorted = false;
}

auto Bin::addElementSync(Element::Ptr element) -> IoTask<void> {
    if (!element) {
        co_return Err(Error::InvalidArguments);
    }
    addElement(element);
    // Async state here
    if (auto res = co_await element->setState(state()); !res) {
        removeElement(element);
        co_return Err(res.error());
    }
    co_return {};
}

auto Bin::removeElement(Element::Ptr element) -> bool {
    if (!element) {
        return false;
    }
    auto it = std::ranges::find(mChildren, element);
    if (it == mChildren.end()) {
        return false;
    }
    // Remove the member belong the bin
    (*it)->mParent = nullptr;
    (*it)->setClock({});
    (*it)->setPipelineBus({});
    mChildren.erase(it);
    mSorted = false;
    return true;
}

auto Bin::syncElements() -> IoTask<void> {
    return setChildrenState(state());
}

auto Bin::dumpInfoInternal(FILE * where, int level) -> void {
    Element::dumpInfoInternal(where, level);
    ::fprintf(where, "%*s  Children:\n", level, "");
    for (auto &child : mChildren) {
        child->dumpInfoInternal(where, level + 4);
    }
}

auto Bin::onInitialize() -> IoTask<void> {
    logger::info("[Bin] '{}' initializing children", name());
    return setChildrenState(State::Ready);
}

auto Bin::onPrepare() -> IoTask<void> {
    logger::info("[Bin] '{}' preparing children", name());
    return setChildrenState(State::Paused);
}

auto Bin::onRun() -> IoTask<void> {
    logger::info("[Bin] '{}' running children", name());
    return setChildrenState(State::Running);
}

auto Bin::onPause() -> IoTask<void> {
    logger::info("[Bin] '{}' pausing children", name());
    return setChildrenState(State::Paused);
}

auto Bin::onStop() -> IoTask<void> {
    logger::info("[Bin] '{}' stopping children", name());
    return setChildrenState(State::Ready);
}

auto Bin::onTeardown() -> IoTask<void> {
    logger::info("[Bin] '{}' tearing down children", name());
    return setChildrenState(State::Null);
}

auto Bin::setChildrenState(State newState) -> IoTask<void> {
    // Check if we need to sort
    if (!mSorted) {
        if (!topologicalSort()) {
            co_return Err(Error::InvalidTopology);
        }
        mSorted = true;
        logger::info("[Bin] '{}' topological sort done", name());
    }
    // Check we are init(forward) or shutdown(backword)
    static_assert(toUnderlying(State::Running) > toUnderlying(State::Null));
    bool forward = toUnderlying(newState) > toUnderlying(state());
    if (forward) { // Forward
        for (auto &child : mChildren | std::views::reverse) { // From sink to source
            if (auto res = co_await child->setState(newState); !res) {
                co_return Err(res.error());
            }
        }
    }
    else { // Backward
        for (auto &child : mChildren) { // From source to sink
            if (auto res = co_await child->setState(newState); !res) { // Backward will ignore the error
                logger::warn("[Bin] '{}' child '{}' failed to set state to '{}', error: {}", name(), child->name(), newState, res.error().message());
            }
        }
    }
    co_return {};
}

auto Bin::topologicalSort() -> bool {
    if (mChildren.empty()) {
        return true; // No children, no-op
    }

    // Init inDegrees...
    auto inDegrees = std::unordered_map<Element *, size_t>{};
    for (auto &child : mChildren) {
        inDegrees[child.get()] = 0;
    }

    for (auto &child : mChildren) {
        for (auto &output : child->outputs()) {
            if (output.isLinked()) {
                inDegrees[output.peerElement()] += 1;
            }
        }
    }

    // Topological sort
    auto sorted = std::vector<Element::Ptr>{};
    auto queue = std::queue<Element *>{};
    for (auto &[element, degree] : inDegrees) {
        if (degree == 0) {
            queue.push(element);
        }
    }

    while (!queue.empty()) {
        auto curElement = queue.front();
        queue.pop();

        sorted.push_back(curElement->shared_from_this());
        for (auto &output : curElement->outputs()) {
            if (!output.isLinked()) {
                continue;
            }
            auto peerElement = output.peerElement();
            auto &peerInDegree = inDegrees[peerElement];
            peerInDegree -= 1;
            if (peerInDegree == 0) {
                queue.push(peerElement);
            }
        }
    }

    // Check
    if (sorted.size() != mChildren.size()) {
        logger::error("[Bin] '{}' topological sort failed, cycle detected", name());
        return false; // Circle detected
    }
    else {
        mChildren = std::move(sorted);
        return true;
    }
}

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
    auto watchBus(ilias::mpsc::Receiver<Event> receiver) -> Task<void>;

    // To Reference pipeline
    Pipeline                             *self = nullptr;

    // Clock field
    std::chrono::steady_clock::time_point clockEpoch {};
    std::chrono::nanoseconds              clockTime {};
    bool                                  clockPaused = true;
    ilias::WaitHandle<void>               clockTicking {}; // Used when the master clock is self, used to generate the clock update event

    // Bus field
    ilias::WaitHandle<void>               busMonitor {};
};

Pipeline::Pipeline(std::string_view name) : Bin(name), d(std::make_unique<Impl>()), mContext(std::make_unique<Context>()) {
    mIsPipeline = true;
    d->self = this;
}

Pipeline::~Pipeline() {

}

auto Pipeline::onInitialize() -> IoTask<void> {
    // pipelineBus initialize onInitialize, cleanup onTeardown
    assert(!d->busMonitor);
    auto [sender, receiver] = ilias::mpsc::channel<Event>();
    d->busMonitor = ilias::spawn(d->watchBus(std::move(receiver)));
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
        for (auto &child : mChildren) {
            if (auto res = child->sendQuery(Query::ClockSource {}); res) {
                auto [clock] = res->toClockSource();
                assert(clock);
                mClocks.emplace_back(std::move(clock));
            }
        }

        // Add self's clock, using alias
        auto self = Clock::Ptr {shared_from_this(), d.get()};
        mClocks.emplace_back(std::move(self));

        // Sort it by category
        std::ranges::sort(mClocks, [](const auto &lhs, const auto &rhs) { return toUnderlying(lhs->category()) < toUnderlying(rhs->category()); });

        // Set clock to the children
        setClock(mClocks.front());
    }
    if (mClocks.front().get() == d.get()) { // Use system as clock
        logger::info("[Pipeline] '{}' use system clock", name());
        d->clockTicking = ilias::spawn([this] -> Task<void> {
            while (true) {
                auto _ = co_await pipelineBus().send(Event::ClockUpdate {
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
    logger::info("[Pipeline] '{}' started, master clock: {}, num clocks source: {}", name(), mClocks.front()->time(), mClocks.size());
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
    logger::info("[Pipeline] '{}' paused", name());
    co_return co_await Bin::onPause();
}

auto Pipeline::onStop() -> IoTask<void> {
    auto res = co_await Bin::onStop();
    // Clear the clock
    d->clockTime = {};
    d->clockEpoch = {};
    d->clockPaused = true;
    logger::info("[Pipeline] '{}' stopped", name());
    mClocks.clear();
    setClock({});
    co_return res;
}

auto Pipeline::Impl::watchBus(ilias::mpsc::Receiver<Event> receiver) -> Task<void> {
    while (auto res = co_await receiver.recv()) {
        auto &event = *res;
        logger::info("[Pipeline] '{}' received event: {}", self->name(), event);
    }
}


// MARK: Utils
auto linkElement(Element &src, std::string_view srcPadName, Element &dst, std::string_view dstPadName) -> bool {
    auto srcPad = std::ranges::find_if(src.outputs(), [&](auto &pad) { return pad.name() == srcPadName; });
    auto dstPad = std::ranges::find_if(dst.inputs(), [&](auto &pad) { return pad.name() == dstPadName; });
    if (srcPad != src.outputs().end() && dstPad != dst.inputs().end()) {
        return srcPad->link(*dstPad);
    }
    return false;
}

auto linkElement(Element &src, Element &dst) -> bool {
    return linkElement(src, "out", dst, "in");
}

auto toString(State state) -> std::string_view {
    switch (state) {
        case State::Null:    return "Null";
        case State::Ready:   return "Ready";
        case State::Paused:  return "Paused";
        case State::Running: return "Running";
        default:             return "Unknown"; // Impossible!
    }
}

} // namespace nekoav
