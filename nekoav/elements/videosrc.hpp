#pragma once

#include "../elements.hpp"

NEKO_NS_BEGIN

/**
 * @brief Test Video Source will output RGBA data in 30FPS
 * 
 * 
 */
class TestVideoSource : public Element {
public:
    /**
     * @brief Set the Output Size object
     * 
     * @param width 
     * @param height 
     */
    virtual void setOutputSize(int width, int height) = 0;
};
class ScreenCapture : public Element {
    
};

NEKO_NS_END