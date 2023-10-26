#pragma once

#include "../media.hpp"

NEKO_NS_BEGIN

class Anime4kFilter : public MediaElement {
public:
    enum Config : int {
        None, //< Disabled
        Auto,
    };

    virtual void setConfig(int algorithm) = 0;
private:

};


NEKO_NS_END