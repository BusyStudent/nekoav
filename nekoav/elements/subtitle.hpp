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
     * @brief Get the num of the subtitles metadata
     * 
     * @return Vec<std::string> 
     */
    virtual Vec<Properties>  subtitles() = 0;
    /**
     * @brief Set the current Subtitle by index 
     * 
     * @param idx 
     */
    virtual void             setSubtitle(int idx) = 0;
    /**
     * @brief Set the Default Font object for ass render
     * 
     * @param font 
     */
    virtual void             setFont(std::string_view font) = 0;
    /**
     * @brief Set the fallback font Family
     * 
     * @param family 
     */
    virtual void             setFamily(std::string_view family) = 0;

};

NEKO_NS_END