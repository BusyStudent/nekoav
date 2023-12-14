#pragma once

#include "../elements.hpp"
#include <functional>

#ifndef NDEBUG
    #define NEKO_GET_LOCATION() std::source_location::current()
#else
    #define NEKO_GET_LOCATION() std::source_location()
#endif

NEKO_NS_BEGIN

namespace _abiv1 {

class ElementBasePrivate;
/**
 * @brief For Basic Element Dispatch
 * 
 */
class ElementDelegate {
public:
    // States
    virtual Error onInitialize() { return Error::Ok; }
    virtual Error onPrepare() { return Error::Ok; }
    virtual Error onRun() { return Error::Ok; }
    virtual Error onPause() { return Error::Ok; }
    virtual Error onStop() { return Error::Ok; }
    virtual Error onTeardown() { return Error::Ok; }

    // Event
    virtual Error onEvent(View<Event> event) { return Error::Ok; }
    virtual Error onSinkEvent(View<Pad> pad, View<Event> event) { return Error::NoImpl; }
    virtual Error onSourceEvent(View<Pad> pad, View<Event> event) { return Error::NoImpl; }
    virtual Error onSinkPush(View<Pad> pad, View<Resource> resource) { return Error::NoImpl; }
};
/**
 * @brief For Threading Element Dispatch
 * 
 */
class ThreadingElementDelegate : public ElementDelegate {
public:
    virtual Error onLoop() { return Error::NoImpl; }
};
/**
 * @brief For Threading customed Thread allocate
 * 
 */
class ThreadingElementDelegateEx : public ThreadingElementDelegate {
public:
    virtual Thread *allocThread() = 0;
    virtual void    freeThread(Thread *thread) = 0;
};

/**
 * @brief Helper class for impl Element
 * 
 */
class NEKO_API ElementBase {
public:
    enum Kind {
        NoThreading = 0,
        Threading   = 1,
        ThreadingEx = 2
    };
    ElementBase(ThreadingElementDelegateEx *delegate, Element *element) : ElementBase(delegate, element, ThreadingEx) {}
    ElementBase(ThreadingElementDelegate *delegate, Element *element) : ElementBase(delegate, element, Threading) {}
    ElementBase(ElementDelegate *delegate, Element *element) : ElementBase(delegate, element, NoThreading) {}
    ElementBase(ElementDelegate *delegate, Element *element, int kind);
    ElementBase(const ElementBase &) = delete;
    ~ElementBase();

    // Utils
    /**
     * @brief Push event to upstream (to all outputs pad)
     * 
     * @param event 
     * @return Error 
     */
    Error pushEventToUpstream(View<Event> event);
    /**
     * @brief Push event to downstream (to all inputs pad)
     * 
     * @param event 
     * @return Error 
     */
    Error pushEventToDownstream(View<Event> event);
    /**
     * @brief Push resource to
     * 
     * @param pad 
     * @param resource 
     * @return Error 
     */
    Error pushTo(View<Pad> pad, View<Resource> resource);
    /**
     * @brief Push event to
     * 
     * @param pad 
     * @param event 
     * @return Error 
     */
    Error pushEventTo(View<Pad> pad, View<Event> event);
    /**
     * @brief Raise a Error and send it to the Bus
     * 
     * @param errcode 
     * @param message 
     * @return Error 
     */
    Error raiseError(Error errcode, std::string_view message = { }, std::source_location loc = NEKO_GET_LOCATION());
    /**
     * @brief return true on Thread is asked to stop
     * 
     * @return true 
     * @return false 
     */
    bool stopRequested() const noexcept;
    /**
     * @brief Invoke Any callable at Element Thread， and wait it finished
     * 
     * @tparam Callable 
     * @tparam Args 
     * @param cb 
     * @param args 
     * @return std::invoke_result_t<Callable, Args...> 
     */
    template <typename Callable, typename ...Args>
    auto syncInvoke(Callable &&cb, Args &&...args) -> std::invoke_result_t<Callable, Args...>;
    /**
     * @brief Get the thread
     * 
     * @return Thread* 
     */
    Thread *thread() const noexcept;
    /**
     * @brief Check is at work thread
     * 
     * @return true 
     * @return false 
     */
    bool    isWorkThread() const noexcept;

    // --- Internal API for GetCommonImpl
    Error _changeState(StateChange change);
    Error _sendEvent(View<Event> event);
    Pad  *_polishPad(Pad *);
private:
    void  _run();
    void  _invoke(std::function<void()> &&cb);
    Error _dispatchChangeState(StateChange change);
    Error _onSinkEvent(Pad *pad, View<Event> event);
    Error _onSourceEvent(Pad *pad, View<Event> event);
    Error _onSinkPush(Pad *pad, View<Resource> resource);

    Box<ElementBasePrivate> d;
    ElementDelegate *mDelegate = nullptr;
    Element *mElement = nullptr;

