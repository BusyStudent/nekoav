#pragma once

#include "../elements.hpp"

NEKO_NS_BEGIN

class Demuxer : public Element {
public:
    virtual void setSource(std::string_view url) = 0;
};

NEKO_NS_END