#pragma once

#include "property.hpp"
#include "error.hpp"
#include "flags.hpp"
#include "enum.hpp"
#include <typeinfo>
#include <string>
#include <vector>
#include <span>

NEKO_NS_BEGIN

class EventSink;
class Resource;
class Element;
class Event;
class Pad;

enum class ElementFlags : uint32_t {
    None          = 0,
    DynamicInput  = 1 << 0,
    DynamicOutput = 1 << 1,
};
NEKO_DECLARE_FLAGS(ElementFlags);

/**
 * @brief The smallest executable unit of the system
 * @details This is a interface of the minimum executable unit of the system. 
 * all code want to run in this system should inherit.
 * 
 */
class NEKO_API Element : public std::enable_shared_from_this<Element> {
public:
    virtual ~Element();

    /**
     * @brief Get the state object
     * 
     * @return State 
     */
    State       state() const noexcept;
    /**
     * @brief Get the buf sink of this element
     * 
     * @return EventSink* 
     */
    EventSink * bus() const noexcept;
    /**
     * @brief Get the name of this element
     * 
     * @return std::string 
     */
    std::string name() const;
    /**
     * @brief Get the inputs pad 
     * 
     * @return std::span<Pad> 
     */
    std::span<Pad*> inputs() const;
    /**
     * @brief Get the outputs pad
     * 
     * @return std::span<Pad> 
     */
    std::span<Pad*> outputs() const;
    /**
     * @brief Find the input pad by name
     * 
     * @param name 
     * @return Pad* 
     */
    Pad *findInput(std::string_view name) const;
    /**
     * @brief Find the output pad by name
     * 
     * @param name 
     * @return Pad* 
     */
    Pad *findOutput(std::string_view name) const;
    /**
     * @brief Set the Element output event sink (bus)
     * 
     * @param newBus 
     * @return Error 
     */
    Error setBus(EventSink *newBus);
    /**
     * @brief Set the Name object
     * 
     * @param name 
     */
    void  setName(std::string_view name);

    template <typename T>
    T       *as() noexcept;
    template <typename T>
    const T *as() const noexcept;

    /**
     * @brief Set the State object
     * 
     * @param newState 
     * @return Error 
     */
    virtual  Error setState(State newState);
    /**
     * @brief Send a event to the object
     * 
     * @param event 
     * @return Error 
     */
    virtual  Error sendEvent(View<Event> event) = 0;

    void *operator new(size_t size) {
        return libc::malloc(size);
    }
    void operator delete(void *ptr) {
        return libc::free(ptr);
    }
protected:
    Element();
    Element(const Element &) = delete;

    void addPad(Pad *pad);
    void removePad(Pad *pad);

    Pad *addInput(std::string_view name);
    Pad *addOutput(std::string_view name);

    virtual Error changeState(StateChange stateChange) = 0;
protected:
    Atomic<State> mState { State::Null };
private:
    EventSink    *mBus { nullptr };
    std::string   mName;
    ElementFlags  mFlags { ElementFlags::None };
    Vec<Pad *>    mInputs;
    Vec<Pad *>    mOutputs;
};

/**
 * @brief Link elements from left to right (a, b, c) => (a -> b -> c)
 * 
 * @return NEKO_API 
 */
extern NEKO_API Error LinkElements(std::span<View<Element> > elements);


// -- IMPL Here
inline State Element::state() const noexcept {
    return mState.load();
}
inline EventSink *Element::bus() const noexcept {
    return mBus;
}
inline std::span<Pad*> Element::inputs() const {
    return {
        const_cast<Pad**>(mInputs.data()),
        mInputs.size()
    };
}
inline std::span<Pad*> Element::outputs() const {
    return {
        const_cast<Pad**>(mInputs.data()),
        mInputs.size()
    };
}

template <typename T>
inline T *Element::as() noexcept {
    return dynamic_cast<T*>(this);
}
template <typename T>
inline const T *Element::as() const noexcept {
    return dynamic_cast<const T*>(this);
}


template <typename ...Args>
inline Error LinkElements(Args &&...args) {
    View<Element> elements [] {std::forward<Args>(args)...};
    return LinkElements(elements);
}

NEKO_NS_END