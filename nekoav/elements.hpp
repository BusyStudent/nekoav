#pragma once

#include "resource.hpp"
#include "property.hpp"
#include <typeindex>
#include <typeinfo>
#include <string>
#include <vector>
#include <mutex>
#include <list>
#include <map>

NEKO_NS_BEGIN

class PipelineImpl;
class Pipeline;
class Resource;
class Element;
class Thread;
class Graph;
class Pad;

enum class Error : int {
    Ok = 0,              //< No Error
    NoConnection,        //< Pad is unlinked
    NoImpl,              //< User doesnot impl it
    NoStream,            //< No Media Strean founded
    NoCodec,             //< Mo Media Codec founded
    UnsupportedFormat,   //< Unsupported media format
    UnsupportedResource, //< Unsupported Resource type 
    InvalidArguments,
    InvalidTopology,
    InvalidState,
    OutOfMemory,

    Unknown,
    NumberOfErrors, //< Numbers of Error code
};
enum class State {
    Stopped,  //< Default state
    Ready,    //< Init Compeleted
    Paused,   //< Paused
    Running,  //< Data is streaming
    Error,
};
enum class ThreadPolicy {
    AnyThread,    //< It can run on any thread
    SingleThread, //< It require a new thread
};


/**
 * @brief The smallest executable unit of the system
 * @details This is a interface of the minimum executable unit of the system. 
 * all code want to run in this system should inherit.
 * 
 */
class NEKO_API Element {
public:
    ~Element();

    /**
     * @brief Get the state
     * 
     * @return State 
     */
    auto state() const -> State {
        return mState.load();
    }
    /**
     * @brief Set the State
     * 
     * @param state 
     */
    auto setState(State state) -> void;
    /**
     * @brief Set the Thread Policy object
     * 
     * @param policy 
     */
    auto setThreadPolicy(ThreadPolicy policy) noexcept -> void {
        mThreadPolicy = policy;
    }
    auto setBus(Bus *bus) noexcept -> void {
        mBus = bus;
    }
    /**
     * @brief Get the thread policy
     * 
     * @return ThreadPolicy 
     */
    auto threadPolicy() const -> ThreadPolicy {
        return mThreadPolicy;
    }
    /**
     * @brief Set the Thread object
     * 
     * @param thread 
     */
    auto setThread(Thread *thread) -> void {
        mWorkthread = thread;
    }
    auto setGraph(Graph *graph) -> void {
        mGraph = graph;
    }

    auto &inputs() noexcept {
        return mInputPads;
    }
    auto &outputs() noexcept {
        return mOutputPads;
    }
    auto &inputs() const noexcept {
        return mInputPads;
    }
    auto &outputs() const noexcept {
        return mOutputPads;
    }
    auto  graph() const noexcept {
        return mGraph;
    }
    auto  bus() const noexcept {
        return mBus;
    }
    /**
     * @brief Link the output pad to target pad
     * 
     * @param source The output pad name in self
     * @param sink The dst element 
     * @param sinkName The target input pad
     * @return true 
     * @return false 
     */
    [[nodiscard]]
    auto linkWith(std::string_view source, View<Element> sink, std::string_view sinkName) -> bool;
    /**
     * @brief Get debug output string
     * 
     * @return std::string 
     */
    auto toDocoument() const -> std::string;

    auto operator new(size_t size) -> void * {
        return libc::malloc(size);
    }
    auto operator delete(void *ptr) -> void {
        return libc::free(ptr);
    }
protected:
    Element();

    auto addInput(const char *name) -> Pad *;
    auto addOutput(const char *name) -> Pad *;
    void removeInput(Pad *pad);
    void removeOutput(Pad *pad);
    void removeAllInputs();
    void removeAllOutputs();
    void dispatchTask();
    void waitTask();

    virtual void stateChanged(State newState) { }
    virtual auto processInput(Pad &inputPad, View<Resource> resourceView) -> Error { return Error::NoImpl; }
    virtual auto teardown() -> Error { return Error::NoImpl; }
    virtual auto init() -> Error { return Error::NoImpl; }
    virtual auto resume() -> Error { return Error::NoImpl; }
    virtual auto pause() -> Error { return Error::NoImpl; }
    virtual auto run() -> Error { return Error::NoImpl; }
private:
    void _run();

    Atomic<State> mState { State::Stopped };
    Atomic<bool>  mIsInitialized { false };
    Atomic<bool>  mIsThreadRunning { false }; //< Is running at thread
    Thread       *mWorkthread { nullptr };
    ThreadPolicy  mThreadPolicy { ThreadPolicy::AnyThread };
    Graph        *mGraph { nullptr };
    Bus          *mBus { nullptr };
    

    std::vector<Pad *> mInputPads;
    std::vector<Pad *> mOutputPads;
friend class Pad;
};

/**
 * @brief The pad of the element, it used to send data to another elements
 * 
 */
class NEKO_API Pad {
public:
    enum Type {
        Input,
        Output,
    };
    /**
     * @brief Construct a new Pad object is delete
     * 
     */
    Pad(const Pad &) = delete;
    /**
     * @brief Destroy the Pad
     * 
     */
    ~Pad() = default;

