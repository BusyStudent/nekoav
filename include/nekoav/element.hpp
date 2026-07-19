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

#include <nekoav/defines.hpp> 
#include <nekoav/message.hpp>
#include <nekoav/context.hpp>
#include <nekoav/event.hpp>
#include <nekoav/query.hpp>
#include <nekoav/clock.hpp>
#include <nekoav/pad.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/task.hpp>
#include <optional>
#include <string>
#include <vector>
#include <array>
#include <list>

namespace nekoav {

// Forward declare
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
enum class State : uint8_t {
    Null    = 0,
    Ready   = 1,
    Paused  = 2,
    Running = 3,
};

enum class StateChange : uint8_t {
    Initialize = 1,
    Prepare    = 2,
    Run        = 3,
    Pause      = 4,
    Stop       = 5,
    Teardown   = 6,
};

/**
 * @brief The type of the Element
 * 
 */
enum class ElementType : uint8_t {
    Source,    // The source, it has no input pad, 1 or more output pad
    Sink,      // The sink, it has 1 or more input pad, no output pad
    Transform, // The transform, it has 1 or more input pad, 1 or more output pad
    Bin,       // The bin, the pad is unspecified
    Other,     // The other, the pad are unspecified
};

// MARK: Element
// Core Elements: Element, Bin, Pipeline, Source, Sink, Transform
/**
 * @brief Base class for all media processing elements.
 * 
 */
class NEKOAV_API Element : public std::enable_shared_from_this<Element> {
public:
    using Ptr = std::shared_ptr<Element>;
    using PadList = std::list<Pad>;

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
     * Intermediate steps are applied automatically. Example: `Null -> Paused` runs
     * `Null -> Ready -> Paused`.
     *
     * Forward transitions (`Null` toward `Running`) are transactional with respect to the
     * call-time state (origin): each successful step is committed; if a later step fails,
     * reverse lifecycle hooks run until the element is back at origin, and the original
     * error is returned. Backward transitions are best-effort toward the target and do not
     * roll upward on failure.
     *
     * Users must reach `Null` before destroying the element so resources can be released.
     *
     * @param targetState The target state to transition to.
     * @return IoTask<void> Completes when the transition succeeds. On forward failure,
     *         `error()` holds the failure and state is the call-time origin (if rollback
     *         succeeded).
     */
    auto setState(State targetState) -> IoTask<void>;

    /**
     * @brief Set the Context of the element, it will set the context to all child elements
     * 
     * @param context The context to set
     */
    auto setContext(Context::Ptr context) -> void;

    /**
     * @brief Set the new name of the element
     * 
     * @param name if empty, we will set an unique name of it
     */
    auto setName(std::string_view name) -> void;

    /**
     * @brief Shutdown the element, equivalent to setState(State::Null)
     * 
     * @return IoTask<void> 
     */
    auto shutdown() -> IoTask<void> { return setState(State::Null); }

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
     * @return Context::Ptr (nullptr on not exist)
     */
    auto context() const -> const Context::Ptr & { return mContext; }

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

    // RTTI
    auto isPipeline() const -> bool { return mIsPipeline; }
    auto isBin() const -> bool { return mType == ElementType::Bin; }
    auto isSink() const -> bool { return mType == ElementType::Sink; }
    auto isSource() const -> bool { return mType == ElementType::Source; }
    auto isTransform() const -> bool { return mType == ElementType::Transform; }

    // No copy
    auto operator =(const Element &) = delete;
    auto operator =(Element &&) = delete;
protected:
    /**
     * @brief Construct a new Element object
     * 
     * @param name The name of the element (optional)
     */
    Element(ElementType type, std::string_view name = {});
    Element(std::string_view name = {}) : Element(ElementType::Other, name) {}

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
     * @brief Post an message to parent element
     * @note This method is MT safe, can be called from any thread
     * 
     * @param message 
     * @return 
     */
    auto postMessage(Message message) -> bool;

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

    // State / Parent / Clock
    State           mState = State::Null;
    ilias::Mutex    mStateMutex; // Mutex for setState
    std::error_code mError = {}; // If this is set, the element is in error

    // Filed used for topological
    Bin            *mParent = nullptr;
    Clock::Ptr      mClock = nullptr;
    Context::Ptr    mContext = nullptr;

    // avoid to use RTTI, use bool is faster
    ElementType     mType = ElementType::Other;
    bool            mIsPipeline = false;

    // Name
    std::string mName;

    // Pads
    PadList mInputs;
    PadList mOutputs;
friend class Pipeline;
friend class Bin;
friend class Pad;
};

// MARK:
/**
 * @brief Sink base element, sink is a element that has only input pad
 * 
 */
class NEKOAV_API Sink : public Element {
public:
    using Ptr = std::shared_ptr<Sink>;
protected:
    Sink(std::string_view name = {}) : Element(ElementType::Sink, name) {}
    ~Sink();
};

/**
 * @brief Source base element, source is a element that has only output pad
 * 
 */
class NEKOAV_API Source : public Element {
public:
    using Ptr = std::shared_ptr<Source>;
protected:
    Source(std::string_view name = {}) : Element(ElementType::Source, name) {}
    ~Source();
};

/**
 * @brief Transform base element, transform is a element that has both input and output pads, (processing element or not)
 * 
 */
class NEKOAV_API Transform : public Element {
public:
    using Ptr = std::shared_ptr<Transform>;
protected:
    Transform(std::string_view name = {}) : Element(ElementType::Transform, name) {}
    ~Transform();
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
 * @brief get the string representation of a ElementType enum value.
 * 
 * @param type 
 * @return std::string_view 
 */
extern NEKOAV_API auto toString(ElementType type) -> std::string_view;

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
NEKOAV_FORMATTER_4(nekoav::ElementType);