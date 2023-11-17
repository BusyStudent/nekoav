#define _NEKO_SOURCE
#include "../threading.hpp"
#include "../factory.hpp"
#include "../event.hpp"
#include "../media.hpp"
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
        mSink->setEventCallback(std::bind(&MediaQueueImpl::processEvent, this, std::placeholders::_1));
    }
    ~MediaQueueImpl() {
        delete mThread;
        mThread = nullptr;
    }
    Error sendEvent(View<Event>) override {
        return Error::Ok;
    }
    Error changeState(StateChange change) override {
        if (change == StateChange::Initialize) {
            mRunning = true;
            mThread = new Thread(&MediaQueueImpl::threadEntry, this);
        }
        else if (change == StateChange::Teardown) {
            mRunning = false;
            mCond.notify_one();
            delete mThread;
            mThread = nullptr;

            clearQueue();
        }
        return Error::Ok;
    }
    Error processInput(View<Resource> resource) {
        if (!resource) {
            return Error::InvalidArguments;
        }
        
        Item item;
        item.packet = resource.viewAs<MediaPacket>();
        item.resource = resource->shared_from_this();
        
        if (item.packet) {
            mDuration += item.packet->duration();
        }
        else {
            item.frame = resource.viewAs<MediaFrame>();
            if (item.frame) {
                mDuration += item.frame->duration();
            }
        }
        return pushItem(std::move(item));
    }
    Error processEvent(View<Event> event) {
        if (!event) {
            return Error::InvalidArguments;
        }
        if (event->type() == Event::FlushRequested) {
            clearQueue();
        }
        Item item;
        item.event = event->shared_from_this();
        return pushItem(std::move(item));
    }
    Error pushItem(auto &&item) {
        std::unique_lock lock(mMutex);
        mQueue.emplace(std::move(item));
        lock.unlock();

        mCond.notify_one();

        lock.lock();
        while (mQueue.size() > mMaxSize) {
            lock.unlock();

            if (Thread::msleep(10) == Error::Interrupted) {
                // Interrupted
                return Error::Ok;
            }
            if (state() != State::Running) {
                // If not running, return directly
                return Error::Ok;
            }

            lock.lock();
        }
        return Error::Ok;
    }
    void threadEntry() {
        mThread->setName(name().c_str());
        while (mRunning) {
            mThread->dispatchTask();
            while (state() == State::Running && mRunning) {
                mThread->dispatchTask();
                pullQueue();
            }
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
            lock.unlock();

            if (item.event) {
                mSrc->pushEvent(item.event);
            }
            else {
                if (item.packet) {
                    mDuration -= item.packet->duration();
                }
                else if (item.frame) {
                    mDuration -= item.frame->duration();
                }
                mSrc->push(item.resource);
            }

            lock.lock();
        }
    }
    void clearQueue() {
        // Clear Queue
        std::lock_guard locker(mMutex);
        while (!mQueue.empty()) {
            mQueue.pop();
        }
        mDuration = 0.0;
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
        MediaPacket  *packet = nullptr;
        Arc<Resource> resource;
        Arc<Event>    event;
    };
    std::queue<Item>        mQueue;
    std::condition_variable mCond;
    std::mutex              mMutex;
    Atomic<double>          mDuration {0.0};
    Atomic<bool>            mRunning {false};
    size_t                  mMaxSize = 4000;
    Thread                 *mThread = nullptr;
    Pad                    *mSink;
    Pad                    *mSrc;
};

NEKO_REGISTER_ELEMENT(MediaQueue, MediaQueueImpl);

NEKO_NS_END
