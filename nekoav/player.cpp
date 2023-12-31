#define _NEKO_SOURCE
#include "elements/videosink.hpp"
#include "elements/audiosink.hpp"
#include "elements/demuxer.hpp"
#include "elements/decoder.hpp"
#include "elements/subtitle.hpp"
#include "elements/mediaqueue.hpp"
#include "elements/videocvt.hpp"
#include "elements/audiocvt.hpp"
#include "threading.hpp"
#include "pipeline.hpp"
#include "factory.hpp"
#include "player.hpp"
#include "media.hpp"
#include "libc.hpp"
#include "event.hpp"
#include "pad.hpp"

#include <filesystem>

NEKO_NS_BEGIN

class PlayerPrivate {
public:
    Arc<Pipeline>    mPipeline;
    Arc<Demuxer>     mDemuxer;
    Arc<MediaQueue>  mAudioQueue;
    Arc<MediaQueue>  mVideoQueue;
    Arc<SubtitleFilter> mSubtitleFilter;
    MediaController *mController = nullptr;
};

Player::Player() {

}
Player::~Player() {
    stop();
}

void *Player::addFilter(const Filter &filter) {
    // Print in hex
    if (!GetElementFactory()->createElement(filter.name())) {
        // Unexists
        return nullptr;
    }
    return &mFilters.emplace_back(filter);
}
void Player::removeFilter(void *where) {
    if (where == nullptr) {
        return;
    }
    for (auto it = mFilters.begin(); it != mFilters.end(); it++) {
        if (&*it == where) {
            mFilters.erase(it);
            return;
        }
    }
}

void Player::setUrl(std::string_view url) {
    stop();
    mUrl = url;
}
void Player::setOptions(const Properties *options) {
    if (options) {
        mOptions.reset(new Properties(*options));
    }
    else {
        mOptions.reset();
    }
}
void Player::setOption(std::string_view key, std::string_view value) {
    if (!mOptions) {
        mOptions.reset(new Properties);
    }
    mOptions->insert(std::make_pair(std::string(key), Property(value)));
}

void Player::setVideoRenderer(VideoRenderer* renderer) {
    mRenderer = renderer;
}


void Player::stop() {
    mState = State::Null;
    delete mThread;
    d.reset();
    mThread = nullptr;
}
void Player::setErrorCallback(std::function<void(Error, std::string_view)>&& callback) {
    mErrorCallback = std::move(callback);
}
void Player::setPositionCallback(std::function<void(double)>&& callback) {
    mPositionCallback = std::move(callback);
}
void Player::setStateChangedCallback(std::function<void(State)>&& callback) {
    mStateChangedCallback = std::move(callback);
}
void Player::setPosition(double position) {
    if (d && d->mPipeline) {
        auto event = SeekEvent::make(position);
        d->mPipeline->sendEvent(event);
    }
}
double Player::duration() const noexcept {
    if (d && d->mDemuxer) {
        return d->mDemuxer->duration();
    }
    return 0.0;
}
double Player::position() const noexcept {
    if (d && d->mController) {
        return d->mController->masterClock()->position();
    }
    return 0.0;
}
State Player::state() const noexcept {
    return mState;
}
bool Player::hasAudio() const noexcept {
    if (d && d->mDemuxer) {
        for (const auto& pad : d->mDemuxer->outputs()) {
            if (pad->name().starts_with("audio")) {
                return true;
            }
        }
    }
    return false;
}
bool Player::hasVideo() const noexcept {
    if (d && d->mDemuxer) {
        for (const auto& pad : d->mDemuxer->outputs()) {
            if (pad->name().starts_with("video")) {
                return true;
            }
        }
    }
    return false;
}
bool Player::isSeekable() const noexcept {
    if (d && d->mDemuxer) {
        return d->mDemuxer->isSeekable();
    }
    return false;
}

