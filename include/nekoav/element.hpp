#pragma once

#include <nekoav/defines.hpp>
#include <nekoav/sample.hpp>
#include <nekoav/event.hpp>
#include <nekoav/query.hpp>
#include <nekoav/caps.hpp>
#include <optional>
#include <string>
#include <vector>
#include <array>
#include <list>
#include <bit>

namespace nekoav {

// Forward declare
class Element;

/**
 * @brief Represents the state of an Element.
 * 
 * @details This enum defines the lifecycle of an Element. The valid state transitions are as follows:
 * 
 *  - Null    -> Ready    (Initialize)
 *  - Ready   -> Paused   (Prepare)
 *  - Paused  -> Running  (Run)
 *  - Running -> Paused   (Pause)
 *  - Paused  -> Ready    (Stop)
 *  - Ready   -> Null     (Teardown)
 *  - Error   -> Null     (Teardown)
 * 
 *  An Element in the Error state can only transition back to Null.
 */
enum class State {
    Null    = 0,
    Ready   = 1,
    Paused  = 2,
    Running = 3,

    Error   = 0x0721, // Error state, the element is in an unrecoverable state. can't switch to another state, (except NUll)
};

enum class StateChange {
    Initialize = 1,
    Prepare    = 2,
    Run        = 3,
    Pause      = 4,
    Stop       = 5,
    Teardown   = 6,
};

/**
 * @brief The type of the Pad
 * 
 */
enum class PadType {
    Input,
    Output,
};

/**
 * @brief Represents a connection point on an Element for linking to other Elements.
 * 
 * Pads are used to establish the data flow pipeline. An output pad of one element
 * can be linked to an input pad of another.
 */
class NEKOAV_API Pad {
public:
    Pad(Element &element, PadType type, std::string_view name) : mElement(element), mType(type), mName(name) {}
    Pad(const Pad &) = delete;
    ~Pad() { unlink(); }

    // Get the name of the pad
    auto name() const -> std::string_view {
        return mName;
    }

    // Check the pad is linked?
    auto isLinked() const -> bool {
        return mPeer != nullptr;
    }

    // Unlink the pad to its peer.
    auto unlink() -> void {
        if (mPeer) {
            mPeer->mPeer = nullptr;
            mPeer = nullptr;
        }
    }

    /**
     * @brief Link this pad to a peer pad
     * 
     * @param peer 
     * @return true 
     * @return false 
     */
    auto link(Pad &peer) -> bool {
        if (isLinked() || peer.isLinked()) {
            return false;
        }
        if (mType == peer.mType) {
            return false;
        }
        mPeer = &peer;
        peer.mPeer = this;
        return true;
    }

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
     * @brief Get the mutable caps, only for the element
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
    auto push(Sample::Ptr sample) -> IoTask<void>;

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
        auto callable = [args...](Pad &self, Sample::Ptr sample) -> IoTask<void> {
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
    using PushCallback = auto (*)(Pad &self, Sample::Ptr sample) -> IoTask<void>;
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
        auto array = std::array<std::byte, sizeof(Callable)>{};
        ::memcpy(array.data(), data.data(), sizeof(Callable));
        return std::bit_cast<Callable>(array);
    }

    // Proxy for push callback
    template <typename Callable>
    static auto pushProxy(Pad &self, Sample::Ptr sample) -> IoTask<void> {
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
};

/**
 * @brief Base class for all media processing elements.
 * 
 */
class NEKOAV_API Element : public std::enable_shared_from_this<Element> {
public:
    using Ptr = std::shared_ptr<Element>;
    using PadList = std::list<Pad>;

    /**
     * @brief Construct a new Element object
     * 
     * @param name The name of the element (optional)
     */
    Element(std::string_view name = {});
    Element(const Element &) = delete;
    virtual ~Element();

    /**
     * @brief Dump the information of the element on the console (for debug)
     * 
     */
    auto dumpInfo(FILE *where = stderr) -> void { dumpInfoInternal(where, 0); };

    /**
     * @brief Asynchronously transitions the element to a new state.
     * 
     * The framework will automatically calculate the required intermediate state transitions.
     * For example, to transition from `Null` to `Paused`, it will automatically execute
     * the sequence `Null -> Ready -> Paused`.
     * 
     * Note: Users must transition the element to the `Null` state before destroying it
     * to ensure proper resource cleanup.
     * 
     * @param targetState The target state to transition to. (can't be State::Error)
     * @return IoTask<void> An asynchronous task that completes when the state transition is successful.
     *         On failure, the element's state is set to `Error`, and the task returns an error.
     */
    auto setState(State targetState) -> IoTask<void>;

    /**
     * @brief Set the new name of the element
     * 
     * @param name if empty, we will set an unique name of it
     */
    auto setName(std::string_view name) -> void;