    /**
     * @brief Connect to a new pad
     * 
     * @param other 
     * @return true 
     * @return false 
     */
    auto link(View<Pad> other) -> bool {
        if (mType != Output && other->mType != Input) {
            return false;
        }
        mNext = other.get();
        return true;
    }
    /**
     * @brief Disconnect the connection
     * 
     */
    void unlink() {
        mNext = nullptr;
    }
    bool isLinked() const {
        return mNext != nullptr;
    }
    /**
     * @brief Set the Name of the pad
     * 
     * @param name 
     */
    void setName(std::string_view name) {
        mName = name;
    }
    /**
     * @brief Set the master of the pad
     * 
     * @param element 
     */
    void setElement(Element *element) {
        mElement = element;
    }
    /**
     * @brief Get the name of the pad
     * 
     * @return auto 
     */
    auto name() const noexcept {
        return std::string_view(mName);
    }
    /**
     * @brief Send data to the next element
     * 
     * @param view The view of the resource
     * @return Error 
     */
    auto send(View<Resource> view) -> Error;
    /**
     * @brief Get the type of the pad
     * 
     * @return Type 
     */
    auto type() const -> Type {
        return mType;
    }
    /**
     * @brief Get the element of the pad
     * 
     * @return Element* 
     */
    auto element() const -> Element * {
        return mElement;
    }
    /**
     * @brief Get the connected element of the pad
     * 
     * @return Element* 
     */
    auto nextElement() const -> Element * {
        if (mNext) {
            return mNext->element();
        }
        return nullptr;
    }
    auto next() const -> Pad * {
        return mNext;
    }
    /**
     * @brief Get the property map of the pad
     * 
     * @return Property& 
     */
    auto properties() -> Property & {
        return mProperties;
    }
    /**
     * @brief Indexing the property map
     * 
     * @param name 
     * @return Property& 
     */
    auto property(std::string_view name) -> Property & {
        return mProperties[name];
    }

    auto operator new(size_t size) -> void * {
        return libc::malloc(size);
    }
    auto operator delete(void *ptr) -> void {
        return libc::free(ptr);
    }
private:
    /**
     * @brief Construct a new Pad object
     * 
     * @param type The pad type
     */
    explicit Pad(Type type) : mType(type) { }

    Property      mProperties {Property::newMap()};
    Element      *mElement {nullptr}; //< Which element belong
    Pad          *mNext {nullptr};
    Type          mType;
    std::string   mName;
friend class Element;
};

/**
 * @brief Container of the Elemenet and Global Interface
 * 
 */
class NEKO_API Graph  {
public:
    Graph();
    ~Graph();

    void addElement(Element *element);
    void removeElement(Element *element);
    bool hasCycle() const;

    void addElement(Box<Element> &&element) {
        addElement(element.release());
    }

    template <typename T>
    void registerInterface(T *ptr) {
        _registerInterface(typeid(T), ptr);
    }
    template <typename T>
    void unregisterInterface(T *ptr) {
        _unregisterInterface(typeid(T), ptr);
    }
    template <typename T>
    auto queryInterface() const -> T * {
        return static_cast<T *>(_queryInterface(typeid(T)));
    }

    auto begin() const {
        return mElements.begin();
    }
    auto end() const {
        return mElements.end();
    }
    auto size() const noexcept {
        return mElements.size();
    }
private:
    void _registerInterface(std::type_index idx, void *ptr);
    void _unregisterInterface(std::type_index idx, void *ptr);
    void *_queryInterface(std::type_index idx) const;
    
    std::map<std::type_index, void *> mInterfaces; //< Interfaces set
    std::vector<Element *>            mElements; //< Container of the graph
    mutable std::mutex                mMutex; //< Mutex for Graph
};

/**
 * @brief Pipeline is used to run on a Graph
 * 
 */
class NEKO_API Pipeline {
public:
    Pipeline();
    Pipeline(const Pipeline &) = delete;
    ~Pipeline();

    void setGraph(Graph *graph);
    void setState(State state);
    void start();
    void pause();
    void stop();

    Bus *bus();
private:
    void _start();
    void _resume();

    Graph        *mGraph { nullptr };
    Atomic<State> mState { State::Stopped };
    Box<PipelineImpl> d; //< Private data
};

/**
 * @brief Element creation abstraction
 * 
 */
class ElementFactory {
public:
    virtual Box<Element> createElement(const char *name) const = 0;
    virtual Box<Element> createElement(const std::type_info &info) const = 0;
    template <typename T>
    inline  Box<T> createElement() const {
        auto p = createElement(typeid(T));
        if (!p) {
            return Box<T>();
        }
        auto e = dynamic_cast<T*>(p.get());
        if (!e) {
            return Box<T>();
        }
        p.release();
        return Box<T>(e);
    }
protected:
    ElementFactory() = default;
    ~ElementFactory() = default;
};


NEKO_NS_END