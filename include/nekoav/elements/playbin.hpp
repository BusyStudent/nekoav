#pragma once

#include <nekoav/element.hpp>
#include <nekoav/sample.hpp>
#include <string>

namespace nekoav {

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
private:
    auto onPrepare() -> IoTask<void> override;
    auto onStop() -> IoTask<void> override;

    struct Impl;
    std::unique_ptr<Impl> d;
    std::string           mUrl;
};

} // namespace nekoav