    //< For Threading impl
    Thread  *mThread = nullptr;
    bool     mThreading = false;
    bool     mThreadingEx = false;
};

template <typename Callable, typename ...Args>
inline auto ElementBase::syncInvoke(Callable &&cb, Args &&...args) -> std::invoke_result_t<Callable, Args...> {
    using RetT = std::invoke_result_t<Callable, Args...>;

    if constexpr (std::is_same_v<RetT, void>) {
        return _invoke(std::bind(std::forward<Callable>(cb), std::forward<Args>(args)...));
    }
    else {
        RetT ret {};
        _invoke([&]() {
            ret = std::invoke(std::forward<Callable>(cb), std::forward<Args>(args)...);
        });
        return ret;
    }
}
inline bool ElementBase::stopRequested() const noexcept {
    NEKO_ASSERT(mThreading);
    return mElement->state() == State::Null;
}
inline Thread *ElementBase::thread() const noexcept {
    return mThread;
}

/**
 * @brief For shorter template name generated
 * 
 * @tparam V 
 */
template <int V>
class SelectDelegate;
template <>
class SelectDelegate<ElementBase::NoThreading> {
public:
    using type = ElementDelegate;
};
template <>
class SelectDelegate<ElementBase::Threading> {
public:
    using type = ThreadingElementDelegate;
};
template <>
class SelectDelegate<ElementBase::ThreadingEx> {
public:
    using type = ThreadingElementDelegateEx;
};

/**
 * @brief Generate Common Impl for interface Ts... 
 * 
 * @tparam Index (0, 1, 2) 
 * @tparam Ts 
 */
template <int Index, typename ...Ts> 
class _Impl : public Ts..., protected SelectDelegate<Index>::type {
public:    
    static constexpr bool _IsThreading = (Index > 0);

    _Impl() : mBase(this, this) {

    }
    _Impl(const _Impl &) = delete;
    ~_Impl() {
        NEKO_ASSERT(element()->state() == State::Null);
    }
protected:
    Error changeState(StateChange change) override {
        return mBase._changeState(change);
    }
    Error sendEvent(View<Event> event) override {
        return mBase._sendEvent(event);
    }
    Pad *addInput(std::string_view name) {
        auto pad = Element::addInput(name);
        return mBase._polishPad(pad);
    }
    Pad *addOutput(std::string_view name) {
        auto pad = Element::addOutput(name);
        return mBase._polishPad(pad);
    }
    /**
     * @brief Push event to upstream (to all outputs pad)
     * 
     * @param event 
     * @return Error 
     */
    Error pushEventToUpstream(View<Event> event) {
        return mBase.pushEventToUpstream(event);
    }
    /**
     * @brief Push event to downstream (to all inputs pad)
     * 
     * @param event 
     * @return Error 
     */
    Error pushEventToDownstream(View<Event> event) {
        return mBase.pushEventToDownstream(event);
    }
    /**
     * @brief Push the resource to the pad
     * 
     * @param pad 
     * @param resource 
     * @return Error 
     */
    Error pushTo(View<Pad> pad, View<Resource> resource) {
        return mBase.pushTo(pad, resource);
    }
    Error pushEventTo(View<Pad> pad, View<Event> event) {
        return mBase.pushEventTo(pad, event);
    }
    /**
     * @brief Raise a Error and send it to the Bus
     * 
     * @param errcode 
     * @param message 
     * @return Error 
     */
    Error raiseError(Error errcode, std::string_view message = { }, std::source_location loc = NEKO_GET_LOCATION()) {
        return mBase.raiseError(errcode, message, loc);
    }
    /**
     * @brief Check should we quit on the Loop ? (for threading element)
     * 
     * @return true 
     * @return false 
     */
    template <bool Threading = _IsThreading>
    bool stopRequested() const noexcept {
        static_assert(Threading, "It is only available for Threading element");
        return mBase.stopRequested();
    }
    /**
     * @brief Get the thread object
     * 
     * @return Thread* 
     */
    template <bool Threading = _IsThreading>
    Thread *thread() const noexcept {
        static_assert(Threading, "It is only available for Threading element");
        return mBase.thread();
    }
    /**
     * @brief Check is at work thread
     * 
     * @return true 
     * @return false 
     */
    template <bool Threading = _IsThreading>
    bool isWorkThread() const noexcept {
        static_assert(Threading, "It is only available for Threading element");
        return mBase.isWorkThread();
    }
    /**
     * @brief Invoke method at work thread
     * 
     * @warning Do not call this method at work thread
     * 
     * @tparam Callable 
     * @tparam Args 
     * @return auto 
     */
    template <typename Callable, typename ...Args, bool Threading = _IsThreading>
    auto invokeMethodQueued(Callable &&callable, Args &&...args) -> std::invoke_result_t<Callable, Args...> {
        static_assert(Threading, "It is only available for Threading element");

        NEKO_ASSERT(!isWorkThread());
        return mBase.syncInvoke(std::forward<Callable>(callable), std::forward<Args>(args)...);
    }
private:
    Element *element() noexcept {
        return this;
    }

    ElementBase mBase;
};

/**
 * @brief Normal Element Impl
 * 
 * @tparam Ts 
 */
template <typename ...Ts>
using Impl          = _Impl<ElementBase::NoThreading, Ts...>;
/**
 * @brief Threading Element Impl
 * 
 * @tparam Ts 
 */
template <typename ...Ts>
using ThreadingImpl = _Impl<ElementBase::Threading, Ts...>;
/**
 * @brief Threading Element Impl with custome Thread alloc
 * 
 * @tparam Ts 
 */
template <typename ...Ts>
using ThreadingExImpl = _Impl<ElementBase::ThreadingEx, Ts...>;

}

#ifndef NEKO_NO_DEFAULT_IMPL
using _abiv1::Impl;
using _abiv1::ThreadingImpl;
using _abiv1::ThreadingExImpl;
#endif

NEKO_NS_END