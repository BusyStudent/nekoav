#pragma once

#include <nekoav/element.hpp>
#include <nekoav/sample.hpp>
#include <optional>

namespace nekoav {

/**
 * @brief The queue, it will receive the data from in pads and push it to the out pad
 * 
 */
class NEKOAV_API Queue final : public Element {
public:
    Queue(std::string_view name = {});
    ~Queue();
private:
    auto onRun() -> IoTask<void> override;
    auto onPause() -> IoTask<void> override;
    auto onPush(Pad &, Sample::Ptr sample) -> IoTask<void>;
    auto doPull() -> Task<void>;

    // Internal...
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace nekoav