    /**
     * @brief Get the input pad list
     * 
     * @return PadList & 
     */
    auto inputs() -> PadList & { return mInputs; }

    /**
     * @brief Get the output pad list
     * 
     * @return PadList & 
     */
    auto outputs() -> PadList & { return mOutputs; }

    /**
     * @brief Get the input pad list
     * 
     * @return PadList & 
     */
    auto inputs() const -> const PadList & { return mInputs; }

    /**
     * @brief Get the output pad list
     * 
     * @return PadList & 
     */
    auto outputs() const -> const PadList & { return mOutputs; }

    /**
     * @brief Get the current state of the element.
     * 
     * @return State 
     */
    auto state() const -> State { return mState; }

    /**
     * @brief Get the name of the element.
     * 
     * @return std::string_view 
     */
    auto name() const -> std::string_view { return mName; }

    // No copy
    auto operator =(const Element &) = delete;
protected:
    virtual auto dumpInfoInternal(FILE *where, int level) -> void;

    /**
     * @brief Doing the initialize
     * 
     * @return IoTask<void> 
     */
    virtual auto onInitialize() -> IoTask<void>;
    virtual auto onPrepare() -> IoTask<void>;
    virtual auto onRun() -> IoTask<void>;
    virtual auto onPause() -> IoTask<void>;
    virtual auto onStop() -> IoTask<void>;
    virtual auto onTeardown() -> IoTask<void>;

    /**
     * @brief Create and add an input pad
     * 
     * @param name The name of the pad
     * @return Pad &
     */
    auto createInputPad(std::string_view name) -> Pad &;

    /**
     * @brief Create a Output Pad object
     * 
     * @param name The name of the pad
     * @return Pad & 
     */
    auto createOutputPad(std::string_view name) -> Pad &;

    /**
     * @brief Set the element into error state, if bus is available, we will send an error event to the bus
     * 
     * @param errc The error code
     */
    auto setErrorState(std::error_code errc) -> void;
private:
    // Pads
    PadList mInputs;
    PadList mOutputs;

    // State / Parent
    State    mState = State::Null;
    Element *mParent = nullptr;

    // Name
    std::string mName;
friend class Bin;
};

/**
 * @brief The Bin element can contain multiple child elements and manage them as a single unit.
 * 
 */
class NEKOAV_API Bin : public Element {
public:
    using Ptr = std::shared_ptr<Bin>;

    Bin(std::string_view name = {});
    ~Bin();

    /**
     * @brief Add an element to the bin
     * 
     * @param element The shared_ptr of the element (if nullptr, no-op)
     */
    auto addElement(Element::Ptr element) -> void;

    /**
     * @brief Add an element to the bin and sync the new Element to the bin state
     * 
     * @param element The shared_ptr of the element (if nullptr, no-op)
     * @return IoTask<void> 
     */
    auto addElementSync(Element::Ptr element) -> IoTask<void>;

    /**
     * @brief Remove an element from the bin
     * 
     * @param element The shared_ptr of the element (if nullptr, no-op)
     * @return true We successfully removed the element
     * @return false Not removed (maybe the element is not in the bin)
     */
    auto removeElement(Element::Ptr element) -> bool;

    /**
     * @brief Check the bin is empty
     * 
     * @return true 
     * @return false 
     */
    auto empty() const -> bool { return mChildren.empty(); }
protected:
    // Sort, return false on Cycle detected
    auto topologicalSort() -> bool;
private:
    // Dump
    auto dumpInfoInternal(FILE *where, int level) -> void override;

    // State management
    auto onInitialize() -> IoTask<void> override;
    auto onPrepare() -> IoTask<void> override;
    auto onRun() -> IoTask<void> override;
    auto onPause() -> IoTask<void> override;
    auto onStop() -> IoTask<void> override;
    auto onTeardown() -> IoTask<void> override;

    auto setChildrenState(State newState) -> IoTask<void>;

    // Child elements
    std::vector<Element::Ptr> mChildren;
    bool                      mSorted = false;
};

// Utils function
/**
 * @brief Link two elements together via their pads.
 * 
 * @param src The source element
 * @param srcPad The name of the source pad
 * @param dst The destination element
 * @param dstPad The name of the destination pad
 * @return true Success
 * @return false Not linked (maybe the pad not found, or already linked)
 */
extern NEKOAV_API auto linkElement(Element &src, std::string_view srcPad, Element &dst, std::string_view dstPad) -> bool;

/**
 * @brief Get the string representation of a State enum value.
 * 
 * @param state 
 * @return std::string_view 
 */
extern NEKOAV_API auto toString(State state) -> std::string_view;

} // namespace nekoav