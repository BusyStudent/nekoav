#define _NEKO_SOURCE
#include "factory.hpp"
#include <map>

NEKO_NS_BEGIN

class ElementFactoryImpl final : public ElementFactory {
public:
    Arc<Element> createElement(std::string_view name) const override {
        auto it = mMaps.find(name);
        if (it != mMaps.end()) {
            auto [_, create] = *it;
            return create();
        }
        return nullptr;
    }
    void         registerElement(std::string_view name, ElementCreator creator) {
        if (name.empty() || !creator) {
            return;
        }
        mMaps.insert(std::make_pair(name, creator));
    }
private:
    std::map<std::string_view, ElementCreator> mMaps;
};

ElementFactory *GetElementFactory() {
    static ElementFactoryImpl f;
    return &f;
}

NEKO_NS_END