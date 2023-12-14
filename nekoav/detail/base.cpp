#define _NEKO_SOURCE
#include "../threading.hpp"
#include "../eventsink.hpp"
#include "../context.hpp"
#include "../event.hpp"
#include "../pad.hpp"
#include "tracer.hpp"
#include "base.hpp"

NEKO_NS_BEGIN

namespace _abiv1 {

/**
 * @note Please do not use it in destructors, although in most cases this won't be a problem, and relying on implicit destructors can have unforeseen consequences.
 * 
 */
#define TRACE(what, ...)                                                                        \
    if (d->mTracer) {                                                                           \
        d->mTracer->received(mElement, ElementEventType::StageBegin,  what, ##__VA_ARGS__);     \
    }                                                                                           \
    DeferInvoke _inv = [this, tracer = d->mTracer]() {                                          \
        if (tracer) {                                                                           \
            tracer->received(mElement, ElementEventType::StageEnd,  what, ##__VA_ARGS__);       \
        }                                                                                       \
    };

template <typename Callable>
class DeferInvoke {
public:
    DeferInvoke(Callable &&callable) : mCallable(std::move(callable)) {}
    ~DeferInvoke() { mCallable(); }
private:
    Callable mCallable;
};

class ElementBasePrivate {
public:
    ElementEventSubscriber *mTracer = nullptr;
};

ElementBase::ElementBase(ElementDelegate *delegate, Element *element, int kind) 
    : d(new ElementBasePrivate), mDelegate(delegate), mElement(element)
{
    NEKO_ASSERT(mElement);
    switch (kind) {
        case NoThreading: mThreading = false; break;
        case Threading: mThreading = true; break;
        case ThreadingEx: mThreading = true; mThreadingEx = true; break;
        default: NEKO_ASSERT(false); break;
    }
}
ElementBase::~ElementBase() {
    // MUST Stopped
    NEKO_ASSERT(mElement->state() == State::Null);
}

// API Here
Error ElementBase::pushEventToUpstream(View<Event> event) {
    for (const auto v : mElement->inputs()) {
        // auto err = v->pushEvent(event);
        auto err = pushEventTo(v, event);
        if (err != Error::Ok) {
            // Handle Error
        }
    }
    return Error::Ok;
}
Error ElementBase::pushEventToDownstream(View<Event> event) {
    for (const auto v : mElement->outputs()) {
        // auto err = v->pushEvent(event);
        auto err = pushEventTo(v, event);
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
    TRACE("sendEvent");

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
    // Init Tracer here
    if (stateChange == StateChange::Initialize) {
        d->mTracer = mElement->context()->queryObject<ElementEventSubscriber>();
    }
    // Begin Trace
    TRACE("changeState");
    
    if (!mThreading) {
        return _dispatchChangeState(stateChange);
    }

    // Threading code here
    if (stateChange == StateChange::Initialize) {
        // Initalize and create a thread
        NEKO_ASSERT(!mThread);

        // Set It to target state, not Null
        mElement->overrideState(GetTargetState(stateChange));

        // Alloc if customize
        if (mThreadingEx) {
            auto alloc = static_cast<ThreadingElementDelegateEx*>(mDelegate);
            mThread = alloc->allocThread();
        }
        else {
            mThread = new Thread();
        }
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

        if (mThreadingEx) {
            auto alloc = static_cast<ThreadingElementDelegateEx*>(mDelegate);
            alloc->freeThread(mThread);
        }
        else {
            delete mThread; //< Wait for all task done
        }

        mThread = nullptr;
        d->mTracer = nullptr;
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

    // std::latch latch {1};
    // mThread->postTask([&]() {
    //     cb();
    //     latch.count_down();
    // });
    // latch.wait();
    mThread->sendTask(std::move(cb));
}
void ElementBase::_run() {
#ifndef NDEBUG
    mThread->setName(mElement->name());
#endif

    auto delegate = static_cast<ThreadingElementDelegate*>(mDelegate);
    auto err = delegate->onLoop();

    if (err == Error::NoImpl) {
        // // Default impl
        // while (!stopRequested()) {
        //     mThread->waitTask();
        // }
        return; //< Continue loop at Thread::_run
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
            return _onSinkPush(pad, resource);
        });
        pad->setEventCallback([this, pad](View<Event> event) -> Error {
            // return 
            return _onSinkEvent(pad, event);
        });
    }
    else {
        pad->setEventCallback([this, pad](View<Event> event) -> Error {
            // return 
            return _onSourceEvent(pad, event);
        });
    }
    return pad;
}
Error ElementBase::_onSinkPush(Pad *pad, View<Resource> resource) {
    TRACE("onSinkPush");

    return mDelegate->onSinkPush(pad, resource);
}
Error ElementBase::_onSinkEvent(Pad *pad, View<Event> event) {
    TRACE("onSinkEvent");

    auto err = mDelegate->onSinkEvent(pad, event);
    if (err == Error::NoImpl) {
        // Not impl, using default impl
        return pushEventToDownstream(event);
    }
    return err;
}
Error ElementBase::_onSourceEvent(Pad *pad, View<Event> event) {
    TRACE("onSourceEvent");

    auto err = mDelegate->onSourceEvent(pad, event);
    if (err == Error::NoImpl) {
        // Not impl, using default impl
        return pushEventToUpstream(event);
    }
    return err;
}
Error ElementBase::pushEventTo(View<Pad> pad, View<Event> event) {
    TRACE("pushEventTo");

    if (event && pad) {
        return pad->pushEvent(event);
    }
    return Error::InvalidArguments;
}
Error ElementBase::pushTo(View<Pad> pad, View<Resource> resource) {
    TRACE("pushTo");

    if (pad && resource) {
        return pad->push(resource);
    }
    return Error::InvalidArguments;
}
bool ElementBase::isWorkThread() const noexcept {
    NEKO_ASSERT(mThread);
    return Thread::currentThread() == mThread;
}

#undef TRACE

}
NEKO_NS_END