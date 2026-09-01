#pragma once

#include <nekoav/elements/bin.hpp>
#include <nekoav/element.hpp>
#include <atomic> // std::atomic

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
     * @brief Send an control event to the pipeline
     * 
     * @param event 
     * @return IoTask<void> 
     */
    auto sendEvent(Event event) -> IoTask<void> override;

    /**
     * @brief Read the message from the pipeline bus
     * @note This method can be called in any thread, MT-SAFE
     * 
     * @return Task<Message> 
     */
    auto readMessage() -> Task<Message>;

    /**
     * @brief Get the current clock position of the pipeline
     * @note This method can be called in any thread, MT-SAFE
     * 
     * @return std::optional<Timestamp> (nullptr on non clock exist)
     */
    auto position() const -> std::optional<Timestamp>;
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
    std::atomic<Clock::Ptr> mMasterClock; // For MT-Safe position()
    std::unique_ptr<Impl>   d;
};

} // namespace nekoav