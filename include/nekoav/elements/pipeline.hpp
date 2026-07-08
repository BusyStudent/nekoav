#pragma once

#include <nekoav/elements/bin.hpp>
#include <nekoav/element.hpp>
#include <ilias/sync/mpsc.hpp>

namespace nekoav {

// MARK: Pipeline
/**
 * @brief The top level bin, manage the global resource
 * 
 */
class NEKOAV_API Pipeline final : public Bin {
public:
    using Ptr = std::shared_ptr<Pipeline>;

    Pipeline(std::string_view name = {});
    ~Pipeline();

    /**
     * @brief Read the message from the pipeline bus
     * @note This method can be called in any thread, MT-SAFE
     * 
     * @return Task<Message> 
     */
    auto readMessage() -> Task<Message>;

    /**
     * @brief Send an control event to the pipeline
     * 
     * @param event 
     * @return IoTask<void> 
     */
    auto sendEvent(Event event) -> IoTask<void> override;
private:
    // Collect clock before run
    auto onInitialize() -> IoTask<void> override;
    auto onRun() -> IoTask<void> override;
    auto onPause() -> IoTask<void> override;
    auto onStop() -> IoTask<void> override;
    auto onTeardown() -> IoTask<void> override;
    auto onTopologyChange() -> void override;
    auto onChildMessage(Message msg) -> void override;

    struct Impl;

    // All clocks in the pipeline, sort by priority
    std::vector<Clock::Ptr> mClocks;
    std::unique_ptr<Impl>   d;
friend class Element;
};

} // namespace nekoav