#pragma once

#include <nekoav/sample.hpp>
#include <nekoav/query.hpp>
#include <nekoav/caps.hpp>
#include <concepts>
#include <cstring> // std::memcpy
#include <string> // std::string_view
#include <memory> // std::shared_ptr
#include <array> // std::array
#include <new> // std::launder

namespace nekoav {
namespace detail {

template <typename T, size_t N>
class SmallFunc;

/**
 * @brief The small function wrapper, only store pod callable
 * 
 * @tparam R 
 * @tparam Args 
 * @tparam N 
 */
template <typename R, typename ...Args, size_t N> requires(N > 0)
class SmallFunc<R(Args...), N> {
public:
    SmallFunc(SmallFunc &&) = default;
    SmallFunc(std::nullptr_t) {}
    SmallFunc() = default;

    template <typename Fn> requires(std::is_invocable_v<Fn, Args...>)
    SmallFunc(Fn fn) noexcept {
        static_assert(sizeof(fn) <= N, "The function is too large");
        static_assert(std::is_trivially_copyable_v<Fn>, "The function is not trivially copyable");
        static_assert(std::is_trivially_destructible_v<Fn>, "The function is not trivially destructible");
        static_assert(alignof(Fn) <= alignof(decltype(mUser)), "The function is not aligned");
        new (mUser.data()) Fn{std::move(fn)}; // Put into the buffer
        mFn = &proxy<Fn>;
    }

    // Operator
    auto operator <=>(const SmallFunc &) const = default;
    auto operator =(SmallFunc &&) -> SmallFunc & = default;
    auto operator =(std::nullptr_t) -> SmallFunc & {
        mFn = nullptr;
        mUser.fill(std::byte{});
        return *this;
    }

    // Call
    template <typename ...Ts>
    auto operator ()(Ts &&...args) -> R {
        assert(mFn);
        return mFn(mUser.data(), std::forward<Ts>(args)...);
    }

    // Check the empty
    explicit operator bool() const noexcept { return mFn != nullptr; }
private:
    template <typename Fn>
    static auto proxy(std::byte *user, Args ...args) -> R {
        // Get fn
        auto fn = std::launder(reinterpret_cast<Fn *>(user));
        return (*fn)(std::forward<Args>(args)...);
    }

    R                      (*mFn)(std::byte *user, Args...) = nullptr;
    std::array<std::byte, N> mUser = {};
};

} // namespace detail


// Forward declare
class Element;
class PadLink;

/**
 * @brief The type of the Pad
 * 
 */
enum class PadType : uint8_t {
    Input,
    Output,
};

// MARK: Pad
/**
 * @brief Represents a connection point on an Element for linking to other Elements.
 * 
 * Pads are used to establish the data flow pipeline. An output pad of one element
 * can be linked to an input pad of another.
 */
class NEKOAV_API Pad final {
public:
    Pad(Element &element, PadType type, std::string_view name) : mElement(element), mType(type), mName(name) {}
    Pad(const Pad &) = delete;
    ~Pad();

    // Get the name of the pad
    auto name() const -> std::string_view {
        return mName;
    }

    // Get the type of the pad
    auto type() const -> PadType {
        return mType;
    }

    // Check the pad is linked?
    auto isLinked() const -> bool {
        return mPeer != nullptr;
    }

    // Check the pad is flushing?
    auto isFlushing() const -> bool;

    /**
     * @brief Unlink the peer pad, if the pad is already unlink (no-op, return true)
     * 
     * @return true Success
     * @return false Fail, (maybe you unlink the pad when the element still running?)
     */
    auto unlink() -> bool;

    /**
     * @brief Link this pad to a peer pad
     * 
     * @param peer 
     * @return true 
     * @return false 
     */
    auto link(Pad &peer) -> bool;

    /**
     * @brief Get the peer pad
     * 
     * @return Pad* 
     */
    auto peer() const -> Pad * {
        return mPeer;
    }

    /**
     * @brief Get the peer element
     * 
     * @return Element* 
     */
    auto peerElement() const -> Element * {
        return mPeer ? &mPeer->mElement : nullptr;
    }

    /**
     * @brief Get the caps
     * 
     * @return Caps& 
     */
    auto caps() const -> const Caps & {
        return mCaps;
    }

    /**
     * @brief Get the mutable caps
     * @note Only used it if you own the pad (like the element)
     * 
     * @return Caps& 
     */
    auto mutableCaps() -> Caps & {
        return mCaps;
    }

    /**
     * @brief Push the sample to the peer pad
     * @note This method is not `CANCELLATION SAFE`, the push element should use unstoppable to protected it
     * 
     * @param sample The shared_ptr of the Sample
     * @return IoTask<void> (Err on ublink or other element has error happened)
     */
    auto push(Sample sample) -> IoTask<void>;

