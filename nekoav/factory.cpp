#define _NEKO_SOURCE
#include "factory.hpp"
#include <cctype>
#include <map>

NEKO_NS_BEGIN

class CaseInsensitiveCompare {
public:
    bool operator()(std::string_view lhs, std::string_view rhs) const {
        return std::lexicographical_compare(
            lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
            [](char l, char r) {
                return std::tolower(l) < std::tolower(r);
            }
        );
    }
};

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
    std::map<std::string_view, ElementCreator, CaseInsensitiveCompare> mMaps;
};

ElementFactory *GetElementFactory() {
    static ElementFactoryImpl f;
    return &f;
}

NEKO_NS_END