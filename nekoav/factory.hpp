#pragma once

#include "defs.hpp"
#include <type_traits>
#include <string>

#define NEKO_REGISTER_ELEMENT(interf, type)                                   \
    NEKO_CONSTRUCTOR(type##_ctor) {                                           \
        auto factory = NEKO_NAMESPACE::GetElementFactory();                   \
        factory->registerElement<interf, type>();                             \
        factory->registerElement<type>(#interf);                              \
    }

NEKO_NS_BEGIN

/**
 * @brief Event creation abstraction
 * 
 */
class ElementFactory {
public:
    typedef Arc<Element> (*ElementCreator)();
    virtual Arc<Element> createElement(std::string_view name) const = 0;
    virtual void         registerElement(std::string_view name, ElementCreator creator) = 0;
    
    template <typename Interface, typename Type>
    inline  void         registerElement() {
        static_assert(std::is_base_of<Interface, Type>::value, "Type must be derived from Interface");
        static_assert(std::is_abstract<Interface>::value, "Interface must be abstract");
        registerElement(typeid(Interface).name(), make<Type>);
    }
    template <typename T>
    inline  void         registerElement(std::string_view name) {
        registerElement(name, make<T>);
    }

    template <typename T>
    inline Arc<T>        createElement() const {
        return std::static_pointer_cast<T>(
            createElement(typeid(T).name())
        );
    }
protected:
    ElementFactory() = default;
    ~ElementFactory() = default;

    template <typename T>
    static Arc<Element> make() {
        return std::make_shared<T>();
    }
};

/**
 * @brief Get the Global Element Factory object
 * 
 * @return ElementFactory* 
 */
extern NEKO_API ElementFactory *GetElementFactory();
/**
 * @brief Parse the string and build the elements
 * 
 * @param graph 
 * @return NEKO_API 
 */
extern NEKO_API Arc<Element>    ParseGraph(ElementFactory *factory, std::string_view graph);

template <typename Interface>
inline Arc<Interface> CreateElement() {
    return GetElementFactory()->createElement<Interface>();
}
inline Arc<Element>   ParseGraph(std::string_view graph) {
    return ParseGraph(GetElementFactory(), graph);    
}

NEKO_NS_END