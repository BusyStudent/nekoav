#define _NEKO_SOURCE
#include "../factory.hpp"
#include "../media.hpp"
#include "../utils.hpp"
#include "../pad.hpp"
#include "mediaqueue.hpp"

#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

NEKO_NS_BEGIN

class MediaQueueImpl final : public MediaQueue {
public:
    MediaQueueImpl() {
        mSink = addInput("sink");
        mSrc = addOutput("src");

        mSink->setCallback(std::bind(&MediaQueueImpl::processInput, this, std::placeholders::_1));
    }
    ~MediaQueueImpl() {

    }
    Error sendEvent(View<Event>) override {
        return Error::Ok;
    }
    Error changeState(StateChange change) override {
        if (change == StateChange::Initialize) {
            mRunning = true;
            mThread = std::thread(&MediaQueueImpl::threadEntry, this);
        }
        else if (change == StateChange::Teardown) {
            mRunning = false;
            mCond.notify_one();
            mThread.join();
        }
        return Error::Ok;
    }
    Error processInput(View<Resource> resource) {
        if (!resource) {
            return Error::InvalidArguments;
        }
        std::unique_lock lock(mMutex);
        
        Item item;
        item.frame = resource.viewAs<MediaFrame>();
        item.resource = resource->shared_from_this();

        if (item.frame) {
            mDuration += item.frame->duration();
        }

        mQueue.emplace(std::move(item));
        lock.unlock();

        mCond.notify_one();

        lock.lock();
        while (mQueue.size() > mMaxSize) {
            lock.unlock();

            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            lock.lock();
        }
        return Error::Ok;
    }
    void threadEntry() {
        NEKO_SetThreadName(name());
        while (mRunning) {
            while (state() == State::Running && mRunning) {
                pullQueue();
            }
        }
    }
    void pullQueue() {
        std::unique_lock lock(mMutex);
        while (mRunning && mQueue.empty()) {
            mCond.wait(lock);
        }
        if (!mQueue.empty()) {
            Item item = std::move(mQueue.front());
            mQueue.pop();

            if (item.frame) {
                mDuration -= item.frame->duration();
            }
            mSrc->push(item.resource);
        }
    }
    double duration() const override {
        return mDuration.load();
    }
    void setCapacity(size_t n) override {
        mMaxSize = n;
    }
private:
    class Item {
    public:
        MediaFrame   *frame = nullptr;
        Arc<Resource> resource;
    };
    std::queue<Item>        mQueue;
    std::condition_variable mCond;
    std::mutex              mMutex;
    Atomic<double>          mDuration {0.0};
    Atomic<bool>            mRunning {false};
    size_t                  mMaxSize = 4000;
    std::thread             mThread;
    Pad                      *mSink;
    Pad                      *mSrc;
};

NEKO_REGISTER_ELEMENT(MediaQueue, MediaQueueImpl);

NEKO_NS_END
