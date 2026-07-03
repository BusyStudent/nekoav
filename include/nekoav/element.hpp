/**
 * @file element.hpp
 * @author BusyStudent (fyw90mc@gmail.com)
 * @brief The core element abstraction
 * @version 0.1
 * @date 2026-01-18
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include <nekoav/message.hpp>
#include <nekoav/defines.hpp>
#include <nekoav/context.hpp>
#include <nekoav/sample.hpp>
#include <nekoav/event.hpp>
#include <nekoav/query.hpp>
#include <nekoav/clock.hpp>
#include <nekoav/caps.hpp>
#include <ilias/sync/mpsc.hpp>
#include <ilias/task.hpp>
#include <optional>
#include <string>
#include <vector>
#include <array>
#include <list>
#include <bit>

namespace nekoav {

// Forward declare
class Pipeline;
class Element;
class Bin;

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
 * 
 */
enum class State {
    Null    = 0,
    Ready   = 1,
    Paused  = 2,
    Running = 3,
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
 * @brief The type of the Element
 * 
 */
enum class ElementType {
    Source,    // The source, it has no input pad, 1 or more output pad
    Sink,      // The sink, it has 1 or more input pad, no output pad
    Transform, // The transform, it has 1 or more input pad, 1 or more output pad
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
    ~Pad() { unlink(); }

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

    // Unlink the pad to its peer.
    auto unlink() -> void;

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
        auto array = std::array<std::byte, sizeof(Callable)>{};
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
};

// MARK: Element
/**
 * @brief Base class for all media processing elements.
 * 
 */
class NEKOAV_API Element : public std::enable_shared_from_this<Element> {
public:
    using Ptr = std::shared_ptr<Element>;
    using PadList = std::list<Pad>;

    Element(const Element &) = delete;
    Element(Element &&) = delete;
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
     * @param targetState The target state to transition to.
     * @return IoTask<void> An asynchronous task that completes when the state transition is successful.
     *         On failure, the element's error field is set, and the task returns an error.
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
     * @brief Get the error field
     * 
     * @return std::error_code
     */
    auto error() const -> std::error_code { return mError; };

    /**
     * @brief Check the element is in error status
     * 
     * @return true 
     * @return false 
     */
    auto hasError() const -> bool { return static_cast<bool>(mError); }

    /**
     * @brief Get the name of the element.
     * 
     * @return std::string_view 
     */
    auto name() const -> std::string_view { return mName; }

    /**
     * @brief Get the parent bin
     * 
     * @return Bin * 
     */
    auto parent() const -> Bin * { return mParent; }

    /**
     * @brief Get the clock used for syncronization
     * 
     * @return Clock::Ptr (nullptr on not in pipeline) 
     */
    auto clock() const -> const Clock::Ptr & { return mClock; }

    /**
     * @brief Get the pipeline context for query / set interface
     * 
     * @return Context * 
     */
    auto context() const -> Context *;

    /**
     * @brief Send an sync query to the element
     * 
     * @param query 
     * @return std::optional<Reply> 
     */
    virtual auto sendQuery(Query query) -> std::optional<Reply>;

    /**
     * @brief Send an async event to the element
     * 
     * @param event 
     * @return IoTask<void> 
     */
    virtual auto sendEvent(Event event) -> IoTask<void>;

    // No copy
    auto operator =(const Element &) = delete;
    auto operator =(Element &&) = delete;
protected:
    /**
     * @brief Construct a new Element object
     * 
     * @param name The name of the element (optional)
     */
    Element(std::string_view name = {});

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

    /**
     * @brief Get the bus object
     * 
     * @return The bus used to send message to pipeline 
     */
    auto pipelineBus() -> ilias::mpsc::Sender<Message> & { return mPipelineBus; }

    /**
     * @brief Forward the control event to downstream or upstream
     * 
     * @code 
     * auto MyElement::onPadPush(Pad &pad, Event event) -> IoTask<void> {
     *    co_return co_await forwardEvent(pad, std::move(event));
     * }
     * @endcode 
     * 
     * @param pad Where did the event come from
     * @param event The event to be forwarded
     * @return IoTask<void> 
     */
    auto forwardEvent(Pad &pad, Event event) -> IoTask<void>;
private:
    // Interface for pipeline and bin to use
    /**
     * @brief Set new clock for the element, it will broadcast this element to all children if self is `bin`
     * 
     * @param clock The new clock to be set
     */
    auto setClock(Clock::Ptr clock) -> void;

    /**
     * @brief Set the Pipeline Bus that send an message to pipeline
     * 
     * @param bus 
     */
    auto setPipelineBus(ilias::mpsc::Sender<Message> bus) -> void;

    // Pads
    PadList mInputs;
    PadList mOutputs;

    // State / Parent / Clock
    State           mState = State::Null;
    bool            mStateChanging = false;
    std::error_code mError = {}; // If this is set, the element is in error

    // Filed used for topological
    Bin            *mParent = nullptr;
    Clock::Ptr      mClock = nullptr;
    ilias::mpsc::Sender<Message> mPipelineBus {};

    // avoid to use RTTI, use bool is faster
    bool            mIsPipeline = false;
    bool            mIsBin = false;

    // Name
    std::string mName;
friend class Pipeline;
friend class Bin;
friend class Pad;
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
 * @brief Link two elements together via their pads.
 * @note This function will euqual to linkElement(src, "out", dst, "in")
 * 
 * @param src The source element
 * @param dst The destination element
 * @return true 
 * @return false 
 */
extern NEKOAV_API auto linkElement(Element &src, Element &dst) -> bool;

/**
 * @brief Get the string representation of a State enum value.
 * 
 * @param state 
 * @return std::string_view 
 */
extern NEKOAV_API auto toString(State state) -> std::string_view;

/**
 * @brief Link the elements in the chain together.
 * 
 * @param ...elements (must be at least 2 elements)
 * @return true 
 * @return false 
 */
template <typename First, typename Second, typename... Rest> 
    requires(std::is_base_of_v<Element, First> && std::is_base_of_v<Element, Second>)
inline bool linkChain(First &first, Second &second, Rest&... rest) {
    if (!linkElement(first, second)) {
        return false;
    }
    if constexpr (sizeof...(rest) > 0) {
        return linkChain(second, rest...);        
    }
    return true;
}

} // namespace nekoav

// Formatter
NEKOAV_FORMATTER_4(nekoav::State);