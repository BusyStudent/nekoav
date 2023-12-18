#define _NEKO_SOURCE
#include "../detail/template.hpp"
#include "../format.hpp"
#include "../factory.hpp"
#include "../media.hpp"
#include "../utils.hpp"
#include "../pad.hpp"
#include "../log.hpp"
#include "videosink.hpp"

// #ifdef _WIN32
#if 0
    #include <wrl/client.h>
    #include <d2d1_1.h>
    #include <d2d1.h>
#endif

NEKO_NS_BEGIN

class VideoSinkImpl final : public Template::GetThreadImpl<VideoSink, MediaClock, MediaElement> {
public:
    VideoSinkImpl() {
        mSink = addInput("sink");
        mSink->setCallback(std::bind(&VideoSinkImpl::onSink, this, std::placeholders::_1));
        mSink->setEventCallback(std::bind(&VideoSinkImpl::onSinkEvent, this, std::placeholders::_1));
    }
    ~VideoSinkImpl() {

    }
    Error setRenderer(VideoRenderer *renderer) override {
        mRenderer = renderer;
        return Error::Ok;
    }

    // Element
    Error onInitialize() override {
        mController = GetMediaController(this);
        if (!mRenderer) {
            return Error::InvalidState;
        }
        auto prop = Property::newList();
        for (auto fmt : mRenderer->supportedFormats()) {
            prop.push_back(fmt);
        }
        mSink->addProperty(Properties::PixelFormatList, std::move(prop));
        return Error::Ok;
    }
    Error onTeardown() override {
        mSink->properties().clear();
        mRenderer->setFrame(nullptr); //< Tell Renderer is no frame now
        mController = nullptr;
        mAfterSeek = false;
        return Error::Ok;
    }
    Error sendEvent(View<Event> event) override {
        return Error::Ok;
    }
    Error onSink(View<Resource> resource) {
        auto frame = resource.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (mAfterSeek) {
            mAfterSeek = false;
            NEKO_LOG("After seek, first frame arrived pts {}", frame->timestamp());
        }
        // Wait if too much
        std::unique_lock lock(mMutex);
        while (mFrames.size() > 4) {
            lock.unlock();
            if (Thread::msleep(10) == Error::Interrupted) {
                // Is Interrupted, we need return right now
                lock.lock();
                break;
            }
            lock.lock();
        }
        // Push one to the frame queue
        mFrames.emplace(frame->shared_from_this<MediaFrame>());
        thread()->postTask([]() {}); //< To wakeup
        return Error::Ok;
    }
    Error onSinkEvent(View<Event> event) {
        if (event->type() == Event::FlushRequested) {

        }
        else if (event->type() == Event::SeekRequested) {
            mAfterSeek = true;
            NEKO_DEBUG(event.viewAs<SeekEvent>()->position());
        }
        else {
            return Error::Ok;
        }

        mNumFramesDropped = 0;
        // Flush frames
        std::lock_guard lock(mMutex);
        while (!mFrames.empty()) {
            mFrames.pop();
        }
        mCondition.notify_one();
        return Error::Ok;
    }
    void drawFrame(const Arc<MediaFrame> &frame) {
        if (!mController) {
            mRenderer->setFrame(frame);
            return;
        }
        // Sync here
        auto current = mController->masterClock()->position();
        auto pts = frame->timestamp();
        auto diff = current - pts;

        mPosition = pts;
        if (diff < -0.01 && diff > -10.0) {
            // Too fast
            std::unique_lock lock(mCondMutex);
            mCondition.wait_for(lock, std::chrono::milliseconds(int64_t(-diff * 1000)));
        }
        else if (diff > 0.3) {
            mNumFramesDropped += 1;
            if (mNumFramesDropped > 10) {
                // NEKO_BREAKPOINT();
                NEKO_DEBUG(mNumFramesDropped.load());
            }
            // Too slow, drop
            NEKO_DEBUG("Too slow, drop");
            NEKO_DEBUG(diff);
            return;
        }
        mRenderer->setFrame(frame);
    }
    Error onLoop() override {
        while (!stopRequested()) {
            thread()->waitTask();
            while (state() == State::Running) {
                thread()->waitTask(10);
                std::unique_lock lock(mMutex);
                while (!mFrames.empty() && state() == State::Running) {
                    auto frame = std::move(mFrames.front());
                    mFrames.pop();
                    lock.unlock();

                    // Do drawing
                    drawFrame(frame);
                    thread()->dispatchTask();

                    lock.lock();
                }
            }
        }
        return Error::Ok;
    }
    // MediaClock
    double position() const override {
        return mPosition;
    }
    ClockType type() const override {
        return ClockType::Video;
    }

    // MediaElement
    bool isEndOfFile() const override {
        std::lock_guard lock(mMutex);
        return mFrames.empty();
    }
    MediaClock *clock() const override {
        return const_cast<VideoSinkImpl*>(this);
    }
private:
    // MediaController
    MediaController *mController = nullptr;

    // Vec<PixelFormat> mFormats;
    VideoRenderer   *mRenderer = nullptr;
    Pad             *mSink = nullptr;
    bool             mAfterSeek = false;

    std::condition_variable mCondition;
    std::mutex              mCondMutex;

    mutable std::mutex           mMutex;
    std::queue<Arc<MediaFrame> > mFrames;
    
    Atomic<size_t> mNumFramesDropped {0};
    Atomic<double> mPosition {0.0}; //< Current time
};
NEKO_REGISTER_ELEMENT(VideoSink, VideoSinkImpl);

NEKO_NS_END