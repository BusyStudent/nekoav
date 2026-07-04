#pragma once

#include <nekoav/element.hpp>
#include <nekoav/format.hpp>
#include <nekoav/caps.hpp>

namespace nekoav {

/**
 * @brief The AudioContext class, wrapping miniaudio ma_context
 * 
 */
class AudioContext {
public:
    static constexpr std::string_view TypeId = "audioContext";

    virtual ~AudioContext() = default;
    virtual auto backend() const -> std::string_view = 0;

    /**
     * @brief Create the AudioContext
     * 
     * @return std::shared_ptr<AudioContext> 
     */
    NEKOAV_API
    static auto make() -> std::shared_ptr<AudioContext>;
};

/**
 * @brief The AudioSink class, it accept raw audio data and play it
 * 
 */
class NEKOAV_API AudioSink final : public Sink {
public:
    AudioSink(std::string_view name = {});
    ~AudioSink();

    auto sendQuery(Query query) -> std::optional<Reply> override;
private:
    auto onPrepare() -> IoTask<void> override;
    auto onPause() -> IoTask<void> override;
    auto onRun() -> IoTask<void> override;
    auto onStop() -> IoTask<void> override;
    auto onPush(Pad &, Sample sample) -> IoTask<void>;
    auto onEvent(Pad &, Event event) -> IoTask<void>;
    auto initDevice(AudioFrame *frame) -> IoResult<void>;

    struct Impl;
    std::unique_ptr<Impl> d;
    Pad                  &mInput;
};

} // namespace nekoav