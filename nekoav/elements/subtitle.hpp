#pragma once

#include "../elements.hpp"

NEKO_NS_BEGIN

class SubtitleFilter : public Element {
public:
    /**
     * @brief Set the Url object of the subtitle, otherwise, the filter
     * 
     * @param subtitleUrl 
     */
    virtual void             setUrl(std::string_view subtitleUrl) = 0;
    /**
     * @brief Get the num of the subtitles title
     * 
     * @return Vec<std::string> 
     */
    virtual Vec<std::string> subtitles() = 0;
    /**
     * @brief Set the current Subtitle by index 
     * 
     * @param idx 
     */
    virtual void             setSubtitle(int idx) = 0;
};

NEKO_NS_END