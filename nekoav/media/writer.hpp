#pragma once

#include "frame.hpp"

NEKO_NS_BEGIN

/**
 * @brief Write a media frame (video) to file, useful on debug
 * 
 * @param frame 
 * @param path 
 * @return Error Error::NoImpl on no implement
 */
extern Error NEKO_API WriteVideoFrameToFile(View<MediaFrame> frame, const char *path);

NEKO_NS_END