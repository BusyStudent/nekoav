#define _NEKO_SOURCE
#include "../threading.hpp"
#include "../eventsink.hpp"
#include "../event.hpp"
#include "../pad.hpp"
#include "base.hpp"

NEKO_NS_BEGIN

namespace _abiv1 {

class ElementBasePrivate {
public:

};

ElementBase::ElementBase(ElementDelegate *delegate, bool threading) 
    : d(new ElementBasePrivate), mDelegate(delegate), mElement(dynamic_cast<Element*>(delegate)), mThreading(threading) 
{
    NEKO_ASSERT(mElement);
}
ElementBase::~ElementBase() {
    // MUST Stopped
    NEKO_ASSERT(mElement->state() == State::Null);
}

// API Here
Error ElementBase::sendEventToUpstream(View<Event> event) {
    for (const auto v : mElement->inputs()) {
        auto err = v->pushEvent(event);
        if (err != Error::Ok) {
            // Handle Error
        }
    }
    return Error::Ok;
}
Error ElementBase::sendEventToDownstream(View<Event> event) {
    for (const auto v : mElement->outputs()) {
        auto err = v->pushEvent(event);
        if (err != Error::Ok) {
            // Handle Error
        }
    }
    return Error::Ok;
}
Error ElementBase::raiseError(Error errcode, std::string_view message, std::source_location loc) {
    if (mElement->bus()) {
        auto event = ErrorEvent::make(errcode, message, mElement);
        mElement->bus()->sendEvent(event);
    }
    return errcode;
}
Error ElementBase::_sendEvent(View<Event> event) {
    if (mThreading) {
        if (!mThread) {
            return Error::InvalidState;
        }
        if (Thread::currentThread() != mThread) {
            // syncInvoke
            return syncInvoke(&ElementBase::_sendEvent, this, event);
        }
    }
    return mDelegate->onEvent(event);
}
Error ElementBase::_changeState(StateChange stateChange) {
    if (!mThreading) {
        return _dispatchChangeState(stateChange);
    }

    // Threading code here
    if (stateChange == StateChange::Initialize) {
        // Initalize and create a thread
        NEKO_ASSERT(!mThread);

        // Set It to target state, not Null
        mElement->overrideState(GetTargetState(stateChange));
        mThread = new Thread();
    }
    auto err = syncInvoke(&ElementBase::_dispatchChangeState, this, stateChange);
    if (stateChange == StateChange::Initialize && err == Error::Ok) {
        // Init OK, post a loop
        mThread->postTask(std::bind(&ElementBase::_run, this));
    }
    // TODO : Finished
    if (stateChange == StateChange::Teardown || (stateChange == StateChange::Initialize && err != Error::Ok)) {
        // Teardown or Init failed
        NEKO_ASSERT(mThread != nullptr);
        // Set It to target state to Null to stop
        mElement->overrideState(State::Null);
        delete mThread;
        mThread = nullptr;
    }

    return Error::Ok;
}
Error ElementBase::_dispatchChangeState(StateChange stateChange) {
    switch (stateChange) {
        case StateChange::Initialize : return mDelegate->onInitialize();
        case StateChange::Prepare    : return mDelegate->onPrepare();
        case StateChange::Run        : return mDelegate->onRun();
        case StateChange::Pause      : return mDelegate->onPause();
        case StateChange::Stop       : return mDelegate->onStop();
        case StateChange::Teardown   : return mDelegate->onTeardown();
        default                      : return Error::InvalidArguments;
    }
}
void ElementBase::_invoke(std::function<void()> &&cb) {
    NEKO_ASSERT(mThread);

    std::latch latch {1};
    mThread->sendTask([&]() {
        cb();
        latch.count_down();
    });
    latch.wait();
}
void ElementBase::_run() {
#ifndef NDEBUG
    mThread->setName(mElement->name());
#endif

    auto delegate = static_cast<ThreadingElementDelegate*>(mDelegate);
    auto err = delegate->onLoop();

    if (err == Error::NoImpl) {
        // Default impl
        while (!stopRequested()) {
            mThread->waitTask();
        }
    }
    else if (err != Error::Ok) {
        raiseError(err);
    }
}
Pad *ElementBase::_polishPad(Pad *pad) {
    if (!pad) {
        return pad;
    }
    if (pad->type() == Pad::Input) {
        // Input Pad
        // pad->setCallback(std::bind(ElementBase::_onSinkPushed, this, std::ref(*pad), std::placeholders::_2));
        pad->setCallback([this, pad](View<Resource> resource) -> Error {
            return _onSinkPushed(pad, resource);
        });
        pad->setEventCallback([this, pad](View<Event> event) -> Error {
            // return 
            return _onSinkEvent(pad, event);
        });
    }
    else {
        pad->setEventCallback([this, pad](View<Event> event) -> Error {
            // return 
            return Error::NoImpl;
        });
    }
    return pad;
}
Error ElementBase::_onSinkPushed(Pad *pad, View<Resource> resource) {
    return mDelegate->onSinkPushed(pad, resource);
}
Error ElementBase::_onSinkEvent(Pad *pad, View<Event> event) {
    auto err = mDelegate->onSinkEvent(pad, event);
    if (err == Error::NoImpl) {
        // Not impl, using default impl
        return sendEventToDownstream(event);
    }
    return err;
}

}
NEKO_NS_END