#include <nekoav/element.hpp>
#include <nekoav/error.hpp>
#include <ilias/task.hpp>
#include <unordered_map>
#include <algorithm>
#include <ranges>
#include <queue>
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
        logger::error("Invalid state transition from {} to {}", toString(cur), toString(target));
        ::abort(); // Invalid state transition
    }

    auto dumpValue(FILE *where, const Value &value) -> void {
        const auto visitor = Overloads {
            [&](auto &_) {
                ::fprintf(where, "not impl yet");
            },
            [&](const std::string &str) {
                ::fprintf(where, "'%s'", str.c_str());
            },
            [&](int64_t num) {
                ::fprintf(where, "%lld", num);
            },
            [&](double num) {
                ::fprintf(where, "%lf", num);
            },
            [&](bool b) {
                ::fprintf(where, "%s", b ? "true" : "false");
            },
            [&](std::monostate) {
                ::fprintf(where, "null");
            },
            [&](PixelFormat fmt) {
                ::fprintf(where, "%s", toString(fmt).data());
            },
            [&](ColorRange range) {
                ::fprintf(where, "%s", toString(range).data());
            },
            [&](ColorPrimaries pri) {
                ::fprintf(where, "%s", toString(pri).data());
            },
            [&](ColorTransfer transfer) {
                ::fprintf(where, "%s", toString(transfer).data());
            },
            [&](ColorSpace space) {
                ::fprintf(where, "%s", toString(space).data());
            },
            [&](SampleFormat fmt) {
                ::fprintf(where, "%s", toString(fmt).data());
            },
            [&](std::chrono::nanoseconds ns) {
                if (ns.count() % 1'000'000 == 0) {
                    ::fprintf(where, (std::to_string(ns.count() / 1'000'000) + "ms").c_str());
                }
                else {
                    ::fprintf(where, (std::to_string(ns.count()) + "ns").c_str());
                }
            },
            [&](Rational r) {
                ::fprintf(where, "%d / %d", r.num, r.den);
            },
            [&](const Value::Bytes &bytes) {
                ::fprintf(where, "bytes[%zu]", bytes.size());
            },
            [&](const Value::List &list) {
                ::fprintf(where, "[");
                for (auto &value : list) {
                    dumpValue(where, value);
                    ::fprintf(where, ", ");
                }
                ::fprintf(where, "]");
            },
            [&](const Value::Map &map) {
                ::fprintf(where, "{");
                for (auto &[key, value] : map) {
                    ::fprintf(where, "'%s': ", key.c_str());
                    dumpValue(where, value);
                    ::fprintf(where, ", ");
                }
                ::fprintf(where, "}");
            },
        };
        value.visit(visitor);
    };
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
    if (cur) {
        auto &element = cur->mElement;
        logger::info("[Pad] push event to element '{}', pad '{}'", element.name(), cur->name());
        if (cur->mEventCallback) {
            if (auto res = co_await cur->mEventCallback(*cur, event); !res) {
                logger::error("Failed to push event to pad '{}': {}", cur->name(), res.error().message());
                co_return Err(res.error());
            }
            if (event.consumed()) {
                co_return {};
            }
        }
        // Continue walk
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
    }
    co_return {};
}

