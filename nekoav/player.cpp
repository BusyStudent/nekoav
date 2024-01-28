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
    Arc<VideoSink>   mVideoSink;
    Arc<AudioSink>   mAudioSink;
    Arc<SubtitleFilter> mSubtitleFilter;
    MediaController *mController = nullptr;

    // Metadata from streams
    Vec<Properties> mSubtitleStreams;
    Vec<Properties> mAudioStreams;
    Vec<Properties> mVideoStreams;

    // Title from streams
    Vec<std::string_view> mSubtitleTitles;
    Vec<std::string_view> mAudioTitles;
    Vec<std::string_view> mVideoTitles;

    int             mtLoopsCount = 0; //< Num Of current Loops
};

Player::Player() {

}
Player::~Player() {
    stop();
}

//
//             VideoQueue -> Decoder -> VideoConverter -> SubtitleFilter(opt) -> [VFilters] -> VideoSink
// Demuxer ->
//             AudioQueue -> Decoder -> AudioConverter -> [AFilters] -> AudioSink
//

void *Player::addFilter(const Filter &filter) {
    // Print in hex
    auto element = GetElementFactory()->createElement(filter.name());
    if (!element) {
        // Unexists
        return nullptr;
    }
    // Previous filter
    auto cookie = &mFilters.emplace_back(filter);
    // Dynmaic add to if
    if (!d || !d->mVideoSink) {
        return cookie;
    }
    auto currentState = d->mPipeline->state();
    d->mPipeline->setState(State::Paused);

    auto pad = d->mVideoSink->findInput("sink");
    auto prevElement = pad->peerElement();
    auto prev = pad->peer();

    prev->unlink();
    LinkElements(prevElement, element, d->mVideoSink);


    // Configure it
    d->mPipeline->addElement(element);
    if (filter.mConfigure) {
        filter.mConfigure(*element);
    }
    element->setState(currentState);
    cookie->mElement = element.get();

    // Restore
    d->mPipeline->setState(currentState);
    return cookie;
}
void Player::removeFilter(void *_where) {
    Filter *where = static_cast<Filter *>(_where);
    if (where == nullptr) {
        return;
    }
    std::list<Filter>::iterator it;
    for (it = mFilters.begin(); it != mFilters.end(); it++) {
        if (&*it == where) {
            break;
        }
    }
    if (it == mFilters.end()) {
        return;
    }
    // Dynmaic remove if
    if (!d || !d->mVideoSink || !where->mElement) {
        mFilters.erase(it);
        return;
    }
    auto currentState = d->mPipeline->state();
    d->mPipeline->setState(State::Paused);

    // Configure it
    auto prevPad = where->mElement->findInput("sink");
    auto nextPad = where->mElement->findOutput("src");
    auto prevElement = prevPad->peerElement();
    auto nextElement = nextPad->peerElement();
    nextPad->unlink();
    prevPad->unlink();

    LinkElements(prevElement, nextElement);

    where->mElement->setState(State::Null);
    d->mPipeline->removeElement(where->mElement);

    // Restore
    d->mPipeline->setState(currentState);
    mFilters.erase(it);
}

