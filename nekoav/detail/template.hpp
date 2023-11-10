#pragma once

#include "../defs.hpp"
#include "../event.hpp"
#include "../eventsink.hpp"
#include "../elements.hpp"
#include "../threading.hpp"

NEKO_NS_BEGIN

namespace Template {

class StateDispatch {
public:
    virtual Error onInitialize() { return Error::Ok; }
    virtual Error onPrepare(){ return Error::Ok; }
    virtual Error onRun() { return Error::Ok; }
    virtual Error onPause() { return Error::Ok; }
    virtual Error onStop() { return Error::Ok; }
    virtual Error onTeardown() { return Error::Ok; }

    Error onChangeState(StateChange change) {
        switch (change) {
            case StateChange::Initialize : return onInitialize();
            case StateChange::Prepare    : return onPrepare();
            case StateChange::Run        : return onRun();
            case StateChange::Pause      : return onPause();
            case StateChange::Stop       : return onStop();
            case StateChange::Teardown   : return onTeardown();
            default                      : return Error::InvalidArguments;
        }
    }
};

/**
 * @brief Template for Impl Elements has thread
 * 
 * @tparam Ts 
 */
template <typename ...Ts>
class GetThreadImpl : public Ts..., protected StateDispatch {
public:
    Error sendEvent(View<Event> ev) override {
        if (!mThread) {
            return Error::InvalidState;
        }
        Error err;
        mThread->sendTask([&, this]() {
            err = event(ev);
        });
        return err;
    }
    Error changeState(StateChange change) override {
        if (change == StateChange::Initialize) {
            assert(mThread == nullptr);
            mThread = new Thread();
            mRunning = true;
        }
        Error ret;
        mThread->sendTask([&, this]() {
            ret = StateDispatch::onChangeState(change);
        });
        if (change == StateChange::Initialize) {
            // Start thread main
            mThread->postTask(std::bind(&GetThreadImpl::_threadEntry, this));
        }
        if (change == StateChange::Teardown) {
            assert(mThread != nullptr);
            mRunning = false;
            delete mThread;
            mThread = nullptr;
        }
        return ret;
    }

    // Event here
    virtual Error event(View<Event> event) {
        return Error::Ok;
    }
    virtual Error onLoop() {
        while (!stopRequested()) {
            // While not stop
            mThread->waitTask();
        }
        return Error::Ok;
    }

    Thread *thread() const noexcept {
        return mThread;
    }
    bool   stopRequested() const noexcept {
        return !mRunning;
    }
private:
    void  _threadEntry() {
#ifndef     NDEBUG
        mThread->setName(typeid(*this).name());
#endif
        auto err = onLoop();
        if (err != Error::Ok && this->bus()) {
            auto event = ErrorEvent::make(err, this);
            this->bus()->postEvent(event);
        }
    }

    Thread      *mThread = nullptr;
    Atomic<bool> mRunning {false};
};

template <typename ...Ts>
class GetImpl : public Ts..., protected StateDispatch {
public:
    Error sendEvent(View<Event> ev) override {
        return event(ev);
    }
    Error changeState(StateChange change) override {
        return StateDispatch::onChangeState(change);
    }
    virtual Error event(View<Event> event) {
        return Error::Ok;
    }
};

}

NEKO_NS_END