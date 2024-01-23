#pragma once

#include "defs.hpp"
#include "error.hpp"
#include "property.hpp"
#include <stdexcept>
#include <functional>

NEKO_NS_BEGIN

class NEKO_API Pad {
 public:
    enum Type {
        Input,
        Output,

        Sink = Input,
        Source = Output,
    };
    using Callback = std::function<Error(View<Resource> )>;
    using EventCallback = std::function<Error(View<Event> )>;

    Pad(Element *master, Type type, std::string_view name);
    Pad(const Pad &) = delete;
    virtual ~Pad();

    /**
     * @brief Link the pad with target pad
     * 
     * @param target 
     * @return Error 
     */
    Error link(View<Pad> target);
    /**
     * @brief Unlink the pad
     * 
     * @return Error 
     */
    Error unlink();
    /**
     * @brief Push a resource to the link
     * 
     * @param resource 
     * @return Error 
     */
    Error push(View<Resource> resource);
    /**
     * @brief Push a event to the link
     * 
     * @return Error 
     */
    Error pushEvent(View<Event> event);
    /**
     * @brief Set the resource arrived Callback object
     * 
     * @param callback 
     * @return Error 
     */
    void setCallback(Callback &&callback);
    /**
     * @brief Set the Event Callback object
     * 
     * @param callback 
     */
    void setEventCallback(EventCallback &&callback);
    /**
     * @brief Set the Name object
     * 
     * @param name 
     */
    void setName(std::string_view name);
    /**
     * @brief Get human readable string of this pad
     * 
     * @return std::string 
     */
    std::string toDocoument() const;
    /**
     * @brief Get the name of the pad
     * 
     * @return std::string_view 
     */
    std::string_view name() const noexcept;
    /**
     * @brief Get the master of the pad
     * 
     * @return Element* 
     */
    Element         *element() const noexcept;
    /**
     * @brief Get the master of the peer pad
     * 
     * @return Element* 
     */
    Element         *peerElement() const noexcept;
    /**
     * @brief Get next pad (valid for Output pad)
     * 
     * @return Pad* 
     */
    Pad             *next() const noexcept;
    /**
     * @brief Get prev pad (valid for Input pad)
     * 
     * @return Pad* 
     */
    Pad             *prev() const noexcept;
    /**
     * @brief Get the peer pad
     * 
     * @return Pad* 
     */
    Pad             *peer() const noexcept;
    /**
     * @brief Get the type of the pad
     * 
     * @return Type 
     */
    Type             type() const noexcept;
    /**
     * @brief Get the properties of the pad
     * 
     * @return Properties& 
     */
    Properties      &properties() noexcept;
    /**
     * @brief Get the properties of the pad
     * 
     * @return const Properties& 
     */
    const Properties& properties() const noexcept;
    /**
     * @brief Check the pad is linked
     * 
     * @return true 
     * @return false 
     */
    bool             isLinked() const noexcept;
    /**
     * @brief Find the property of this pad
     * 
     * @param name 
     * @return Property& 
     */
    Property        &property(std::string_view name);
    /**
     * @brief Find the property of this pad
     * 
     * @param name 
     * @return const Property& 
     */
    const Property  &property(std::string_view name) const;
    /**
     * @brief Check the pad has this property
     * 
     * @param name 
     * @return true 
     * @return false 
     */
    bool             hasProperty(std::string_view name) const noexcept;
    /**
     * @brief Add a property by name
     * 
     * @param name The name 
     * @param p The property
     */
    void             addProperty(std::string_view name, Property &&p);
    /**
     * @brief Remove the property by name
     * 
     * @param name The name
     * @return true 
     * @return false No property founded 
     */
    bool             removeProperty(std::string_view name);
    /**
     * @brief Clear all property
     * 
     */
    void             clearProperties();

    void *operator new(size_t size) {
        return libc::malloc(size);
    }
    void operator delete(void *ptr) {
        return libc::free(ptr);
    }
private:
    Element *mElement = nullptr;
    Type        mType;
    std::string mName;
    Pad        *mPrev = nullptr;
    Pad        *mNext = nullptr;
    Properties mProperties;
    Callback   mCallback;
    EventCallback mEventCallback;
};

class NEKO_API ProxyPad final : public Pad {

};


inline Pad::Type Pad::type() const noexcept {
    return mType;
}
inline Pad *Pad::next() const noexcept {
    return mNext;
}
inline Pad *Pad::prev() const noexcept {
    return mPrev;
}
inline Pad *Pad::peer() const noexcept {
    if (mType == Input) {
        return mPrev;
    }
    else {
        return mNext;
    }
}
inline Element *Pad::element() const noexcept {
    return mElement;
}
inline Element *Pad::peerElement() const noexcept {
    if (auto p = peer(); p) {
        return p->element();
    }
    return nullptr;
}
inline std::string_view Pad::name() const noexcept {
    return mName;
}
inline Properties &Pad::properties() noexcept {
    return mProperties;
}
inline const Properties &Pad::properties() const noexcept {
    return mProperties;
}
inline bool Pad::isLinked() const noexcept {
    return mNext != nullptr;
}
inline Property &Pad::property(std::string_view name) {
    auto it = mProperties.find(name);
    if (it != mProperties.end()) {
        return it->second;
    }
    return mProperties[std::string(name)];
}
inline const Property &Pad::property(std::string_view name) const {
    auto it = mProperties.find(name);
    if (it != mProperties.end()) {
        return it->second;
    }
    throw std::out_of_range("Pad::property");
}
inline bool Pad::hasProperty(std::string_view name) const noexcept {
    return mProperties.contains(name);
}
inline void Pad::addProperty(std::string_view name, Property &&property) {
    mProperties.emplace(std::string(name), std::move(property));
}
inline bool Pad::removeProperty(std::string_view name) {
    auto iter = mProperties.find(name);
    if (iter != mProperties.end()) {
        mProperties.erase(iter);
        return true;
    }
    return false;
}
inline void Pad::clearProperties() {
    mProperties.clear();
}


NEKO_NS_END