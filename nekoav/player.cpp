#define _NEKO_SOURCE
#include "elements/videosink.hpp"
#include "elements/audiosink.hpp"
#include "elements/demuxer.hpp"
#include "elements/decoder.hpp"
#include "elements/mediaqueue.hpp"
#include "elements/videocvt.hpp"
#include "elements/audiocvt.hpp"
#include "pipeline.hpp"
#include "factory.hpp"
#include "event.hpp"
#include "player.hpp"
#include "media.hpp"
#include "pad.hpp"

NEKO_NS_BEGIN

class PlayerImpl final : public Player {
public:
    PlayerImpl() {

    }
    ~PlayerImpl() {

    }

    void cleanupPipeline() {
        mPipeline.reset();
        mDemuxer.reset();
        mController = nullptr;
    }
    void buildPipeline() {
        cleanupPipeline();

        Error err;

        auto factory = GetElementFactory();
        mPipeline = factory->createElement<Pipeline>();

        // Add elements
        mDemuxer = factory->createElement<Demuxer>();
        err = mPipeline->addElements(mDemuxer);
        
        // Audio part
        auto audioQueue = factory->createElement<MediaQueue>();
        auto audioDecoder = factory->createElement<Decoder>();
        auto audioCvt = factory->createElement<AudioConverter>();
        auto audioSink = factory->createElement<AudioSink>();

        err = mPipeline->addElements(audioQueue, audioDecoder, audioCvt, audioSink);
        err = LinkElements(audioQueue, audioDecoder, audioCvt, audioSink);

        // Video Parts
        auto videoQueue = factory->createElement<MediaQueue>();
        auto videoDecoder = factory->createElement<Decoder>();
        auto videoCvt = factory->createElement<VideoConverter>();
        auto videoSink = factory->createElement<VideoSink>();

        err = mPipeline->addElements(videoQueue, videoDecoder, videoCvt, videoSink);
        err = LinkElements(videoQueue, videoDecoder, videoCvt, videoSink);

        mController = GetMediaController(mPipeline);
        NEKO_ASSERT(mController);

        // Set Props
        mDemuxer->setUrl(mUrl);
        videoSink->setRenderer(mRenderer);
    }
    Error changeState(StateChange change) override {
        if (!mPipeline) {
            buildPipeline();
        }
        auto err = mPipeline->setState(GetTargetState(change));
        if (change == StateChange::Prepare) {
            // TODO 
            // Connect the demuxer
            // for (auto &output : mDemuxer->outputs()) {
            //     if (output->name().starts_with("video")) {
            //         output->link();
            //     }
            // }
        }
        if (change == StateChange::Teardown) {
            cleanupPipeline();
        }
        return err;
    }
    Error sendEvent(View<Event> event) override {
        if (!mPipeline) {
            return Error::InvalidState;
        }
        return mPipeline->sendEvent(event);
    }
    void setUrl(std::string_view url) override {
        mUrl = url;
    }
    void setVideoRenderer(VideoRenderer *render) override {
        mRenderer = render;
    }
    void setPosition(double pos) override {
        if (mPipeline) {
            auto event = SeekEvent::make(pos);
            mPipeline->sendEvent(event);
        }
    }
    double position() const override {
        if (mController) {
            return mController->masterClock()->position();
        }
        return 0.0;
    }
    double duration() const override {
        if (mDemuxer) {
            return mDemuxer->duration();
        }
        return 0.0;
    }
private:
    Arc<Pipeline>    mPipeline;
    Arc<Demuxer>     mDemuxer;
    MediaController *mController = nullptr;
    std::string      mUrl;
    VideoRenderer   *mRenderer = nullptr;
};

NEKO_REGISTER_ELEMENT(Player, PlayerImpl);

NEKO_NS_END