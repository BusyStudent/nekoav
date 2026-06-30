#pragma once

#include <nekoav/elements/bin.hpp>
#include <nekoav/element.hpp>
#include <nekoav/sample.hpp>
#include <string>
#include <memory>

namespace nekoav {

// Forward declarations
class VideoRenderer;

/**
 * @brief The PlayBin used to play media files.
 * 
 */
class NEKOAV_API PlayBin final : public Bin {
public:
    PlayBin(std::string_view name = {});
    ~PlayBin();

    // Source
    auto setUrl(std::string_view url) -> void;

    // VideoSink
    auto setRenderer(std::shared_ptr<VideoRenderer> renderer) -> void;
private:
    auto onPrepare() -> IoTask<void> override;
    auto onStop() -> IoTask<void> override;

    struct Impl;
    std::unique_ptr<Impl> d;
    std::string           mUrl;
    std::shared_ptr<VideoRenderer> mRenderer;
};

} // namespace nekoav