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
    virtual Error forElements(const std::function<bool (View<Element>)> &cb) = 0;
    /**
     * @brief Get Elements size in the container
     * 
     * @return size
     */
    virtual size_t size() = 0;
    /**
     * @brief Add this elements all to the container
     * 
     * @param elements 
     * @return Error 
     */
    inline  Error addElements(std::initializer_list<View<Element> > elements);
    /**
     * @brief Add this elements all to the container
     * 
     * @tparam Args 
     * @param args 
     * @return Error 
     */
    template <typename ...Args>
    inline  Error addElements(Args &&... args);
};

/**
 * @brief Create a Container object
 * 
 * @return Container
 */
extern NEKO_API Arc<Container> CreateContainer();
/**
 * @brief Dump the topology as mermaid graph stromg
 * 
 * @param container 
 * @return std::string 
 */
extern NEKO_API std::string DumpTopology(View<Container> container);
/**
 * @brief Check element in this container has cycle
 * 
 * @param container 
 * @return NEKO_API 
 */
extern NEKO_API bool        HasCycle(View<Container> container);


// --IMPL
inline Error Container::addElements(std::initializer_list<View<Element> > elements) {
    for (auto elem : elements) {
        if (auto err = addElement(elem); err != Error::Ok) {
            return err;
        }
    }
    return Error::Ok;
}
template <typename ...Args>
inline  Error Container::addElements(Args &&... args) {
    return addElements({args...});
}

NEKO_NS_END