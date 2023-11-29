#pragma once

#include "../elements.hpp"
#include <source_location>
#include <functional>

#ifndef NDEBUG
    #define NEKO_GET_LOCATION() std::source_location::current()
#else
    #define NEKO_GET_LOCATION() std::source_location()
#endif

NEKO_NS_BEGIN

namespace _abiv1 {

class ElementBasePrivate;
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
    virtual Error onSinkEvent(Pad *pad, View<Event> event) { return Error::NoImpl; }
    virtual Error onSourceEvent(Pad *pad, View<Event> event) { return Error::NoImpl; }
    virtual Error onSinkPushed(Pad *pad, View<Resource> resource) { return Error::NoImpl; }
};
class ThreadingElementDelegate : public ElementDelegate {
public:
    virtual Error onLoop() { return Error::NoImpl; }
};

/**
 * @brief Helper class for impl Element
 * 
 */
class NEKO_API ElementBase {
public:
    enum {
        NoThreading = false,
        Threading = true,
    };

    ElementBase(ThreadingElementDelegate *delegate) : ElementBase(delegate, Threading) {}
    ElementBase(ElementDelegate *delegate) : ElementBase(delegate, NoThreading) {}
    ElementBase(ElementDelegate *delegate, bool threading);
    ElementBase(const ElementBase &) = delete;
    ~ElementBase();

    // Utils
    /**
     * @brief Send event to upstream (to all outputs pad)
     * 
     * @param event 
     * @return Error 
     */
    Error sendEventToUpstream(View<Event> event);
    /**
     * @brief Send event to downstream (to all inputs pad)
     * 
     * @param event 
     * @return Error 
     */
    Error sendEventToDownstream(View<Event> event);
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
    Error _onSinkPushed(Pad *pad, View<Resource> resource);

    Box<ElementBasePrivate> d;
    ElementDelegate *mDelegate = nullptr;
    Element *mElement = nullptr;

    //< For Threading impl
    Thread  *mThread = nullptr;
    bool     mThreading = false;
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
 * @brief Generate Common Impl for interface Ts... 
 * 
 * @tparam Threading 
 * @tparam Ts 
 */
template <bool Threading, typename ...Ts> 
class _Impl : public Ts..., public std::conditional_t<Threading, ThreadingElementDelegate, ElementDelegate> {
public:    
    _Impl() : mBase(this) {

    }
    _Impl(const _Impl &) = delete;
    ~_Impl() {
        NEKO_ASSERT(this->state() == State::Null);
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
     * @brief Send event to upstream (to all outputs pad)
     * 
     * @param event 
     * @return Error 
     */
    Error sendEventToUpstream(View<Event> event) {
        return mBase.sendEventToUpstream(event);
    }
    /**
     * @brief Send event to downstream (to all inputs pad)
     * 
     * @param event 
     * @return Error 
     */
    Error sendEventToDownstream(View<Event> event) {
        return mBase.sendEventToDownstream(event);
    }
    /**
     * @brief Raise a Error and send it to the Bus
     * 
     * @param errcode 
     * @param message 
     * @return Error 
     */
    Error raiseError(Error errcode, std::string_view message = { }, std::source_location loc = NEKO_GET_LOCATION()) {
        return mBase.raiseError(errcode, message);
    }
    /**
     * @brief Check should we quit on the Loop ? (for threading element)
     * 
     * @return true 
     * @return false 
     */
    template <bool _IsThreading = Threading>
    bool stopRequested() const noexcept {
        static_assert(_IsThreading, "It is only available for Threading element");
        return mBase.stopRequested();
    }
    template <bool _IsThreading = Threading>
    Thread *thread() const noexcept {
        static_assert(_IsThreading, "It is only available for Threading element");
        return mBase.thread();
    }
private:
    ElementBase mBase;
};

template <typename ...Ts>
using Impl          = _Impl<false, Ts...>;
template <typename ...Ts>
using ThreadingImpl = _Impl<true, Ts...>;

}

NEKO_NS_END