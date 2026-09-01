#pragma once

#include <nekoav/element.hpp>
#include <nekoav/sample.hpp>
#include <functional>
#include <string>

namespace nekoav {

/**
 * @brief The element read data from the url and demux it, output the packet from the pad, wrapping AVFormatContext
 * 
 * @note It wil load the media on Ready -> Pause (Prepare)
 */
class NEKOAV_API UrlSource final : public Source {
public:
    UrlSource(std::string_view name = {});
    ~UrlSource();

    /// @copydoc Element::sendEvent
    auto sendEvent(Event event) -> IoTask<void> override;

    /**
     * @brief Set the Url of the source
     * 
     * @param url The utf-8 url
     */
    auto setUrl(std::string_view url) -> void;

    /**
     * @brief Get the video output pads list
     * 
     * @return std::vector<Pad *> 
     */
    auto videoOutputs() -> std::vector<Pad *>;

    /**
     * @brief Get the audio output pads list
     * 
     * @return std::vector<Pad *> 
     */
    auto audioOutputs() -> std::vector<Pad *>;

    /**
     * @brief Get the subtitle output pads list
     * 
     * @return std::vector<Pad *> 
     */
    auto subtitleOutputs() -> std::vector<Pad *>;
private:
    // State
    auto onInitialize() -> IoTask<void> override;
    auto onTeardown() -> IoTask<void> override;

    auto onRun() -> IoTask<void> override;
    auto onPause() -> IoTask<void> override;

    auto onPrepare() -> IoTask<void> override;

    // Query / Event from Pad
    auto onPadQuery(Pad &pad, Query query) -> std::optional<Reply>;
    auto onPadEvent(Pad &pad, Event event) -> IoTask<void>;

    // FFmpeg
    auto doSeek() -> IoTask<void>;
    auto readWorker() -> Task<void>;
    auto interruptCallback() -> int;

    // Hidden context
    struct Impl;
    std::unique_ptr<Impl> d;

    // Configure...
    std::string   mUrl;
};

} // namespace nekoav