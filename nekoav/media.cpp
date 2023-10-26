#define _NEKO_SOURCE
#include "media.hpp"
#include "queue.hpp"
#include "format.hpp"
#include "audio/device.hpp"
#include "video/renderer.hpp"
#include "time.hpp"
#include "log.hpp"
#include <chrono>
#include <queue>

NEKO_NS_BEGIN

namespace _abiv1 {

namespace chrono = std::chrono;

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
    ~MediaQueueImpl() {

    }
    Error run() override {
        while (state() != State::Stopped) {
            dispatchTask(); //< 0n Idle
            std::unique_lock lock(mMutex);
            while (!mQueue.empty()) {
                auto data = std::move(mQueue.front());
                mQueue.pop();

                lock.unlock();
                mCondition.notify_one();

                // Send it
                mSourcePad->send(data);
                dispatchTask();

                lock.lock();
            }
            // Empty release it
            lock.unlock();

            waitTask(2);
        }
        return Error::Ok;
    }
    Error doProcessInput(Pad &, ResourceView resource) override {
        // We should return as soon as possible
        // NEKO_DEBUG(mQueue.size());
        std::unique_lock lock(mMutex);
        mCondition.wait(lock, [this]() {
            auto v = mQueue.size() < mCapacity;
            if (!v) {
                // NEKO_LOG("MediaQueue is full current {}", mQueue.size());
            }
            return v;
        });

        mQueue.push(resource->shared_from_this());
        return Error::Ok;
    }
    Error teardown() override {
        while (!mQueue.empty()) {
            mQueue.pop();
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
    std::condition_variable    mCondition;
    std::mutex                 mMutex;
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
class VideoPresenterImpl final : public VideoPresenter, MediaClock {
public:
    VideoPresenterImpl() {
        setThreadPolicy(ThreadPolicy::SingleThread);
        
        mSinkPad = addInput("sink");
    }
    ~VideoPresenterImpl() {

    }
    Error init() override {
        mController = graph()->queryInterface<MediaController>();
        if (!mController) {
            return Error::InvalidState;
        }
        if (!mRenderer) {
            return Error::InvalidState;
        }
        mController->addClock(this);
        mRenderer->init();

        // Query the prop and add it to it
        auto list = Property::newList();
        for (auto fmt : mRenderer->supportedFormats()) {
            list.push_back(fmt);
        }
        mSinkPad->property(MediaProperty::PixelFormatList) = std::move(list);
        return Error::Ok;
    }
    Error teardown() override {
        if (!mRenderer) {
            return Error::InvalidState;
        }
        mSinkPad->properties().clear();
        mRenderer->teardown();
        mController->removeClock(this);
        mController = nullptr;
        return Error::Ok;
    }
    Error run() override {
        while (state() != State::Stopped) {
            waitTask();
        }
        return Error::Ok;
    }
    Error processInput(Pad &pad, ResourceView resourceView) {
        // Draw frame out
        auto frame = resourceView.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        auto pts = frame->timestamp();
        auto diff = mController->masterClock()->position() - pts;
        if (diff < -0.01) {
            // Too fast
            std::this_thread::sleep_for(std::chrono::milliseconds(int64_t(-diff * 1000)));
        }
        else if (diff > 0.3) {
            // Too slow, drop
            NEKO_DEBUG("Too slow, drop");
            NEKO_DEBUG(diff);
            return Error::Ok;
        }
        // else {
        //     // Normal, sleep by duration
        //     auto delay = std::min(diff, frame->duration());
        //     std::this_thread::sleep_for(std::chrono::milliseconds(int64_t(delay * 1000)));
        // }

        mRenderer->sendFrame(frame);
        mPosition = pts;
        return Error::Ok;
    }
    void setRenderer(VideoRenderer *render) override {
        mRenderer = render;
    }
    double position() const override {
        return mPosition.load();
    }
    Type type() const override {
        return Video;
    }
private:
    Pad             *mSinkPad = nullptr;
    Atomic<double>   mPosition {0.0}; //< Current media clock position
    MediaController *mController = nullptr;
    VideoRenderer   *mRenderer = nullptr;
};
/**
 * @brief Present for generic audio
 * 
 */
class AudioPresenterImpl final : public AudioPresenter, MediaClock {
public:
    AudioPresenterImpl() {
        auto pad = addInput("sink");
        auto list = Property::newList();

        list.push_back(SampleFormat::U8);
        list.push_back(SampleFormat::S16);
        list.push_back(SampleFormat::S32);
        list.push_back(SampleFormat::FLT);

        pad->property(MediaProperty::SampleFormatList) = std::move(list);
    }
    ~AudioPresenterImpl() {

    }

    Error init() override {
        mDevice = CreateAudioDevice();
        mDevice->setCallback([this](void *buf, int len) {
            audioCallback(buf, len);
        });

        mController = graph()->queryInterface<MediaController>();
        if (mController) {
            mController->addClock(this);
        }
        return Error::Ok;
    }
    Error teardown() override {
        mDevice.reset();
        if (mController) {
            mController->removeClock(this);
        }
        mController = nullptr;
        return Error::Ok;
    }
    Error processInput(Pad &, ResourceView resourceView) override {
        auto frame = resourceView.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (!mOpened) {
            // Try open it
            mOpened = mDevice->open(frame->sampleFormat(), frame->sampleRate(), frame->channels());
            if (!mOpened) {
                return Error::UnsupportedFormat;
            }
            // Start it
            mDevice->pause(false);
        }

        // Is Opened, write to queue
        std::lock_guard lock(mMutex);
        mFrames.push(frame->shared_from_this<MediaFrame>());
        return Error::Ok;
    }

    double position() const override {
        return mPosition;
    }
    Type type() const override {
        return Audio;
    }
    void audioCallback(void *_buf, int len) {
        auto buf = reinterpret_cast<uint8_t*>(_buf);
        while (len > 0) {
            // No current frame
            if (!mCurrentFrame) {
                std::lock_guard lock(mMutex);
                if (mFrames.empty()) {
                    break;
                }
                mCurrentFramePosition = 0; //< Reset position
                mCurrentFrame = std::move(mFrames.front());
                mFrames.pop();

                // Update audio clock
                mPosition = mCurrentFrame->timestamp();
            }
            void *frameData = mCurrentFrame->data(0);
            // int frameLen = mCurrentFrame->linesize(0);
            int frameLen = mCurrentFrame->sampleCount() * 
                           mCurrentFrame->channels() * 
                           GetBytesPerSample(mCurrentFrame->sampleFormat());
            int bytes = std::min(len, frameLen - mCurrentFramePosition);
            memcpy(buf, (uint8_t*) frameData + mCurrentFramePosition, bytes);

            mCurrentFramePosition += bytes;
            buf += bytes;
            len -= bytes;

            // Update audio clock
#if         NEKO_CXX20
            mPosition += mCurrentFrame->duration() * bytes / frameLen;
#else
            mPosition  = mPosition + mCurrentFrame->duration() * bytes / frameLen;
#endif
            // NEKO_DEBUG(mPosition.load());

            if (mCurrentFramePosition == frameLen) {
                mCurrentFrame.reset();
                mCurrentFramePosition = 0;
            }
        }
        if (len > 0) {
            NEKO_DEBUG("No Audio data!!!!");
            ::memset(buf, 0, len);
        }
    }
    void setDevice(AudioDevice *device) override {
        assert(false && ! "Not impl yet");
    }
private:
    MediaController *mController = nullptr;
    Atomic<double>   mPosition {0.0}; //< Current media clock position
    Box<AudioDevice> mDevice;
    bool             mPaused = false;
    bool             mOpened = false;

    // Audio Callback data
    std::mutex                   mMutex;
    std::queue<Arc<MediaFrame> > mFrames;
    Arc<MediaFrame>              mCurrentFrame;
    int                          mCurrentFramePosition = 0;
};
class MediaFactoryImpl final : public MediaFactory {
public:
    MediaFactoryImpl() {

    }
    Box<Element> createElement(const char *name) const override {
        auto iter = mMap.find(name);
        if (iter != mMap.end()) {
            return iter->second();
        }
        for (auto m : mFactories) {
            if (auto v = m->createElement(name); v) {
                return v;
            }
        }
        return nullptr;
    }
    Box<Element> createElement(const std::type_info &info) const override {
        auto iter = mMap.find(typenameOf(info));
        if (iter != mMap.end()) {
            return iter->second();
        }
        for (auto m : mFactories) {
            if (auto v = m->createElement(info); v) {
                return v;
            }
        }
        return nullptr;
    }
    void registerFactory(ElementFactory *factory) override {
        if (factory == nullptr || factory == this) {
            return;
        }
        mFactories.push_back(factory);
    }
    static std::string_view typenameOf(const std::type_info &info) {
#ifdef _MSC_VER
        return info.raw_name();
#else
        return info.name();
#endif
    }
    template <typename T>
    static std::string_view typenameOf() {
        return typenameOf(typeid(T));
    }
    template <typename T>
    static Box<Element> make() {
        return std::make_unique<T>();
    }
private:
    std::vector<ElementFactory *> mFactories;
    std::map<std::string_view, Box<Element> (*)()> mMap {
        {typenameOf<MediaQueue>(), make<MediaQueueImpl>},
        {typenameOf<AppSource>(), make<AppSourceImpl>},
        {typenameOf<AppSink>(), make<AppSinkImpl>},
        {typenameOf<AudioPresenter>(), make<AudioPresenterImpl>},
        {typenameOf<VideoPresenter>(), make<VideoPresenterImpl>}
    };
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
auto CreateAudioPresenter() -> Box<AudioPresenter> {
    return std::make_unique<AudioPresenterImpl>();
}
auto CreateVideoPresenter() -> Box<VideoPresenter> {
    return std::make_unique<VideoPresenterImpl>();
}
auto GetMediaFactory() -> MediaFactory * {
    static MediaFactoryImpl f;
    return &f;
}


}

NEKO_NS_END