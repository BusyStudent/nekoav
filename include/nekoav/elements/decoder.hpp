#pragma once

#include <nekoav/element.hpp>
#include <nekoav/format.hpp>
#include <nekoav/caps.hpp>

namespace nekoav {

/**
 * @brief Decode the packet into a frame.
 * 
 */
class Decoder : public Element {
public:
    Decoder(std::string_view name = {});
    ~Decoder();
private:
    auto onPrepare() -> IoTask<void> override;
    auto onTeardown() -> IoTask<void> override;

    // Query / Event from Pad
    auto onPadPush(Pad &pad, Sample::Ptr sample) -> IoTask<void>;
    auto onPadQuery(Pad &pad, const Query &query) -> std::optional<Reply>;
    auto onPadEvent(Pad &pad, const Event &event) -> IoTask<void>;

    struct Impl;

    std::unique_ptr<Impl> d;
    Pad                  &mInput;
    Pad                  &mOutput;
};

} // namespace nekoav