void Player::setUrl(std::string_view url) {
    stop();
    mUrl = url;
}
void Player::setSubtitleUrl(std::string_view url) {
    mSubtitleUrl = url;
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
void Player::setLoops(int loops) {
    mLoops = loops;
}
void Player::setSubtitleStream(int idx) {
    if (!d || idx < 0 || idx >= d->mSubtitleStreams.size()) {
        return;
    }
    d->mSubtitleFilter->setSubtitle(idx);
}


void Player::stop() {
    mState = State::Null;
    delete mThread;
    d.reset();
    mThread = nullptr;

    // Reset all Filters elements info
    for (auto &filter : mFilters) {
        filter.mElement = nullptr;
    }
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
    if (d && d->mPipeline && (mState == State::Running || mState == State::Paused)) {
        // Only on Running / Pause, tell the pipeline set the position
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
    if (hasAudio()) {
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
    if (mRenderer && hasVideo()) {
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
    // Collect metadata
    _collectMetadata();

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

#ifndef NDEBUG
    fprintf(stderr, "%s\n", DumpContainer(d->mPipeline).c_str());
#endif
}
void Player::_buildAudioPart() {
    auto factory = GetElementFactory();
    d->mAudioQueue = factory->createElement<MediaQueue>();

    auto decoder = factory->createElement<Decoder>();
    auto converter = factory->createElement<AudioConverter>();
    d->mAudioSink = factory->createElement<AudioSink>();

    //< From Queue to sink
    auto err = d->mPipeline->addElements(d->mAudioQueue, decoder, converter, d->mAudioSink);
    if (err != Error::Ok) {
        return _error(err, "Fail to add audio elements");
    }
    err = LinkElements(d->mAudioQueue, decoder, converter, d->mAudioSink);
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
    d->mVideoSink = factory->createElement<VideoSink>();

    // From Queue to sink
    auto err = d->mPipeline->addElements(d->mVideoQueue, decoder, converter, d->mVideoSink);
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
    for (auto &filter : mFilters) {
        auto currentElement = factory->createElement(filter.mName);
        if (!currentElement) {
            return _error(Error::InvalidArguments, libc::asprintf("Fail to create filter '%s'", filter.mName));
        }
        if (filter.mConfigure) {
            filter.mConfigure(*currentElement);
        }
        // Store the element (for dyn add / remove)
        filter.mElement = currentElement.get();
        
        d->mPipeline->addElement(currentElement);
        LinkElements(prevElement, currentElement);
        prevElement = currentElement;
    }

    // Link the last element to renderer
    err = LinkElements(prevElement, d->mVideoSink);
    if (err != Error::Ok) {
        return _error(err, "Fail to link video elements");
    }

    d->mVideoSink->setRenderer(mRenderer);
    d->mVideoSink->setName("NekoVideoSink");
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
void Player::_collectMetadata() {
    if (d->mSubtitleFilter) {
        d->mSubtitleStreams = d->mSubtitleFilter->subtitles();
    }
    // int videoIndex = 0;
    // int audioIndex = 0;
    for (const auto& stream : d->mDemuxer->outputs()) {
        if (stream->name().starts_with("video")) {
            Property prop = stream->property(Properties::Metadata);
            if (prop.isMap()) {
                auto &mp = prop.toMap();
                d->mVideoStreams.emplace_back(mp.begin(), mp.end());
            }
            else {
                d->mVideoStreams.push_back({});
            }
        }
        else if (stream->name().starts_with("audio")) {
            Property prop = stream->property(Properties::Metadata);
            if (prop.isMap()) {
                auto &mp = prop.toMap();
                d->mAudioStreams.emplace_back(mp.begin(), mp.end());
            }
            else {
                d->mAudioStreams.push_back({});
            }
        }
    }
}
void Player::_translateEvent(View<Event> event) {
    switch (event->type()) {
        case Event::ClockUpdated: {
            if (mPositionCallback) {
                mPositionCallback(event.viewAs<ClockEvent>()->position());
            }
            constexpr auto v = __builtin_FILE();
            break;
        }
        case Event::ErrorOccurred: {
            auto err = event.viewAs<ErrorEvent>();
            _error(err->error(), err->message());
            break;
        }
        case Event::MediaEndOfFile: {
            if (mLoops < 0) {
                // INF Loops
                setPosition(0);
                break;
            }
            d->mtLoopsCount += 1;
            if (d->mtLoopsCount <= mLoops) {
                setPosition(0);
                break;
            }
            _setState(State::Null);
            break;
        }
    }
}
Vec<Properties> Player::audioStreams() const {
    if (d) {
        return d->mAudioStreams;
    }
    return {};
}
Vec<Properties> Player::videoStreams() const {
    if (d) {
        return d->mVideoStreams;
    }
    return {};
}
Vec<Properties> Player::subtitleStreams() const {
    if (d) {
        return d->mSubtitleStreams;
    }
    return {};
}

static char **_convertToTitleArray(Vec<Properties> &&vec) {
    char **ret = (char**) libc::malloc(sizeof(char *) * (vec.size() + 1));
    size_t i = 0;
    for (; i < vec.size(); i++) {
        auto title = vec[i][Properties::Title].toStringOr("");
        ret[i] = (char*) libc::malloc(sizeof(char) * (title.size() + 1));
        ::memcpy(ret[i], title.c_str(), title.size());
        ret[i][title.size()] = '\0';
    }
    ret[i] = nullptr;
    return ret;
}
char **Player::_audioStreamTitles() const {
    return _convertToTitleArray(audioStreams());
}
char **Player::_videoStreamTitles() const {
    return _convertToTitleArray(videoStreams());
}
char **Player::_subtitleStreamTitles() const {
    return _convertToTitleArray(subtitleStreams());
}

NEKO_NS_END