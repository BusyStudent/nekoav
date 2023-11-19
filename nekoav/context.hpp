#pragma once

#include "defs.hpp"
#include <shared_mutex>
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
     * @details Note, This method doesnot take the ownship of the object
     *
     * @param idx the type index of the object
     * @param inf a pointer to the object
     *
     * @return true if the object was added successfully, false otherwise
     *
     * @throws None
     */
    bool  addObject(std::type_index idx, void *inf);
    /**
     * @brief Remove an object from the Context.
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
    bool  addObject(View<T> object) {
        return addObject(typeid(T), object.get());
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
    std::map<std::type_index, void *> mObjects;
    mutable std::shared_mutex         mMutex;
};

NEKO_NS_END