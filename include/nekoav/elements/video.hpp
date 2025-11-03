#pragma once

#include <nekoav/element.hpp>
#include <nekoav/format.hpp>
#include <nekoav/caps.hpp>

namespace nekoav {

/**
 * @brief Decode the packet into a frame.
 * 
 */
class VideoDecoder : public Element {
public:
    VideoDecoder(std::string_view name = {});
    ~VideoDecoder();
private:
    struct Impl;

    std::unique_ptr<Impl> d;
};

/**
 * @brief Convert the frame's format to make it suitable for the next element.
 * 
 */
class VideoConverter : public Element {
    
};

/**
 * @brief Present the frame to the user. 
 * 
 */
class VideoSink : public Element {
    
};

} // namespace nekoav
