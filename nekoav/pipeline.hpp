#pragma once

#include "container.hpp"
#include <functional>

NEKO_NS_BEGIN

/**
 * @brief Special streaming container for element
 * 
 */
class Pipeline : public Container {
public:
    /**
     * @brief Set the Event Callback
     * @warning This callback will be invoked at worker thread
     * 
     * @param cb The event callback
     */
    virtual void setEventCallback(std::function<void(View<Event> )> &&cb) = 0;
};

/**
 * @brief Create a Pipeline object
 * 
 * @return Arc<Pipeline> 
 */
extern Arc<Pipeline> NEKO_API CreatePipeline();

NEKO_NS_END