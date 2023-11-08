#pragma once

#include "elements.hpp"
#include <functional>

NEKO_NS_BEGIN

class Container : public Element {
public:
    /**
     * @brief Add the element, transfer the ownship to container
     * 
     * @param element 
     * @return Error 
     */
    virtual Error addElement(View<Element> element) = 0;
    /**
     * @brief Detach the element, transfer the ownship to user
     * 
     * @param element 
     * @return Error 
     */
    virtual Error detachElement(View<Element> element) = 0;
    /**
     * @brief For all element in this container
     * 
     * @param cb Iteration callback true on continue, false on stop
     * @return Error 
     */
    virtual Error forElement(const std::function<bool (View<Element>)> &cb) = 0;
};

NEKO_NS_END