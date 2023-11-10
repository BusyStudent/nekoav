#pragma once

#include "../elements.hpp"

NEKO_NS_BEGIN

class WavSource : public Element {
public:
    virtual void setUrl(std::string_view url) = 0;
};

NEKO_NS_END