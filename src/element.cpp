#include <nekoav/element.hpp>
#include <nekoav/error.hpp>

namespace nekoav {

// State calc
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
        throw std::runtime_error("Invalid State");
    }
} // clac

// Pad
auto Pad::push(Sample::Ptr sample) -> IoTask<void> {
    if (!isLinked()) {
        co_return Err(Error::NoLink);
    }
    if (!mPeer->mCallback) {
        co_return Err(Error::NoPushCallback);
    }
    co_return co_await mPeer->mCallback(*mPeer, std::move(sample));
}

// Element
Element::Element(std::string_view name) : mName(name) {
    if (mName.empty()) {
        mName = "#Element " + std::to_string(std::bit_cast<uintptr_t>(this));
    }
}

Element::~Element() {
    // We could only destroy the Element when it is in null
    if (mState != State::Null) {
        ::fprintf(stderr, "Invalid state on element, element could only be destroyed when it state is null");
        ::abort();
    }
}

auto Element::setState(State targetState) -> IoTask<void> {
    if (targetState == State::Error) {
        co_return Err(Error::InvalidState);
    }

    if (targetState == mState) { // Same state, no-op
        co_return {};
    }
    
    // Check is error state
    if (mState == State::Error) {
        if (targetState != State::Null) { // The only state allow to set to is Null
            co_return Err(Error::InvalidState);
        }
        if (auto res = co_await onTeardown(); !res) {
            co_return Err(res.error());
        }
        mState = State::Null;
        co_return {};
    }

    // Do transations
    // Check is forward (Null -> Running)
    // Backward is (Running -> NUll)
    auto isForward = [&]() {
        return int(targetState) > int(mState);
    };
    auto nextState = [&](State state) {
        if (isForward()) {
            return State(int(state) + 1);
        }
        else {
            return State(int(state) - 1);
        }
    };

    // DO transation
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
        if (auto res = co_await std::move(task); !res) { // FAILED!!!
            mState = State::Error;
            co_return Err(res.error());
        }
    }
    // Done
    mState = targetState;
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
    return mInputs.emplace_back(*this, PadType::Input);
}

auto Element::createOutputPad(std::string_view name) -> Pad & {
    return mOutputs.emplace_back(*this, PadType::Output);
}

} // namespace nekoav