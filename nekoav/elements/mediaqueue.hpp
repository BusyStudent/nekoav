#pragma once

#include "../elements.hpp"

NEKO_NS_BEGIN

class MediaQueue : public Element {
public:
    /**
     * @brief Get media frame durations at queue
     * 
     * @return double 
     */
    virtual double duration() const = 0;
    /**
     * @brief Set the Capacity object
     * 
     * @param capacity 
     */
    virtual void setCapacity(size_t capacity) = 0;
};

NEKO_NS_END