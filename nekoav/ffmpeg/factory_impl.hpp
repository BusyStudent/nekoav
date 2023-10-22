#pragma once

#include "../elements.hpp"
#include "factory.hpp"
#include <typeindex>
#include <map>


NEKO_NS_BEGIN

class FFElementFactory final : public ElementFactory {
public:
    Box<Element> createElement(const char *name) const override {
        auto iter = mMap.find(name);
        if (iter != mMap.end()) {
            return iter->second();
        }
        return nullptr;
    }
    Box<Element> createElement(const std::type_info &info) const override {
#if     defined(_MSC_VER)
        auto name = info.raw_name();
#else
        auto name = info.name();
#endif
        return createElement(name);
    }
    void registerElement(const std::type_info &info, Box<Element> (*func)()) {
#if     defined(_MSC_VER)
        auto name = info.raw_name();
#else
        auto name = info.name();
#endif
        mMap[name] = func;
    }
    void registerElement(std::string_view name, Box<Element> (*func)()) {
        mMap[name] = func;
    }
    template <typename InterfaceType, Box<InterfaceType> (*FN)()>
    void registerElement() {
        registerElement(typeid(InterfaceType), []() -> Box<Element> {
            return FN();
        });
    }

    template <typename InterfaceType, typename Type>
    void registerElement() {
        static_assert(std::is_base_of_v<InterfaceType, Type>);
        registerElement(typeid(InterfaceType), make<Type>);
    }
    template <typename Type>
    void registerElement(std::string_view name) {
        registerElement(name, make<Type>);
    }

    template <typename U>
    static Box<Element> make() {
        return std::make_unique<U>();
    }
private:
    std::map<std::string_view, Box<Element> (*)()> mMap;
};

NEKO_NS_END