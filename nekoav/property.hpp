#pragma once

#include "defs.hpp"
#include <variant>
#include <string>
#include <vector>
#include <map>

NEKO_NS_BEGIN

/**
 * @brief Proprty system
 * 
 */
class NEKO_API Property {
public:
    using List = std::vector<Property>;
    using Map  = std::map<std::string, Property>;

    Property();
    Property(int64_t value);
    Property(double value);
    Property(bool value);
    Property(std::string_view value);
    Property(const Property &other);
    Property(Property &&other);
    ~Property();

    template <typename T,
              typename _Cond = std::enable_if_t<std::is_integral_v<T> >
    >
    Property(T value) : Property(static_cast<int64_t>(value)) { }
    template <typename T,
              typename _Cond = std::enable_if_t<std::is_floating_point_v<T> >,
              char = 0
    >
    Property(T value) : Property(static_cast<double>(value)) { }
    Property(const char *value) : Property(std::string_view(value)) { }

    std::string toDocoument() const;
    std::string toString() const;
    double      toDouble() const;
    bool        toBool() const;
    int64_t     toInt() const;
    const List &toList() const;
    const Map  &toMap() const;

    Property    clone() const {
        return Property(*this);
    }

    bool        isString() const;
    bool        isDouble() const;
    bool        isBool() const;
    bool        isInt() const;
    bool        isNull() const;
    bool        isList() const;
    bool        isMap() const;

    bool        compare(const Property &) const;
    bool        contains(const Property &) const;
    bool        containsKey(std::string_view key) const;

    void        push_back(Property &&prop);
    void        push_front(Property &&prop);

    Property& operator =(const Property &other);
    Property& operator =(Property &&other);
    Property& operator [](size_t n);
    Property& operator [](std::string_view idx);
    
    bool      operator ==(const Property &prop) const {
        return compare(prop);
    }
    bool      operator !=(const Property &prop) const {
        return !compare(prop);
    }

    static Property newList();
    static Property newMap();
private:
    std::variant<
        std::monostate,
        std::string,
        int64_t,
        double,
        bool,
        List,
        Map
    > mValue;
};

NEKO_NS_END