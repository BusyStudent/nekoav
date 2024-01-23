#pragma once

#include "defs.hpp"
#include "enum.hpp"
#include <functional>
#include <string>
#include <list>

NEKO_NS_BEGIN

class VideoRenderer;
class PlayerPrivate;

/**
 * @brief A Filter description
 * 
 * @internal I donn't known why qt motified the sizeof(std::string), so we use plain malloc / free instead of std::string
 * 
 */
class Filter {
public:
    using Configure = std::function<void(Element &)>;
    
    Filter(std::string_view name) { _setName(name); }
    Filter(std::string_view name, Configure &&conf) : mConfigure(std::move(conf)) { _setName(name); }
    Filter(const std::type_info &info) : Filter(info.name()) { }
    Filter(const std::type_info &info, Configure &&conf) : Filter(info.name(), std::move(conf)) { }
    Filter(const Filter &other) : mConfigure(other.mConfigure) { _setName(other.mName); }
    Filter(Filter &&other) : mName(other.mName), mConfigure(std::move(other.mConfigure)) { other.mName = nullptr; }
    ~Filter() { _tidy(); }

    Filter &operator =(const Filter &other) {
        if (this != &other) {
            _setName(other.mName);
            mConfigure = other.mConfigure;
        }
        return *this;
    }
    Filter &operator =(Filter &&other) {
        if (this != &other) {
            _tidy();
            mName = other.mName;
            mConfigure = std::move(other.mConfigure);
            other.mName = nullptr;
        }
        return *this;
    }

    /**
     * @brief Get the create by name
     * 
     * @return std::string_view 
     */
    std::string_view name() const noexcept {
        return mName;
    }
    /**
     * @brief Get the configure callback
     * 
     * @return const Configure& 
     */
    const Configure &configure() const noexcept {
        return mConfigure;
    }
    /**
     * @brief Set the Configure callback object
     * 
     * @param conf 
     */
    void setConfigure(Configure &&conf) noexcept { mConfigure = std::move(conf); }
    /**
     * @brief Create filter by type
     * 
     * @tparam T 
     * @return Filter 
     */
    template <typename T>
    static Filter fromType() {
        return Filter(typeid(T));
    }
private:
    void _tidy() {
        if (mName) {
            libc::free(mName);
            mName = nullptr;
        }
    }
    void _setName(std::string_view name) {
        _tidy();
        if (name.empty()) {
            return;
        }
        mName = static_cast<char *>(libc::malloc(name.size() + 1));
        ::memcpy(mName, name.data(), name.size());
        mName[name.size()] = '\0';
    }

    char       *mName = nullptr; //< Filter name
    Configure   mConfigure; //< Callback to configure
    Element    *mElement = nullptr; //< Created elements
friend class Player;
};

/**
 * @brief Wrapper for easily use Pipeline to play Video
 * 
 */
class NEKO_API Player {
public:
    Player();
    Player(const Player &) = delete;
    ~Player();

    /**
     * @brief Add a filter
     * 
     * @param filter 
     * @return void * The marks to remove the filter (0 on add failure)
     */
    void *addFilter(const Filter &filter);
    /**
     * @brief Remove a filter
     * 
     * @param id 
     */
    void removeFilter(void *id);
    /**
     * @brief Set the Url object
     * 
     * @param url 
     */
    void setUrl(std::string_view url);
    /**
     * @brief Set the Url of reading subtitle (empty on auto select)
     * 
     * @param url 
     */
    void setSubtitleUrl(std::string_view url);
    /**
     * @brief Set the Options object for Open Url
     * 
     * @param options The pointer of options, nyllptr on cleaer
     */
    void setOptions(const Properties *options);
    /**
     * @brief Add a Option
     * 
     * @param key 
     * @param value 
     */
    void setOption(std::string_view key, std::string_view value);
    /**
     * @brief Set the Video Renderer object
     * 
     * @param renderer 
     */
    void setVideoRenderer(VideoRenderer *renderer);
    /**
     * @brief Set the Position 
     * 
     * @param position 
     */
    void setPosition(double position);
    /**
     * @brief Check current media has video stream
     * 
     * @return true 
     * @return false 
     */
    bool hasVideo() const noexcept;
    /**
     * @brief Check current media has audio stream
     * 
     * @return true 
     * @return false 
     */
    bool hasAudio() const noexcept;
    /**
     * @brief Check current media is seekable
     * 
     * @return true 
     * @return false 
     */
    bool isSeekable() const noexcept;
    /**
     * @brief Get duration of current media
     * 
     * @return double (in seconds)
     */
    double duration() const noexcept;
    /**
     * @brief Get current position
     * 
     * @return double (in seconds)
     */
    double position() const noexcept;
    /**
     * @brief Get current state
     * 
     * @return State 
     */
    State state() const noexcept;

    // Streams
    Vec<Properties> audioStreams() const;
    Vec<Properties> videoStreams() const;
    Vec<Properties> subtitleStreams() const;

    void setAudioStream(int index);
    void setVideoStream(int index);
    void setSubtitleStream(int index);

    void play();
    void pause();
    void stop();

    void setErrorCallback(std::function<void(Error, std::string_view)> &&callback);
    void setPositionCallback(std::function<void(double)> &&callback);
    void setStateChangedCallback(std::function<void(State)> &&callback);
private:
    void _run();
    void _load();
    void _error(Error err, std::string_view msg);
    void _setState(State state);
    void _translateEvent(View<Event>);
    void _buildAudioPart();
    void _buildVideoPart();
    bool _configureSubtitle();

    Box<PlayerPrivate> d;
    Box<Properties> mOptions; //< The options for open
    Atomic<State>  mState { State::Null };
    Thread        *mThread = nullptr; //< Work Thread
    VideoRenderer *mRenderer = nullptr;
    std::string    mUrl; //< The dest to 
    std::string    mSubtitleUrl; //< The Url of the subtitle

    std::list<Filter> mFilters; //< List of filter 

    // Callbacks
    std::function<void(Error, std::string_view)> mErrorCallback;
    std::function<void(double)>           mPositionCallback;
    std::function<void(State)>            mStateChangedCallback;
};

NEKO_NS_END