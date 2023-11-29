#define _NEKO_SOURCE
#include "pipeline.hpp"
#include "eventsink.hpp"
#include "threading.hpp"
#include "context.hpp"
#include "factory.hpp"
#include "media.hpp"
#include "event.hpp"
#include "log.hpp"
#include <shared_mutex>
#include <vector>

NEKO_NS_BEGIN

class PipelineImpl final : public Pipeline, public MediaController {
public:
    PipelineImpl() {
        // Init env
        setBus(&mBusSink);
        setContext(&mContext);
        addClock(&mExternalClock);

        mContext.addObjectView<MediaController>(this);
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
            NEKO_ASSERT(!mThread && "Already initialized");
            mRunning = true;
            mThread = new Thread(&PipelineImpl::threadEntry, this);

            // Init media controller here
            mPosition = 0.0;
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
            overrideState(GetTargetState(stateChange));

            // Handle clock here
            if (stateChange == StateChange::Run) {
                mExternalClock.start();
            }
            if (stateChange == StateChange::Pause) {
                mExternalClock.pause();
                if (masterClock() != &mExternalClock) {
                    mExternalClock.setPosition(masterClock()->position());
                }
            }
            if (stateChange == StateChange::Stop) {
                mExternalClock.setPosition(0);
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
            NEKO_ASSERT(mThread && "Not initialized");
            mRunning = false;

            if (Thread::currentThread() != mThread) {
                //< Call setState at user thread
                delete mThread;
                mThread = nullptr;
            }
        }
        return ret;
    }
    Error sendEvent(View<Event> event) override {
        for (const auto &elem : mElements) {
            // FIXME : If seek failed???
            if (event->type() == Event::SeekRequested) {
                mExternalClock.setPosition(event.viewAs<SeekEvent>()->time());
            }
            NEKO_TRACE_TIME(duration) {
                elem->sendEvent(event);
            }
            NEKO_LOG("Send event {} to {}, costed {} ms", event->type(), elem->name(), duration);
        }
        return Error::Ok;
    }
    void  setEventCallback(std::function<void(View<Event> )> &&cb) override {
        mEventCallback = std::move(cb);
    }
    size_t size() override {
        return mElements.size();
    }

    // MediaController
    void addClock(MediaClock *clock) override {
        if (!clock) {
            return;
        }
        std::lock_guard locker(mClockMutex);
        mClocks.push_back(clock);

        if (mMasterClock == nullptr) {
            mMasterClock = clock;
        }
        else if (clock->type() > mMasterClock->type()) {
            // Higher
            mMasterClock = clock;
        }
    }
    void removeClock(MediaClock *clock) override {
        if (!clock) {
            return;
        }
        std::lock_guard locker(mClockMutex);
        mClocks.erase(std::remove(mClocks.begin(), mClocks.end(), clock), mClocks.end());
        if (mMasterClock == clock) {
            mMasterClock = nullptr;
            // Find next none
            for (auto it = mClocks.begin(); it != mClocks.end(); it++) {
                if (mMasterClock == nullptr) {
                    mMasterClock = *it;
                    continue;
                }
                if ((*it)->type() > mMasterClock->type()) {
                    mMasterClock = *it;
                }
            }
        }
    }
    MediaClock *masterClock() const override {
        std::shared_lock locker(mClockMutex);
        return mMasterClock;
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
        NEKO_DEBUG(typeid(*event->sender()));
        
        if (event->type() == Event::ErrorOccurred) {
            // Error 
            NEKO_DEBUG(View<Event>(event).viewAs<ErrorEvent>()->error());
            // Do Error here
            setState(State::Null);
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
            while (state() == State::Running) {
                mThread->waitTask(10); //< Delay 10ms if no event
                updateClock();
            }
        }
        NEKO_DEBUG("Pipeline Thread Quit");
    }
    void updateClock() {
        // Update clock if
        auto current = masterClock()->position();
        if (std::abs(current - mPosition) > 1.0) {
            mPosition = int64_t(current);
            NEKO_DEBUG(mPosition);
#ifndef     NDEBUG
            std::shared_lock lock(mClockMutex);
            MediaClock *audioClock = nullptr;
            MediaClock *videoClock = nullptr;
            for (auto v : mClocks) {
                if (v->type() == MediaClock::Audio) {
                    audioClock = v;
                }
                if (v->type() == MediaClock::Video) {
                    videoClock = v;
                }
                if (audioClock && videoClock) {
                    NEKO_DEBUG(audioClock->position() - videoClock->position());
                    break;
                }
            }
            for (auto v : mClocks) {
                NEKO_DEBUG(v->type());
                NEKO_DEBUG(v->position());
            }
#endif

            sendBusEvent(ClockEvent::make(Event::ClockUpdated, masterClock(), this));
        }
    }

    // Pipeline
    Thread            *mThread = nullptr;
    Vec<Arc<Element> > mElements;
    Atomic<bool>       mRunning {false};
    Context            mContext;

    // MediaController
    Vec<MediaClock *>  mClocks;
    MediaClock        *mMasterClock = nullptr;
    ExternalClock      mExternalClock;
    double             mPosition = 0.0; //< Current position
    mutable std::shared_mutex mClockMutex;

    // Event
    std::function<void(View<Event>)> mEventCallback;
};

Arc<Pipeline> CreatePipeline() {
    return make_shared<PipelineImpl>();
}

NEKO_REGISTER_ELEMENT(Pipeline, PipelineImpl);

NEKO_NS_END