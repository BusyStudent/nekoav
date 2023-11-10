#pragma once

#include "container.hpp"
#include <functional>

NEKO_NS_BEGIN

class Pipeline : public Container {
public:
    /**
     * @brief Set the Event Callback
     * @details This callback will be invoked at worker thread
     * 
     * @param cb 
     */
    virtual void        setEventCallback(std::function<void(View<Event> )> &&cb) = 0;
};

extern Arc<Pipeline> NEKO_API CreatePipeline();

NEKO_NS_END