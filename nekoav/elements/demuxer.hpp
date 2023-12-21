#pragma once

#include "../elements.hpp"

NEKO_NS_BEGIN

class Demuxer : public Element {
public:
    /**
     * @brief Set the Media Url
     * 
     * @param url 
     */
    virtual void setUrl(std::string_view url) = 0;
    /**
     * @brief Set the Open Options
     * 
     * @param options 
     */
    virtual void setOptions(const Properties *options) = 0;
    /**
     * @brief Get the media duration
     * 
     * @return double 
     */
    virtual double duration() const = 0;
    /**
     * @brief Check the media is seekable
     * 
     * @return true 
     * @return false 
     */
    virtual bool isSeekable() const = 0;
    /**
     * @brief Get the media metadata
     * 
     * @return Properties 
     */
    virtual Properties metadata() const = 0;
};

NEKO_NS_END