#define _NEKO_SOURCE
#include "../detail/template.hpp"
#include "../factory.hpp"
#include "../media.hpp"
#include "../log.hpp"
#include "../pad.hpp"
#include "audiosink.hpp"
#include "audiodev.hpp"
#include <cstring>

NEKO_NS_BEGIN

/**
 * @brief Present for generic audio
 * 
 */
class AudioSinkImpl final : public Template::GetImpl<AudioSink, MediaClock> {
public:
    AudioSinkImpl() {
        auto pad = addInput("sink");
        pad->addProperty(Properties::SampleFormatList, {
            // SampleFormat::U8, 
            // SampleFormat::S16, 
            // SampleFormat::S32, 
            SampleFormat::FLT
        });
        pad->setCallback(std::bind(&AudioSinkImpl::processInput, this, std::placeholders::_1));
        pad->setEventCallback([this](View<Event> event) {
            if (event->type() == Event::FlushRequested) {
                std::lock_guard locker(mMutex);
                while (!mFrames.empty()) {
                    mFrames.pop();
                }
                return Error::Ok;
            }
            else if (event->type() == Event::SeekRequested) {
                // mPosition = event.viewAs<SeekEvent>()->time();
                // NEKO_DEBUG(mPosition);
            }
            return Error::Ok;
        });
    }
    ~AudioSinkImpl() {

    }

    Error onInitialize() override {
        mDevice = CreateAudioDevice();
        mDevice->setCallback(std::bind(&AudioSinkImpl::audioCallback, this, std::placeholders::_1, std::placeholders::_2));

        mController = GetMediaController(this);
        if (mController) {
            mController->addClock(this);
        }
        return Error::Ok;
    }
    Error onTeardown() override {
        mDevice.reset();
        if (mController) {
            mController->removeClock(this);
        }
        mController = nullptr;
        return Error::Ok;
    }
    Error onPause() override {
        if (mDevice) {
            mDevice->pause(true);
        }
        return Error::Ok;
    }
    Error onStop() override {
        std::lock_guard locker(mMutex);
        while (!mFrames.empty()) {
            mFrames.pop();
        }
        return Error::Ok;
    }
    Error onRun() override {
        if (mDevice) {
            mDevice->pause(false);
        }
        return Error::Ok;
    }
    Error processInput(ResourceView resourceView) {
        if (state() != State::Running && state() != State::Paused) {
            NEKO_DEBUG("Can not process input at this state");
            NEKO_DEBUG(state());
            return Error::TemporarilyUnavailable;
        }
        auto frame = resourceView.viewAs<MediaFrame>();
        if (!frame) {
            return Error::UnsupportedResource;
        }
        if (!mOpened) {
            // Try open it
            mOpened = mDevice->open(frame->sampleFormat(), frame->sampleRate(), frame->channels());
            if (!mOpened) {
                return raiseError(Error::UnsupportedFormat, "Failed to open audio device");
            }
            // Start it
            mDevice->pause(false);
        }

        // Is Opened, write to queue
        std::unique_lock lock(mMutex);
        mFrames.push(frame->shared_from_this<MediaFrame>());
        while (state() == State::Running && mFrames.size() > mMaxFrames) {
            lock.unlock();
            if (Thread::msleep(10) == Error::Interrupted) {
                // In current thread, new task ready
                NEKO_DEBUG("Current Thread::msleep interrupted");
                return Error::Ok;
            }
            lock.lock();
        }
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
            ::memcpy(buf, (uint8_t*) frameData + mCurrentFramePosition, bytes);

            mCurrentFramePosition += bytes;
            buf += bytes;
            len -= bytes;

            // Update audio clock
            mPosition += mCurrentFrame->duration() * bytes / frameLen;
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
    Error setDevice(AudioDevice *device) override {
        // assert(false && ! "Not impl yet");
        return Error::NoImpl;
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
    size_t                       mMaxFrames = 10;
};

NEKO_REGISTER_ELEMENT(AudioSink, AudioSinkImpl);

NEKO_NS_END