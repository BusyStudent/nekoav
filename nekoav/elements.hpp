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
class Context;
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
     * @brief Get the context of this element
     * 
     * @return Context* 
     */
    Context   * context() const noexcept;
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
     * @brief Set the Context object
     * 
     * @param newCtxt 
     * @return Error 
     */
    Error setContext(Context *newCtxt);
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
    void removeAllInputs();
    void removeAllOutputs();

    Pad *addInput(std::string_view name);
    Pad *addOutput(std::string_view name);

    virtual Error changeState(StateChange stateChange) = 0;
protected:
    Atomic<State> mState { State::Null };
private:
    Context      *mContext { nullptr };
    EventSink    *mBus { nullptr };
    std::string   mName;
    ElementFlags  mFlags { ElementFlags::None };
    Vec<Pad *>    mInputs;
    Vec<Pad *>    mOutputs;
};

/**
 * @brief Link elements from left to right (a, b, c) => (a -> b -> c)
 * 
 * @return Error 
 */
extern NEKO_API Error LinkElements(std::initializer_list<View<Element> > elements);
/**
 * @brief Link Element by name
 * 
 * @param src The src element view
 * @param srcPad The output pad name of src element
 * @param dst The dst element view
 * @param dstPad The input pad name of dst element
 * @return Error 
 */
extern NEKO_API Error LinkElement(View<Element> src, std::string_view srcPad, View<Element> dst, std::string_view dstPad);


// -- IMPL Here
inline State Element::state() const noexcept {
    return mState.load();
}
inline EventSink *Element::bus() const noexcept {
    return mBus;
}
inline Context   *Element::context() const noexcept {
    return mContext;
}
inline std::span<Pad*> Element::inputs() const {
    return {
        const_cast<Pad**>(mInputs.data()),
        mInputs.size()
    };
}
inline std::span<Pad*> Element::outputs() const {
    return {
        const_cast<Pad**>(mOutputs.data()),
        mOutputs.size()
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
    return LinkElements({std::forward<Args>(args)...});
}

NEKO_NS_END