auto Pad::sendQuery(Query query) -> std::optional<Reply> {
    auto walkToUp = mType == PadType::Input; // If self is input pad, we walk upstream
    auto cur = peer();
    if (cur) {
        auto &element = cur->mElement;
        logger::info("[Pad] send query to element '{}', pad '{}'", element.name(), cur->name());
        if (cur->mQueryCallback) {
            auto res = cur->mQueryCallback(*cur, query);
            if (res) {
                return res;
            }
        }
        // Continue
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
            case StateChange::Stop:       task = onStop(); break;
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
    // TODO: Handle error
    logger::error("[Element] set error state: {}", errc.message());
    mError = errc;
}

auto Element::dumpInfoInternal(FILE *where, int level) -> void {
    auto dumpCaps = [&](const Caps &caps, int lv) {
        for (auto &[name, value] : caps) {
            ::fprintf(where, "%*s Caps: '%s' : ", lv, "", name.c_str());
            dumpValue(where, value);
            ::fprintf(where, "\n");
        }
    };
    auto dumpPad = [&](Pad &pad, int lv) {
        ::fprintf(where, "%*s Pad: '%s'\n", lv, "", pad.name().data());
        ::fprintf(where, "%*s isLinked: %s\n", lv + 2, "", pad.isLinked() ? "true" : "false");
        dumpCaps(pad.caps(), lv + 2);
    };


    ::fprintf(where, "%*sElement: '%s', State: %s\n", level, "", mName.c_str(), toString(mState).data());
    if (!mInputs.empty()) {
        ::fprintf(where, "%*s  Input Pads:\n", level, "");
        for (auto &pad : mInputs) {
            dumpPad(pad, level + 2);
        }
    }
    if (!mOutputs.empty()) {
        ::fprintf(where, "%*s  Output Pads:\n", level, "");
        for (auto &pad : mOutputs) {
            dumpPad(pad, level + 2);
        }
    }
}

// MARK: Bin
Bin::Bin(std::string_view name) : Element(name) {

}

Bin::~Bin() {

}

// Emm? maybe we should make setState to virtual ?
auto Bin::addElement(Element::Ptr element) -> void {
    if (!element) {
        return;
    }
    element->mParent = this;
    mChildren.emplace_back(std::move(element));
    mSorted = false;
}

auto Bin::addElementSync(Element::Ptr element) -> IoTask<void> {
    if (!element) {
        co_return Err(Error::InvalidArguments);
    }
    element->mParent = this;
    mChildren.emplace_back(element);
    // Async state here
    if (auto res = co_await element->setState(state()); !res) {
        element->mParent = nullptr;
        mChildren.pop_back();
        co_return Err(res.error());
    }
    mSorted = false;
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
    (*it)->mParent = nullptr;
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
                logger::warn("[Bin] '{}' child '{}' failed to set state to '{}', error: {}", name(), child->name(), toString(newState), res.error().message());
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

    // Clock field
    std::chrono::steady_clock::time_point clockEpoch {};
    std::chrono::nanoseconds              clockTime {};
    bool                                  clockPaused = true;

    // Debug field
    ilias::WaitHandle<void>               clockMonitor {};
};

Pipeline::Pipeline(std::string_view name) : Bin(name), d(std::make_unique<Impl>()){
    mIsPipeline = true;
}

Pipeline::~Pipeline() {

}

auto Pipeline::onRun() -> IoTask<void> {
    if (!mSorted) { // Topological changed, the clock may change
        mClocks.clear();
    }
    if (mClocks.empty()) {
        // Get all children who provide clock
        for (auto &child : mChildren) {
            if (auto res = child->sendQuery(Query::ClockSource{}); res) {
                auto [clock] = res->toClockSource();
                assert(clock);
                mClocks.emplace_back(std::move(clock));
            }
        }
        // Add self's clock, using alias
        auto self = Clock::Ptr {shared_from_this(), d.get()};
        mClocks.emplace_back(std::move(self));

        // Sort it by category
        std::ranges::sort(mClocks, [](auto &lhs, auto &rhs) { return toUnderlying(lhs->category()) < toUnderlying(rhs->category()); });
    }

    // Ok, start the bin
    if (auto res = co_await Bin::onRun(); !res) {
        co_return Err(res.error());
    }

    // Update the self clock
    auto time = d->clockTime;
    d->clockEpoch = std::chrono::steady_clock::now() - time;
    d->clockPaused = false;
    d->clockMonitor = ilias::spawn([this]() -> Task<void> {
        auto time = mClocks.front()->time();
        while (true) {
            // Sleep 1s
            co_await ilias::sleep(std::chrono::seconds(1));
            auto now = mClocks.front()->time();
            if ((now - time) > std::chrono::seconds(1)) {
                auto s = std::chrono::duration_cast<std::chrono::seconds>(now);
                logger::info("[Pipeline] '{}' clock update to {}", name(), s);
            }
        }
    });
    logger::info("[Pipeline] '{}' started, master clock: {}, num clocks source: {}", name(), mClocks.front()->time(), mClocks.size());
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
    d->clockMonitor.stop();
    co_await std::exchange(d->clockMonitor, {});
    logger::info("[Pipeline] '{}' paused", name());
    co_return co_await Bin::onPause();
}

auto Pipeline::onStop() -> IoTask<void> {
    d->clockTime = {};
    d->clockEpoch = {};
    d->clockPaused = true;
    mClocks.clear();
    logger::info("[Pipeline] '{}' stopped", name());
    co_return co_await Bin::onStop();
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
