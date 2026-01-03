#pragma once

#include <nekoav/element.hpp>
#include <nekoav/format.hpp>
#include <nekoav/caps.hpp>

namespace nekoav {

/**
 * @brief Decode the packet into a frame.
 * 
 */
class NEKOAV_API Decoder final : public Element {
public:
    Decoder(std::string_view name = {});
    ~Decoder();
private:
    struct Impl;

    auto onPrepare() -> IoTask<void> override;
    auto onTeardown() -> IoTask<void> override;

    // Query / Event from Pad
    auto onPadPush(Pad &pad, Sample sample) -> IoTask<void>;
    auto onPadQuery(Pad &pad, const Query &query) -> std::optional<Reply>;
    auto onPadEvent(Pad &pad, const Event &event) -> IoTask<void>;

    // Initialize the decoder
    auto init(const Caps &caps) -> IoTask<void>;
    auto open(Impl *inner) -> IoResult<void>;

    std::unique_ptr<Impl> d;
    Pad                  &mInput;
    Pad                  &mOutput;
};

} // namespace nekoav
