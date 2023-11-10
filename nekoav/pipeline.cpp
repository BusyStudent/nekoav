#define _NEKO_SOURCE
#include "pipeline.hpp"
#include "eventsink.hpp"
#include "threading.hpp"
#include "context.hpp"
#include "factory.hpp"
#include "media.hpp"
#include "event.hpp"
#include "log.hpp"
#include <vector>

NEKO_NS_BEGIN

class PipelineImpl final : public Pipeline {
public:
    PipelineImpl() {
        // Init env
        setBus(&mBusSink);
        setContext(&mContext);
    }
    ~PipelineImpl() {
        setState(State::Null);
        delete mThread;
    }
    Error addElement(View<Element> element) override {
        if (!element) {
            return Error::InvalidArguments;
        }
        element->setBus(&mBusSink);
        element->setContext(&mContext);
        mElements.push_back(element->shared_from_this());
        return Error::Ok;
    }
    Error detachElement(View<Element> element) override {
        if (!element) {
            return Error::InvalidArguments;
        }
        auto it = std::find_if(mElements.begin(), mElements.end(), [&](auto &&v) {
            return v.get() == element.get();
        });
        if (it == mElements.end()) {
            return Error::InvalidArguments;
        }
        element->setBus(nullptr);
        element->setContext(nullptr);
        mElements.erase(it);
        return Error::Ok;
    }
    Error forElements(const std::function<bool (View<Element>)> &cb) override {
        if (!cb) {
            return Error::InvalidArguments;
        }
        for (const auto &element : mElements) {
            if (!cb(element)) {
                break;
            }
        }
        return Error::Ok;
    }
    Error changeState(StateChange stateChange) override {
        if (stateChange == StateChange::Initialize) {
            // Initalize, new a thread
            assert(!mThread && "Already initialized");
            mRunning = true;
            mThread = new Thread;
            mThread->postTask(std::bind(&PipelineImpl::threadEntry, this));
        }
        Error ret = Error::Ok;
        auto cb = [&, this]() {
            for (const auto &elem : mElements) {
                ret = elem->setState(GetTargetState(stateChange));
                if (ret != Error::Ok) {
                    NEKO_LOG("Failed to set state for {} : {}", elem->name(), ret);
                    return;
                }
            }
        };
        if (Thread::currentThread() == mThread) {
            cb();
        }
        else {
            mThread->sendTask(cb);
        }
        if (stateChange == StateChange::Teardown || (stateChange == StateChange::Initialize && ret != Error::Ok)) {
            // Teardown or Init failed
            assert(mThread && "Not initialized");
            mRunning = false;
            delete mThread;
            mThread = nullptr;
        }
        return ret;
    }
    Error sendEvent(View<Event> event) override {
        for (const auto &elem : mElements) {
            elem->sendEvent(event);
        }
        return Error::Ok;
    }
    void  setEventCallback(std::function<void(View<Event> )> &&cb) override {
        mEventCallback = std::move(cb);
    }
private:
    struct Sink final : public EventSink {
        Sink(PipelineImpl* p) : p(p) {}

        Error postEvent(View<Event> event) override {
            return p->postBusEvent(event);
        }
        Error sendEvent(View<Event> event) override {
            return p->sendBusEvent(event);
        }

        PipelineImpl *p;
    } mBusSink {this};

    Error postBusEvent(View<Event> event) {
        mThread->postTask(std::bind(&PipelineImpl::processEvent, this, event->shared_from_this()));
        return Error::Ok;
    }
    Error sendBusEvent(View<Event> event) {
        if (Thread::currentThread() != mThread) {
            mThread->sendTask(std::bind(&PipelineImpl::processEvent, this, event->shared_from_this()));
        }
        else {
            mThread->postTask(std::bind(&PipelineImpl::processEvent, this, event->shared_from_this()));
        }
        return Error::Ok;
    }
    Error processEvent(const Arc<Event> &event) {
        // Process event here
        NEKO_DEBUG(typeid(*event));
        NEKO_DEBUG(event->type());
        
        if (event->type() == Event::ErrorOccurred) {
            // Error 
            // Do Error here
        }
        if (mEventCallback) {
            mEventCallback(event);
        }
        return Error::Ok;
    }
    void threadEntry() {
        mThread->setName("NekoPipeline");
        NEKO_DEBUG("Pipeline Thread Started");
        while (mRunning) {
            mThread->waitTask();
            NEKO_DEBUG("Pipeline Thread Dispatch once");
        }
    }

    Thread            *mThread = nullptr;
    Vec<Arc<Element> > mElements;
    Atomic<bool>       mRunning {false};
    Context            mContext;

    std::function<void(View<Event>)> mEventCallback;
};

Arc<Pipeline> CreatePipeline() {
    return make_shared<PipelineImpl>();
}

NEKO_REGISTER_ELEMENT(Pipeline, PipelineImpl);

NEKO_NS_END