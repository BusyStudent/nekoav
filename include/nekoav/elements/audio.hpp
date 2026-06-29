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

};

class NEKOAV_API AudioSink final : public Element {
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