#pragma once

#include "../elements.hpp"

NEKO_NS_BEGIN

/**
 * @brief The decoder interface, decode MediaPacket to MediaFrame
 * 
 */
class Decoder : public Element {
public:
    enum HardwarePolicy : int {
        PreferHardware = 0,
        ForceSoftware  = 1,
        ForceHardware  = 2,

        Auto = PreferHardware,
    };

    /**
     * @brief Set the Hardware Policy object
     * 
     * @param policy 
     * @return Error 
     */
    virtual Error setHardwarePolicy(HardwarePolicy policy) = 0;
};

NEKO_NS_END