#pragma once

#include <nekoav/sample.hpp>
#include <nekoav/query.hpp>
#include <nekoav/caps.hpp>
#include <ilias/sync/mutex.hpp>
#include <concepts>
#include <variant>
#include <string>
#include <memory>
#include <bit>

namespace nekoav {

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
        requires (std::is_base_of_v<Element, Object>)
    auto setPushCallback(Object *obj, Args ...args) -> void {
        assert(&mElement == obj && "The obj must be the element this pad belongs to");
        auto callable = [args...](Pad &self, Sample sample) -> IoTask<void> {
            auto &obj = static_cast<Object &>(self.mElement);
            return (obj.*Method)(self, std::move(sample), args...);
        };
        typeEraseTo(callable, mPushUser);
        mPushCallback = &Pad::pushProxy<decltype(callable)>;
    }

    /**
     * @brief Set the callback when the event happened
     * 
     * @tparam Method 
     * @tparam Object 
     * @tparam Args 
     */
    template <auto Method, typename Object, typename ...Args>
        requires (std::is_base_of_v<Element, Object>)
    auto setEventCallback(Object *obj, Args ...args) -> void {
        assert(&mElement == obj && "The obj must be the element this pad belongs to");
        auto callable = [args...](Pad &self, Event &event) -> IoTask<void> {
            auto &obj = static_cast<Object &>(self.mElement);
            return (obj.*Method)(self, event, args...);
        };
        typeEraseTo(callable, mEventUser);
        mEventCallback = &Pad::eventProxy<decltype(callable)>;
    }

    template <auto Method, typename Object, typename ...Args>
        requires (std::is_base_of_v<Element, Object>)
    auto setQueryCallback(Object *obj, Args ...args) -> void {
        assert(&mElement == obj && "The obj must be the element this pad belongs to");
        auto callable = [args...](Pad &self, Query &query) -> std::optional<Reply> {
            auto &obj = static_cast<Object &>(self.mElement);
            return (obj.*Method)(self, query, args...);
        };
        typeEraseTo(callable, mQueryUser);
        mQueryCallback = &Pad::queryProxy<decltype(callable)>;
    }

    /**
     * @brief Set the callback to nullptr, disable the callback
     * 
     */
    auto setPushCallback(std::nullptr_t) -> void {
        mPushCallback = nullptr;
        mPushUser.fill(std::byte{0});
    }

    auto setEventCallback(std::nullptr_t) -> void {
        mEventCallback = nullptr;
        mEventUser.fill(std::byte{0});
    }

    auto setQueryCallback(std::nullptr_t) -> void {
        mQueryCallback = nullptr;
        mQueryUser.fill(std::byte{0});
    }
private:
    // The callback when the pad is pushed or event happened
    using QueryCallback = auto (*)(Pad &self, Query &query) -> std::optional<Reply>;
    using EventCallback = auto (*)(Pad &self, Event &event) -> IoTask<void>;
    using PushCallback = auto (*)(Pad &self, Sample sample) -> IoTask<void>;
    using UserData = std::array<std::byte, sizeof(void*) * 3>; // Small size optimization for the callback

    // Type erase utils
    template <typename Callable>
    static auto typeEraseTo(const Callable &callable, UserData &array) -> void {
        static_assert(sizeof(callable) <= sizeof(UserData), "The callable is too large");
        static_assert(std::is_trivially_copyable_v<Callable>, "The callable must be trivially copyable");
        static_assert(std::is_trivially_destructible_v<Callable>, "The callable must be trivially destructible");
        ::memcpy(array.data(), &callable, sizeof(Callable));
    }

    template <typename Callable>
    static auto typeUnerase(const UserData &data) -> Callable {
        std::array<std::byte, sizeof(Callable)> array{};
        ::memcpy(array.data(), data.data(), sizeof(Callable));
        return std::bit_cast<Callable>(array);
    }

    // Proxy for push callback
    template <typename Callable>
    static auto pushProxy(Pad &self, Sample sample) -> IoTask<void> {
        auto callable = typeUnerase<Callable>(self.mPushUser);
        return callable(self, std::move(sample));
    }

    // Proxy for event callback
    template <typename Callable>
    static auto eventProxy(Pad &self, Event &event) -> IoTask<void> {
        auto callable = typeUnerase<Callable>(self.mEventUser);
        return callable(self, event);
    }

    template <typename Callable>
    static auto queryProxy(Pad &self, Query &query) -> std::optional<Reply> {
        auto callable = typeUnerase<Callable>(self.mQueryUser);
        return callable(self, query);
    }

    // Datas
    Element    &mElement; // The element this pad belongs to.
    PadType     mType;
    std::string mName;
    Pad        *mPeer = nullptr; // The peer pad this pad is linked to.
    Caps        mCaps;

    // Callbacks
    PushCallback mPushCallback = nullptr;
    UserData     mPushUser = {};

    EventCallback mEventCallback = nullptr;
    UserData      mEventUser = {};

    QueryCallback mQueryCallback = nullptr;
    UserData      mQueryUser = {};

    // State
    std::shared_ptr<PadLink> mLink;
};

} // namespace nekoav