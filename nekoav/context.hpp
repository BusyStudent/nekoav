#pragma once

#include "defs.hpp"
#include <typeindex>
#include <typeinfo>
#include <mutex>
#include <map>

NEKO_NS_BEGIN

class NEKO_API Context {
public:
    Context();
    Context(const Context &) = delete;
    ~Context();

    void  registerInterface(std::type_index idx, void *inf);
    void  unregisterInterface(std::type_index idx);
    void *queryInterface(std::type_index idx) const;


    template <typename T>
    void  registerInterface(T *inf) {
        registerInterface(typeid(T), inf);
    }
    template <typename T>
    void unregisterInterface(T *inf) {
        unregisterInterface(typeid(T));
    }
    template <typename T>
    T  *queryInterface() const {
        return static_cast<T*>(queryInterface(typeid(T)));
    }
private:
    std::map<std::type_index, void *> mInterfaces;
    mutable std::mutex                mMutex;
};

NEKO_NS_END