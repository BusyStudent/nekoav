#pragma once

#include "../pad.hpp"
#include "../defs.hpp"
#include "../libc.hpp"
#include "../event.hpp"
#include "../eventsink.hpp"
#include "../elements.hpp"
#include "../threading.hpp"
#include "../backtrace.hpp"

NEKO_NS_BEGIN


// Legacy API Here
namespace [[deprecate("Use new version api in base.hpp")]] Template {

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
            err = onEvent(ev);
        });
        return err;
    }
    Error changeState(StateChange change) override {
        if (change == StateChange::Initialize) {
            NEKO_ASSERT(mThread == nullptr);
            mThread = new Thread();
            mRunning = true;
        }
        Error ret;
        mThread->sendTask([&, this]() {
            ret = StateDispatch::onChangeState(change);
            if (ret == Error::Ok) {
                this->overrideState(GetTargetState(change));
            }
        });
        if (change == StateChange::Initialize) {
            // Start thread main
            mThread->postTask(std::bind(&GetThreadImpl::_threadEntry, this));
        }
        if (change == StateChange::Teardown) {
            NEKO_ASSERT(mThread != nullptr);
            mRunning = false;
            delete mThread;
            mThread = nullptr;
        }
        return ret;
    }

    // Event here
    virtual Error onEvent(View<Event> event) {
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
    /**
     * @brief Send a error event to bus
     * 
     * @param errcode The Error code
     * @param message The message, default is empty
     * 
     */
    Error raiseError(Error errcode, std::string_view message = { }) {
#ifndef     NDEBUG
        Backtrace();
#endif

        auto event = ErrorEvent::make(errcode, message, this);
        this->bus()->postEvent(event);
        return errcode;
    }
private:
    void  _threadEntry() {
#ifndef     NDEBUG
        mThread->setName(this->name().c_str());
#endif
        auto err = onLoop();
        if (err != Error::Ok && this->bus()) {
            raiseError(err);
        }
    }

    Thread      *mThread = nullptr;
    Atomic<bool> mRunning {false};
};

template <typename ...Ts>
class GetImpl : public Ts..., protected StateDispatch {
public:
    Error sendEvent(View<Event> ev) override {
        return onEvent(ev);
    }
    Error changeState(StateChange change) override {
        return StateDispatch::onChangeState(change);
    }
    virtual Error onEvent(View<Event> event) {
        return Error::Ok;
    }
    Error raiseError(Error errcode, std::string_view message = { }) {
#ifndef     NDEBUG
        Backtrace();
#endif
        auto event = ErrorEvent::make(errcode, message, this);
        this->bus()->postEvent(event);
        return errcode;
    }
};
}

NEKO_NS_END