#define _NEKO_SOURCE
#include "../threading.hpp"
#include "../factory.hpp"
#include "../event.hpp"
#include "../media.hpp"
#include "../log.hpp"
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

        mSink->setCallback(std::bind(&MediaQueueImpl::_processInput, this, std::placeholders::_1));
        mSink->setEventCallback(std::bind(&MediaQueueImpl::_processEvent, this, std::placeholders::_1));
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
            mThread = new Thread(&MediaQueueImpl::_threadEntry, this);
        }
        else if (change == StateChange::Teardown) {
            mRunning = false;
            mCond.notify_one();
            delete mThread;
            mThread = nullptr;

            _clearQueue();
        }
        return Error::Ok;
    }
    Error _processInput(View<Resource> resource) {
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
        return _pushItem(std::move(item));
    }
    Error _processEvent(View<Event> event) {
        if (!event) {
            return Error::InvalidArguments;
        }
        if (event->type() == Event::FlushRequested) {
            _clearQueue();
            NEKO_DEBUG("Flush Queue");
        }
        // Send a event by task queue
        std::latch latch {1};
        mThread->postTask([&, this]() {
            NEKO_LOG("Push {} event at {}", event->type(), name());
            mSrc->pushEvent(event);
            latch.count_down();
        });
        mInterrupted = true;
        mCond.notify_one();
        latch.wait();
        return Error::Ok;
    }
    Error _pushItem(auto &&item) {
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
    void _threadEntry() {
        mThread->setName(name().c_str());
        while (mRunning) {
            mThread->dispatchTask();
            while (state() == State::Running && mRunning) {
                mThread->dispatchTask();
                _pullQueue();
            }
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    void _pullQueue() {
        std::unique_lock lock(mMutex);
        while (mRunning && mQueue.empty()) {
            mCond.wait(lock);

            if (mInterrupted) {
                NEKO_DEBUG("Interrupted by pad event");
                mInterrupted = false;
                return;
            }
        }
        if (!mQueue.empty()) {
            Item item = std::move(mQueue.front());
            mQueue.pop();
            lock.unlock();

            if (item.packet) {
                mDuration -= item.packet->duration();
            }
            else if (item.frame) {
                mDuration -= item.frame->duration();
            }
            mSrc->push(item.resource);

            lock.lock();
        }
    }
    void _clearQueue() {
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
    };
    std::queue<Item>        mQueue;
    std::condition_variable mCond;
    std::mutex              mMutex;
    Atomic<double>          mDuration {0.0};
    Atomic<bool>            mRunning {false};
    Atomic<bool>            mInterrupted {false};
    size_t                  mMaxSize = 4000;
    Thread                 *mThread = nullptr;
    Pad                    *mSink = nullptr;
    Pad                    *mSrc = nullptr;
};

NEKO_REGISTER_ELEMENT(MediaQueue, MediaQueueImpl);

NEKO_NS_END
