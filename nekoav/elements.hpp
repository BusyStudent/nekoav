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
    DynamicInput  = 1 << 1,
    DynamicOutput = 1 << 2,
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
     * @brief Get the flags of this element
     * 
     * @return ElementFlags 
     */
    ElementFlags flags() const noexcept;
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
    /**
     * @brief Set the Flags object
     * 
     * @param flags 
     */
    void setFlags(ElementFlags flags);
    /**
     * @brief Sets the state flags for the Element to flags, without telling anyone

     * 
     * @param newState 
     */
    void overrideState(State newState);

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
    /**
     * @brief Add a pad, the ownship of the pad will be transferred
     * 
     * @param pad 
     */
    void addPad(Pad *pad);
    /**
     * @brief Remove a pad
     * 
     * @param pad 
     */
    void removePad(Pad *pad);
    /**
     * @brief Remove all inputs
     * 
     */
    void removeAllInputs();
    /**
     * @brief Remove all outputs
     * 
     */
    void removeAllOutputs();
    /**
     * @brief Create a new input pad with name, and return it, The pad's ownship belongs to the element
     * 
     * @param name 
     * @return Pad* 
     */
    Pad *addInput(std::string_view name);
    /**
     * @brief Create a new output pad with name, and return it, The pad's ownship belongs to the element
     * 
     * @param name 
     * @return Pad* 
     */
    Pad *addOutput(std::string_view name);

    virtual Error changeState(StateChange stateChange) = 0;
private:
    Atomic<State> mState { State::Null };
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
inline ElementFlags Element::flags() const noexcept {
    return mFlags;
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
inline void Element::setFlags(ElementFlags flags) {
    mFlags = flags;
}
inline void Element::overrideState(State newState) {
    mState = newState;
#ifdef __cpp_lib_atomic_wait
    mState.notify_all();
#endif
}

template <typename ...Args>
inline Error LinkElements(Args &&...args) {
    return LinkElements({std::forward<Args>(args)...});
}

NEKO_NS_END