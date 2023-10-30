#define _NEKO_SOURCE
#include "property.hpp"
#include <algorithm>

NEKO_NS_BEGIN

Property::Property() = default;
Property::~Property() = default;
Property::Property(const Property &) = default;
Property::Property(Property &&) = default;
Property& Property::operator=(const Property &) = default;
Property& Property::operator=(Property &&) = default;

Property::Property(std::initializer_list<Property> list) : mValue(List{list}) { }
Property::Property(std::string_view view) : mValue(std::string(view)) { }
Property::Property(int64_t value) : mValue(value) { }
Property::Property(double value) : mValue(value) { }
Property::Property(bool value) : mValue(value) { }

double Property::toDouble() const {
    return std::get<double>(mValue);
}
int64_t Property::toInt() const {
    return std::get<int64_t>(mValue);
}
std::string Property::toDocoument() const {
    if (isNull()) {
        return "null";
    }
    if (isString()) {
        return '"' + toString() + '"';
    }
    if (isBool()) {
        return toBool() ? "true" : "false";
    }
    if (isInt()) {
        return std::to_string(toInt());
    }
    if (isDouble()) {
        return std::to_string(toDouble());
    }
    std::string str;
    str += "{";
    if (isList()) {
        const auto &list = std::get<List>(mValue);
        for (const auto &v : list) {
            str += v.toDocoument();
            str += ", ";
        }
    }
    else if (isMap()) {
        const auto &map = std::get<Map>(mValue);
        for (const auto &v : map) {
            str += v.first;
            str += ": ";
            str += v.second.toDocoument();
            str += ", ";
        }
    }
    str.pop_back();
    str.pop_back();
    str += "}";
    return str;
}
std::string Property::toString() const {
    return std::get<std::string>(mValue);
}
bool Property::toBool() const {
    return std::get<bool>(mValue);
}
auto Property::toList() const -> const List & {
    return std::get<List>(mValue);
}
auto Property::toMap() const -> const Map & {
    return std::get<Map>(mValue);
}

Property Property::newList() {
    Property prop;
    prop.mValue = List();
    return prop;
}
Property Property::newMap() {
    Property prop;
    prop.mValue = Map();
    return prop;
}

bool Property::isString() const {
    return std::holds_alternative<std::string>(mValue);
}
bool Property::isInt() const {
    return std::holds_alternative<int64_t>(mValue);
}
bool Property::isDouble() const {
    return std::holds_alternative<double>(mValue);
}
bool Property::isNull() const {
    return std::holds_alternative<std::monostate>(mValue);
}
bool Property::isBool() const {
    return std::holds_alternative<bool>(mValue);
}
bool Property::isList() const {
    return std::holds_alternative<List>(mValue);
}
bool Property::isMap() const {
    return std::holds_alternative<Map>(mValue);
}

bool Property::compare(const Property &another) const {
    if (mValue.index() != another.mValue.index()) {
        return false;
    }
    if (isNull()) {
        return true;
    }
    if (isInt()) {
        return toInt() == another.toInt();
    }
    if (isDouble()) {
        return toDouble() == another.toDouble();
    }
    if (isString()) {
        return toString() == another.toString();
    }
    if (isBool()) {
        return toBool() == another.toBool();
    }
    if (isList()) {
        auto &vec = std::get<List>(mValue);
        auto &aVec = std::get<List>(another.mValue);
        return std::equal(vec.begin(), vec.end(), aVec.begin(), aVec.end());
    }
    if (isMap()) {
        auto &map = std::get<Map>(mValue);
        auto &aMap = std::get<Map>(another.mValue);
        return std::equal(map.begin(), map.end(), aMap.begin(), aMap.end());
    }
    return false;
}
bool Property::contains(const Property &another) const {
    if (!isList()) {
        return false;
    }
    auto &vec = std::get<List>(mValue);
    return std::find(vec.begin(), vec.end(), another) != vec.end();
}
bool Property::containsKey(std::string_view key) const {
    if (!isMap()) {
        return false;
    }
    auto &map = std::get<Map>(mValue);
    return map.find(std::string(key)) != map.end();
}

void Property::push_back(Property &&prop) {
    std::get<List>(mValue).emplace_back(std::move(prop));
}
void Property::push_front(Property &&prop) {
    auto &vec = std::get<List>(mValue);
    vec.insert(vec.begin(), std::move(prop));
}


Property &Property::operator [](size_t n) {
    return std::get<List>(mValue)[n];
}
Property &Property::operator [](std::string_view key) {
    return std::get<Map>(mValue)[std::string(key)];
}

NEKO_NS_END