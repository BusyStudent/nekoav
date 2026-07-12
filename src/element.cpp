#include <nekoav/elements/pipeline.hpp>
#include <nekoav/elements/bin.hpp>
#include <nekoav/element.hpp>
#include <nekoav/error.hpp>
#include "log.hpp"
#include <ilias/task.hpp>
#include <unordered_map>
#include <algorithm>
#include <ranges>
#include <queue>
#include <print>

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
    NEKOAV_ERROR("Invalid state transition from {} to {}", cur, target);
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
        mElement.mParent->onTopologyChange();
    }
    if (mElement.mState == State::Running) {
        NEKOAV_ERROR("[Pad] Topology changed while element '{}' is running, this may cause undefined behavior", mElement.name());
        std::abort();
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
        mElement.mParent->onTopologyChange();
    }
    if (mElement.mState == State::Running) {
        NEKOAV_ERROR("[Pad] Topology changed while element '{}' is running, this may cause undefined behavior", mElement.name());
        std::abort();
    }
    return true;
}

auto Pad::push(Sample sample) -> IoTask<void> {
    if (!isLinked()) {
        co_return Err(Error::NotLinked);
    }
    if (!mPeer->mPushCallback) {
        NEKOAV_DEBUG("No push callback set on pad '{}'", mPeer->name());
        co_return Err(Error::NoPushCallback);
    }
    co_return co_await mPeer->mPushCallback(*mPeer, std::move(sample));
}

auto Pad::pushEvent(Event event) -> IoTask<void> {
    auto cur = peer();
    if (!cur) {
        co_return Err(Error::NotLinked);
    }
    auto &element = cur->mElement;
    NEKOAV_INFO("[Pad] push event '{}' to element '{}', pad '{}'", event, element.name(), cur->name());
    if (cur->mEventCallback) { 
        if (auto res = co_await cur->mEventCallback(*cur, event); !res) {
            NEKOAV_ERROR("Failed to push event to pad '{}': {}", cur->name(), res.error().message());
            co_return Err(res.error());
        }
        co_return {};
    }
    else { // fallback to the default implementation
        co_return co_await element.forwardEvent(*cur, event);
    }
}

auto Pad::sendQuery(Query query) -> std::optional<Reply> {
    auto walkToUp = mType == PadType::Input; // If self is input pad, we walk upstream
    auto cur = peer();
    if (cur) {
        auto &element = cur->mElement;
        NEKOAV_INFO("[Pad] send query '{}' to element '{}', pad '{}'", query, element.name(), cur->name());
        if (cur->mQueryCallback) {
            auto res = cur->mQueryCallback(*cur, query);
            if (res) {
                NEKOAV_INFO("[Pad] query '{}' is handled by pad '{}' => {}", query, cur->name(), *res);
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
Element::Element(ElementType type, std::string_view name) : mType(type), mName(name) {
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
    auto isForward = std::to_underlying(targetState) > std::to_underlying(mState);
    auto nextState = [&](State state) {
        auto value = std::to_underlying(state);
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
        NEKOAV_DEBUG("[Element] '{}' Change state from '{}' to '{}'", mName, cur, nextState(cur));
        if (auto res = co_await std::move(task); !res && isForward) { // FORWARD, FAILED!!!
            mError = res.error();
            co_return Err(res.error());
        }
        else if (!res) { // BACKWARD, FAILED!!!
            mError = res.error();
            NEKOAV_WARN("[Element] '{}' Failed to backward to state '{}': {}, ignore it", mName, nextState(cur), res.error().message());
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

auto Element::sendEvent(Event event) -> IoTask<void> {
    co_return {};
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
    NEKOAV_ERROR("[Element] set error state: {}", errc.message());
    mError = errc;
    postMessage(Message::Error {
        .error = errc
    });
}

auto Element::postMessage(Message message) -> bool {
    if (!mParent) {
        NEKOAV_ERROR("[Element] '{}' Failed to post message to parent, no parent exists", name());
        return false;
    }
    mParent->onChildMessage(std::move(message));
    return true;
}

auto Element::forwardEvent(Pad &pad, Event event) -> IoTask<void> {
    auto walkToUp = pad.type() == PadType::Output; // If we come from the output pad, we walk upstream
    // Continue walk to find an handler
    if (walkToUp) {
        for (auto &pad : inputs()) {
            if (auto res = co_await pad.pushEvent(event); !res) {
                co_return Err(res.error());
            }
        }
    }
    else {
        for (auto &pad : outputs()) {
            if (auto res = co_await pad.pushEvent(event); !res) {
                co_return Err(res.error());
            }
        }
    }
    co_return {};
}

// MARK: Set Context
auto Element::setContext(Context::Ptr ctxt) -> void {
    mContext = ctxt;
    if (this->isBin()) {
        auto self = static_cast<Bin *>(this);
        for (auto &child : self->mChildren) {
            child->setContext(ctxt);
        }
    }
}

// MARK: Set Clock
auto Element::setClock(Clock::Ptr clock) -> void {
    mClock = clock;
    if (this->isBin()) {
        for (auto &child : static_cast<Bin *>(this)->mChildren) {
            child->setClock(clock);
        }
    }
}

auto Element::dumpInfoInternal(FILE *where, int level) -> void {
    auto dumpCaps = [&](const Caps &caps, int lv) {
        if (caps.empty()) return;
        for (const auto &[name, value] : caps) {
            if (value.isNull()) {
                std::println(where, "{:{}}• {}", "", lv, name);
            }
            else {
                std::println(where, "{:{}}• {}: {}", "", lv, name, value);
            }
        }
    };

    auto dumpPad = [&](Pad &pad, int lv, bool isInput) {
        std::string_view arrow = isInput ? "<-" : "->";
        std::string_view linkState = pad.isLinked() ? "[Linked]" : "[Unlinked]";
        
        std::println(where, "{:{}}{} '{}' {}", "", lv, arrow, pad.name(), linkState);
        dumpCaps(pad.caps(), lv + 3); 
    };

    // Element ： [State] Name
    //   Clock
    //   Context
    std::println(where, "{:{}}[{}] {}", "", level, mState, mName);
    std::println(where, "{:{}}Clock: {}", "", level + 2, static_cast<const void*>(mClock.get()));
    std::println(where, "{:{}}Context: {}", "", level + 2, static_cast<const void*>(mContext.get()));

    //   Caps
    if (!mInputs.empty()) {
        std::println(where, "{:{}}Inputs:", "", level + 2);
        for (auto &pad : mInputs) {
            dumpPad(pad, level + 4, true);
        }
    }

    if (!mOutputs.empty()) {
        std::println(where, "{:{}}Outputs:", "", level + 2);
        for (auto &pad : mOutputs) {
            dumpPad(pad, level + 4, false);
        }
    }
}

// MARK: Sink, Transform, Source dtor
Sink::~Sink() {}
Source::~Source() {}
Transform::~Transform() {}

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

auto toString(ElementType type) -> std::string_view {
    switch (type) {
        case ElementType::Sink:      return "Sink";
        case ElementType::Source:    return "Source";
        case ElementType::Transform: return "Transform";
        case ElementType::Other:     return "Other";
        default:                     return "Unknown"; // Impossible!
    }
}

} // namespace nekoav
