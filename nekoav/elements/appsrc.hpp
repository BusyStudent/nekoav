#pragma once

#include "../elements.hpp"

NEKO_NS_BEGIN

class AppSource : public Element {
public:
    /**
     * @brief Push a data to it
     * 
     * @param resource 
     * @return Error 
     */
    virtual Error push(View<Resource> resource) = 0;
};

NEKO_NS_END