void Player::play() {
    if (mState == State::Null) {
        _load();
    }
    else if (mState == State::Paused) {
        d->mPipeline->setState(State::Running);
        _setState(State::Running);
    }
}
void Player::pause() {
    if (mState == State::Running) {
        d->mPipeline->setState(State::Paused);
        _setState(State::Paused);
    }
}
void Player::_load() {
    stop();
    if (mUrl.empty()) {
        return _error(Error::InvalidArguments, "No Source url");
    }

    // Cleanup
    d.reset(new PlayerPrivate);

    // Build pipeline and demuxer
    auto factory = GetElementFactory();

    d->mPipeline = CreatePipeline();
    d->mController = GetMediaController(d->mPipeline);
    d->mDemuxer = factory->createElement<Demuxer>();

    // Add into
    d->mPipeline->addElement(d->mDemuxer);


    // Check all ready
    // if (!d->mAudioQueue && !d->mVideoQueue) {
    //     return _error(Error::InvalidState, "No A/V Output has set");
    // }
    d->mPipeline->setEventCallback(std::bind(&Player::_translateEvent, this, std::placeholders::_1));
    d->mDemuxer->setName("NekoDemuxer");
    d->mDemuxer->setUrl(mUrl);
    d->mDemuxer->setOptions(mOptions.get());

    // Begin actually load
    mThread = new Thread(&Player::_run, this);
}
void Player::_error(Error err, std::string_view msg) {
    if (mErrorCallback) {
        mErrorCallback(err, msg);
    }
    _setState(State::Null);
}
void Player::_setState(State state) {
    if (mState == state) {
        return;
    }
    mState = state;
    if (mStateChangedCallback) {
        mStateChangedCallback(state);
    }
}
void Player::_run() {
    auto err = d->mPipeline->setState(State::Ready);
    if (err != Error::Ok) {
        return _error(err, "Fail to set pipeline state to ready");
    }
    // Connect Audio if
    if (true) {
        _buildAudioPart();
        for (auto pad : d->mDemuxer->outputs()) {
            if (pad->name().starts_with("audio")) {
                err = LinkElement(d->mDemuxer, pad->name(), d->mAudioQueue, "sink");
                break;
            }
        }
    }
    if (err != Error::Ok) {
        return _error(err, "Fail to link audio elements");
    }
    // Connect Video if
    if (mRenderer) {
        _buildVideoPart();
        for (auto pad : d->mDemuxer->outputs()) {
            if (pad->name().starts_with("video")) {
                err = LinkElement(d->mDemuxer, pad->name(), d->mVideoQueue, "sink");
                break;
            }
        }
    }
    if (err != Error::Ok) {
        return _error(err, "Fail to link video elements");
    }
    
    err = d->mPipeline->setState(State::Ready);
    if (err != Error::Ok) {
        return _error(err, "Fail to set state to ready");
    }
    _setState(State::Ready);

    err = d->mPipeline->setState(State::Running);
    if (err != Error::Ok) {
        return _error(err, "Fail to set state to running");
    }
    _setState(State::Running);
}
void Player::_buildAudioPart() {
    auto factory = GetElementFactory();
    d->mAudioQueue = factory->createElement<MediaQueue>();

    auto decoder = factory->createElement<Decoder>();
    auto converter = factory->createElement<AudioConverter>();
    auto audiosink = factory->createElement<AudioSink>();

    //< From Queue to sink
    auto err = d->mPipeline->addElements(d->mAudioQueue, decoder, converter, audiosink);
    if (err != Error::Ok) {
        return _error(err, "Fail to add audio elements");
    }
    err = LinkElements(d->mAudioQueue, decoder, converter, audiosink);
    if (err != Error::Ok) {
        return _error(err, "Fail to link audio elements");
    }

    d->mAudioQueue->setName("NekoAudioQueue");
}
void Player::_buildVideoPart() {
    auto factory = GetElementFactory();
    d->mVideoQueue = factory->createElement<MediaQueue>();

    auto decoder = factory->createElement<Decoder>();
    auto converter = factory->createElement<VideoConverter>();
    auto renderer = factory->createElement<VideoSink>();

    // From Queue to sink
    auto err = d->mPipeline->addElements(d->mVideoQueue, decoder, converter, renderer);
    if (err != Error::Ok) {
        return _error(err, "Fail to add video elements");
    }
    err = LinkElements(d->mVideoQueue, decoder, converter);
    if (err != Error::Ok) {
        return _error(err, "Fail to link video elements");
    }

    // Last of it
    Arc<Element> prevElement = converter;

    // Check has subtitle
    bool hasSubtitle = true;
    if (mSubtitleUrl.empty()) {
        // Try detect it from the src
        hasSubtitle = false;
        for (auto pad : d->mDemuxer->outputs()) {
            if (pad->name().starts_with("subtitle")) {
                hasSubtitle = true;
                break;
            }
        }
    }
#if 1
    if (!hasSubtitle) {
        // Avoid code-page mismatch
        // We use utf8 
        std::filesystem::path path(reinterpret_cast<const char8_t *>(mUrl.c_str()));
        std::u8string subtitleUrl;
        path.replace_extension(".srt");
        if (path.replace_extension(".srt"); std::filesystem::exists(path)) {
            hasSubtitle = true;
            subtitleUrl = path.u8string();
        }
        else if (path.replace_extension(".ass"); std::filesystem::exists(path)) {
            hasSubtitle = true;
            subtitleUrl = path.u8string();
        }
        if (!subtitleUrl.empty()) {
            mSubtitleUrl = reinterpret_cast<const char *>(subtitleUrl.c_str());
        }
    }
#endif
    if (hasSubtitle) {
        d->mSubtitleFilter = factory->createElement<SubtitleFilter>();
        if (d->mSubtitleFilter) {
            // Configure it
            if (_configureSubtitle()) {
                // Configure ok
                LinkElements(prevElement, d->mSubtitleFilter);
                prevElement = d->mSubtitleFilter;
            }
        }
    }

    // Begin add filters into chain
    for (const auto &filter : mFilters) {
        auto currentElement = factory->createElement(filter.mName);
        if (!currentElement) {
            return _error(Error::InvalidArguments, libc::asprintf("Fail to create filter '%s'", filter.mName));
        }
        if (filter.mConfigure) {
            filter.mConfigure(*currentElement);
        }
        d->mPipeline->addElement(currentElement);
        LinkElements(prevElement, currentElement);
        prevElement = currentElement;
    }

    // Link the last element to renderer
    err = LinkElements(prevElement, renderer);
    if (err != Error::Ok) {
        return _error(err, "Fail to link video elements");
    }

    renderer->setRenderer(mRenderer);
    renderer->setName("NekoVideoSink");
    d->mVideoQueue->setName("NekoVideoQueue");
}
bool Player::_configureSubtitle() {
    d->mPipeline->addElements(d->mSubtitleFilter);
    d->mSubtitleFilter->setName("NekoSubtitleFilter");
    std::string_view url = mSubtitleUrl;
    if (mSubtitleUrl.empty()) {
        url = mUrl;
    }
    d->mSubtitleFilter->setUrl(url);
    // Let it load
    if (auto err = d->mSubtitleFilter->setState(State::Paused); err != Error::Ok) {
        // Fail
        d->mPipeline->removeElement(d->mSubtitleFilter);
        return false;
    }
    // Select subtite
    d->mSubtitleFilter->setSubtitle(0);
    // Done
    return true;
}
void Player::_translateEvent(View<Event> event) {
    switch (event->type()) {
        case Event::ClockUpdated: {
            if (mPositionCallback) {
                mPositionCallback(event.viewAs<ClockEvent>()->position());
            }
            break;
        }
        case Event::ErrorOccurred: {
            auto err = event.viewAs<ErrorEvent>();
            _error(err->error(), err->message());
            break;
        }
        case Event::MediaEndOfFile: {
            _setState(State::Null);
            break;
        }
    }
}

NEKO_NS_END