#pragma once

#include <nekoav/defines.hpp>
#include <functional>
#include <optional>
#include <concepts>
#include <memory>
#include <string>
#include <map>

namespace nekoav {

template <typename T>
concept Interface = requires(T &t) {
    T::TypeId;
};

/**
 * @brief The interface storage for depending injection
 * 
 */
class NEKOAV_API Context {
public:
    using Ptr = Context *;

    Context();
    Context(const Context &) = delete;
    ~Context();

    /**
     * @brief Find the specified interface
     * 
     * @tparam T 
     * @return std::shared_ptr<T> 
     */
    template <Interface T>
    auto find() const -> std::shared_ptr<T> {
        return std::static_pointer_cast<T>(findObject(T::TypeId));
    }

    /**
     * @brief Insert the specified interface
     * 
     * @tparam T 
     * @param object 
     */
    template <Interface T>
    auto insert(std::shared_ptr<T> object) -> void {
        insertObject(T::TypeId, object);
    }
private:
    auto findObject(std::string_view id) const -> std::shared_ptr<void>;
    auto insertObject(std::string_view id, std::shared_ptr<void> object) -> void;

    std::map<std::string, std::shared_ptr<void>, std::less<> > mSlots;
};

} // namespace nekoav