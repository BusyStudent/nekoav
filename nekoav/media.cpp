#define _NEKO_SOURCE
#include "media.hpp"
#include "queue.hpp"
#include <chrono>
#include <queue>

NEKO_NS_BEGIN

namespace _abiv1 {

namespace chrono = std::chrono;

static auto GetTicks() {
    return chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now().time_since_epoch()
    ).count();
}

ExternalClock::ExternalClock() {

}
ExternalClock::~ExternalClock() {

}
double ExternalClock::position() const {
    if (mPaused) {
        return mCurrent;
    }

    // Calc
    return double(GetTicks() - mTicks) / 1000.0;
}
auto ExternalClock::type() const -> Type {
    return External;
}
void ExternalClock::start() {
    if (!mPaused) {
        return;
    }
    mTicks = GetTicks() - mCurrent * 1000;
    mPaused = true;
}
void ExternalClock::pause() {
    if (mPaused) {
        return;
    }
    mCurrent = double(GetTicks() - mTicks) / 1000.0;
    mPaused = true;
}
void ExternalClock::setPosition(double position) {
    mTicks = GetTicks() - position * 1000;
    mCurrent = position;
}

MediaPipeline::MediaPipeline() {
    addClock(&mExternalClock);
}
MediaPipeline::~MediaPipeline() {
    mPipeline.stop();
}
void MediaPipeline::setGraph(Graph *graph) {
    mPipeline.setGraph(graph);
    graph->registerInterface<MediaController>(this);
}
void MediaPipeline::setState(State state) {
    mPipeline.setState(state);
    switch (state) {
        case State::Paused: mExternalClock.pause(); break;
        case State::Stopped: mExternalClock.pause(); mExternalClock.setPosition(0); break;
        case State::Running: mExternalClock.start(); break;
        default: break;
    }
}
void MediaPipeline::addClock(MediaClock *clock) {
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
void MediaPipeline::removeClock(MediaClock *clock) {
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
auto MediaPipeline::masterClock() const -> MediaClock * {
    std::lock_guard locker(mClockMutex);
    return mMasterClock;
}

class MediaQueueImpl final : public MediaQueue {
public:
    MediaQueueImpl() {
        setThreadPolicy(ThreadPolicy::SingleThread);
        mSinkPad = addInput("sink");
        mSourcePad = addOutput("src");
    }
    Error run() override {
        while (state() != State::Stopped) {
            waitTask(1);
            sendData();
        }
        return Error::Ok;
    }
    Error processInput(Pad &, ResourceView resource) override {
        // We should return as soon as possible
        while (mQueue.size() > mCapacity) {
            sendData();
        }
        mQueue.push(resource->shared_from_this());
        return Error::Ok;
    }
    Error sendData() {
        if (!mQueue.empty()) {
            auto data = std::move(mQueue.front());
            mQueue.pop();
            return mSourcePad->send(data);
        }
        return Error::Ok;
    }
    void setCapacity(size_t n) override {
        mCapacity = n;
    }
    size_t size() const override {
        return mQueue.size();
    }
private:
    std::queue<Arc<Resource> > mQueue;
    Pad                       *mSourcePad = nullptr;
    Pad                       *mSinkPad = nullptr;
    size_t                     mCapacity = 1000;
};

class AppSourceImpl final : public AppSource {
public:
    AppSourceImpl() {
        setThreadPolicy(ThreadPolicy::SingleThread);
        mSourcePad = addOutput("src");
    }
    Error run() override {
        Arc<Resource> resource;
        while (state() != State::Stopped) {
            if (mQueue.wait(&resource, 10)) {
                // Got data
                mSourcePad->send(resource);
            }
        }
        return Error::Ok;
    }
    Error push(const Arc<Resource> &res) override {
        if (state() == State::Stopped) {
            return Error::InvalidState;
        }
        mQueue.push(res);
        return Error::Ok;
    }
    size_t size() const override {
        return mQueue.size();
    }
private:
    BlockingQueue<Arc<Resource> > mQueue;
    Pad                          *mSourcePad = nullptr;
};
class AppSinkImpl final : public AppSink {
public:
    AppSinkImpl() {
        mSinkPad = addInput("sink");
    }
    Error processInput(Pad &, ResourceView resourceView) {
        mQueue.push(resourceView->shared_from_this());
        return Error::Ok;
    }
    size_t size() const override {
        return mQueue.size();
    }
    bool wait(Arc<Resource> *res, int timeout) override {
        return mQueue.wait(res, timeout);
    }
private:
    BlockingQueue<Arc<Resource> > mQueue;
    Pad                          *mSinkPad = nullptr;
};

auto CreateAppSource() -> Box<AppSource> {
    return std::make_unique<AppSourceImpl>();
}
auto CreateAppSink() -> Box<AppSink> {
    return std::make_unique<AppSinkImpl>();
}
auto CreateMediaQueue() -> Box<MediaQueue> {
    return std::make_unique<MediaQueueImpl>();
}



}

NEKO_NS_END