    /**
     * @brief Push the event to the peer pad, 
     *  it will automatically go upstream or downstream by the type (input for upstream, output for downstream)
     * @note This method is not `CANCELLATION SAFE`, the push element should use unstoppable to protected it
     * 
     * @param event The event
     * @return IoTask<void> 
     */
    auto pushEvent(Event event) -> IoTask<void>;

    /**
     * @brief Send an sync query to the peer pad and wait for the reply, 
     *  it will automatically go upstream or downstream if element can't reply (input for upstream, output for downstream)
     * 
     * @param query 
     * @return std::optional<Reply>
     */
    auto sendQuery(Query query) -> std::optional<Reply>;

    /**
     * @brief Set the callbak when the pad is pushed
     * 
     * @tparam Method 
     * @tparam Object 
     * @tparam Args 
     * @param obj 
     * @param args 
     */
    template <auto Method, typename Object, typename ...Args>
    auto setPushCallback(Object *obj, Args ...args) -> void;

    /**
     * @brief Set the callback when the event happened
     * 
     * @tparam Method 
     * @tparam Object 
     * @tparam Args 
     */
    template <auto Method, typename Object, typename ...Args>
    auto setEventCallback(Object *obj, Args ...args) -> void;

    /**
     * @brief Set the callback when the pad is queried
     * 
     * @tparam Method 
     * @tparam Object 
     * @tparam Args 
     * @param obj 
     * @param args 
     */
    template <auto Method, typename Object, typename ...Args>
    auto setQueryCallback(Object *obj, Args ...args) -> void;

    /**
     * @brief Set the callback to nullptr, disable the callback
     * 
     */
    auto setPushCallback(std::nullptr_t) -> void;
    auto setEventCallback(std::nullptr_t) -> void;
    auto setQueryCallback(std::nullptr_t) -> void;
private:
    auto sendStickyEvents() -> IoTask<void>; // Sticky events are sent before any sample and event
    auto pushEventInternal(Event event) -> IoTask<void>; // It doesn't process the sticky...

    template <typename T>
    using Fn = detail::SmallFunc<T, sizeof(void*) * 3>;

    // The callback when the pad is pushed or event happened
    using QueryCallback = Fn<auto (Pad &self, Query query) -> std::optional<Reply> >;
    using EventCallback = Fn<auto (Pad &self, Event event) -> IoTask<void> >;
    using PushCallback = Fn<auto (Pad &self, Sample sample) -> IoTask<void> >;

    // Datas
    Element    &mElement; // The element this pad belongs to.
    PadType     mType;
    std::string mName;
    Pad        *mPeer = nullptr; // The peer pad this pad is linked to.
    Caps        mCaps;

    // Callbacks
    PushCallback mPushCallback;
    EventCallback mEventCallback;
    QueryCallback mQueryCallback;

    // Event
    std::vector<Event> mStickyEvents;
    bool mStickyEventsSent = false; // Did we sent the sticky events to the peer?, this value will be reset when unlinked

    // State of the linked
    std::shared_ptr<PadLink> mLink;
friend class Element;
};

// Impl
template <auto Method, typename Object, typename ...Args>
inline auto Pad::setPushCallback(Object *obj, Args ...args) -> void {
    static_assert(std::is_base_of_v<Element, Object>, "The obj must be a subclass of Element");
    assert(&mElement == obj && "The obj must be the element this pad belongs to");

    // Bind
    mPushCallback = [args...](Pad &self, Sample sample) -> IoTask<void> {
        auto &obj = static_cast<Object &>(self.mElement);
        return (obj.*Method)(self, std::move(sample), args...);
    };
}

template <auto Method, typename Object, typename ...Args>
inline auto Pad::setEventCallback(Object *obj, Args ...args) -> void {
    static_assert(std::is_base_of_v<Element, Object>, "The obj must be a subclass of Element");
    assert(&mElement == obj && "The obj must be the element this pad belongs to");

    // Bind
    mEventCallback = [args...](Pad &self, Event event) -> IoTask<void> {
        auto &obj = static_cast<Object &>(self.mElement);
        return (obj.*Method)(self, std::move(event), args...);
    };
}

template <auto Method, typename Object, typename ...Args>
inline auto Pad::setQueryCallback(Object *obj, Args ...args) -> void {
    static_assert(std::is_base_of_v<Element, Object>, "The obj must be a subclass of Element");
    assert(&mElement == obj && "The obj must be the element this pad belongs to");

    // Bind
    mQueryCallback = [args...](Pad &self, Query query) -> std::optional<Reply> {
        auto &obj = static_cast<Object &>(self.mElement);
        return (obj.*Method)(self, std::move(query), args...);
    };
}


} // namespace nekoav