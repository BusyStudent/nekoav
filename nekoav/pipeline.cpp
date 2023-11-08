#define _NEKO_SOURCE
#include "pipeline.hpp"
#include "eventsink.hpp"
#include "threading.hpp"
#include "factory.hpp"
#include "event.hpp"
#include <vector>

NEKO_NS_BEGIN

class PipelineImpl final : public Pipeline {
public:
    Error addElement(View<Element> element) override {
        if (!element) {
            return Error::InvalidArguments;
        }
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
        mElements.erase(it);
        return Error::Ok;
    }
    Error forElement(const std::function<bool (View<Element>)> &cb) override {
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
        for (const auto &elem : mElements) {
            if (auto err = elem->setState(GetTargetState(stateChange)); err != Error::Ok) {
                return err;
            }
        }
        return Error::Ok;
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
        mThread->sendTask(std::bind(&PipelineImpl::processEvent, this, event->shared_from_this()));
        return Error::Ok;
    }
    Error processEvent(const Arc<Event> &event) {
        // Process event here
        if (event->type() == Event::ErrorOccurred) {
            // Error 
            // Do Error here
        }
        if (mEventCallback) {
            mEventCallback(event);
        }
        new PipelineImpl;
        return Error::Ok;
    }

    Thread            *mThread;
    Vec<Arc<Element> > mElements;

    std::function<void(View<Event>)> mEventCallback;
};

Box<Pipeline> CreatePipeline() {
    return std::make_unique<PipelineImpl>();
}

NEKO_REGISTER_ELEMENT(Pipeline, PipelineImpl);

NEKO_NS_END