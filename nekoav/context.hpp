#pragma once

#include "defs.hpp"
#include <shared_mutex>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include <map>

NEKO_NS_BEGIN

/**
 * @brief The container of Context objects
 * 
 */
class NEKO_API Context {
public:
    Context();
    Context(const Context &) = delete;
    ~Context();

    /**
     * @brief Adds an object to the context.
     * 
     * @details Note, This method doesnot take the ownship of the object by default.
     * @internal Maybe your should not use this method directly.
     *
     * @param idx the type index of the object
     * @param inf a pointer to the object
     * @param cleanup a callback for cleanup (set this function of you want to take the ownership of the object)
     *
     * @return true if the object was added successfully, false otherwise
     *
     * @throws None
     */
    bool  addObject(std::type_index idx, void *inf, std::function<void()> &&cleanup = { });
    /**
     * @brief Remove an object from the Context.
     * @internal Maybe your should not use this method directly.
     * 
     * @param idx the type index of the object to be removed
     * @param p a pointer to the object to be removed
     *
     * @return true if the object was successfully removed, false otherwise
     *
     * @throws None
     */
    bool  removeObject(std::type_index idx, void *inf);
    /**
     * @brief Queries an object of the specified type from the context.
     * @internal Maybe your should not use this method directly.
     * 
     * @param idx the type index of the object to query
     *
     * @return a pointer to the queried object, or nullptr if not found
     *
     * @throws None
     */
    void *queryObject(std::type_index idx) const;


    /**
     * @brief Adds an object to the context.
     *
     * @details Note, This method doesnot take the ownship of the object
     * 
     * @param inf a pointer to the object to be added
     *
     * @return true if the object was added successfully, false otherwise
     *
     * @throws None
     */
    template <typename T>
    bool  addObjectView(View<T> object) {
        return addObject(typeid(T), object.get());
    }
    /**
     * @brief Add an object to the context. it will take the ownership of the object
     * 
     * @tparam T The type of the object
     * @param object Arc<T> containing the object
     * @return true 
     * @return false 
     */
    template <typename T>
    bool  addObject(const Arc<T> &object) {
        return addObject(typeid(T), object.get(), [object]() {});
    }
    /**
     * @brief Add an object to the context. it will take the ownership of the object
     * 
     * @tparam T The type of the object
     * @param object Box<T> containing the object
     * @return true 
     * @return false 
     */
    template <typename T>
    bool  addObject(Box<T> &&object) {
        auto v = object.get();
        return addObject(typeid(T), v, [obj = std::move(object)]() {});
    }
    /**
     * @brief Remove an object of type T.
     *
     * @param inf a pointer to the object to be removed
     *
     * @return true if the object was successfully removed, false otherwise
     *
     * @throws None
     */
    template <typename T>
    bool removeObject(View<T> object) {
        return removeObject(typeid(T), object.get());
    }
    /**
     * @brief Retrieves a pointer to the specified type of object.
     *
     * @tparam T the type of object to retrieve
     *
     * @return a pointer to the specified type of object, or nullptr if the object does not exist
     *
     * @throws None
     */
    template <typename T>
    T  *queryObject() const {
        return static_cast<T*>(queryObject(typeid(T)));
    }
private:
    class Object {
    public:
        void *value;
        std::function<void()> cleanup;
    };

    std::map<std::type_index, Object> mObjects;
    mutable std::shared_mutex         mMutex;
};

NEKO_NS_END