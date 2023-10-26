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
/**
 * @brief State of Element / Pipeline
 * 
 * @details from Stopped -> Ready -> Paused <-> Running -> Stopped
 * 
 */
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
    virtual ~Element();

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
     * @param syncLatch
     * 
     * @code {.c++}
     * std::latch syncLatch(1);
     * setState(state, syncLatch);
     * syncLatch.wait();
     * @endcode
     * 
     * @note
     * setState(state, isWait, timeout)
     * changeStateAndWait(state, timeout)
     * 
     */
    auto setState(State state, std::latch *syncLatch = nullptr) -> void;
    /**
     * @brief Set the Thread Policy object
     * 
     * @param policy 
     */
    auto setThreadPolicy(ThreadPolicy policy) noexcept -> void {
        mThreadPolicy = policy;
    }
    /**
     * @brief Set the Name object for debugging
     * 
     * @param name 
     */
    auto setName(std::string_view name) -> void {
        mName = name;
    }
    /**
     * @brief Set the Bus object
     * 
     * @param bus 
     */
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
    auto  thread() const noexcept {
        return mWorkthread;
    }
    auto  graph() const noexcept {
        return mGraph;
    }
    auto  bus() const noexcept {
        return mBus;
    }
    auto  name() const -> std::string;
    /**
     * @brief Find a input pad by name
     * 
     * @param name 
     * @return Pad* 
     */
    auto findInput(std::string_view name) const -> Pad *;
    /**
     * @brief Find a output pad by name
     * 
     * @param name 
     * @return Pad* 
     */
    auto findOutput(std::string_view name) const -> Pad *;
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
     * @brief Link the output pad to target pad with default pad name
     * 
     * @param sink The sink element name
     * @return true 
     * @return false 
     */
    [[nodiscard]]
    auto linkWith(View<Element> sink) -> bool {
        return linkWith("src", sink, "sink");
    }
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

    /**
     * @brief Create a input pad into it
     * 
     * @param name 
     * @return Pad* 
     */
    auto addInput(const char *name) -> Pad *;
    /**
     * @brief Create a output pad and add
     * 
     * @param name 
     * @return Pad* 
     */
    auto addOutput(const char *name) -> Pad *;
    /**
     * @brief Remove the input pad, this pad will be removed and deleted
     * 
     * @param pad 
     */
    void removeInput(Pad *pad);
    /**
     * @brief Remove the output pad, this pad will be removed and deleted
     * 
     * @param pad 
     */
    void removeOutput(Pad *pad);
    /**
     * @brief Remove all input pads
     * 
     */
    void removeAllInputs();
    /**
     * @brief Remove all output pads
     * 
     */
    void removeAllOutputs();
    /**
     * @brief Pull and execute task in task queue
     * 
     * @code {.cpp}
     *  Error run() override {
     *    if (state() != State::Stopped) {
     *      dispatchTask();
     *      Your code here
     *    }
     *  }
     * @endcode
     * 
     * 
     */
    void dispatchTask();
    /**
     * @brief Pull and execute task in task queue with timeout
     * 
     * @param timeout Timeout (-1 in INF)
     */
    void waitTask(int timeout = -1);

    /**
     * @brief Callback notify you state was changed to newState
     * 
     * @param newState 
     */
    virtual void stateChanged(State newState) { }

    /**
     * @brief Invoked by Pad::send, It defaultly invoke processInput or post a invoke task by checking work thread
     * 
     * @note It is note thread safe, It can be called in any thread, you should handle it by your self
     * 
     * @param inputPad 
     * @param resourceView 
     * @return Error 
     */
    virtual auto doProcessInput(Pad &inputPad, View<Resource> resourceView) -> Error;
    virtual auto processInput(Pad &inputPad, View<Resource> resourceView) -> Error;
    virtual auto teardown() -> Error { return Error::Ok; }
    virtual auto init() -> Error { return Error::Ok; }
    virtual auto resume() -> Error { return Error::Ok; }
    virtual auto pause() -> Error { return Error::Ok; }
    virtual auto run() -> Error;
private:
    void _run();

    Atomic<State> mState { State::Stopped };
    Atomic<bool>  mIsInitialized { false };
    Atomic<bool>  mIsThreadRunning { false }; //< Is running at thread
    Thread       *mWorkthread { nullptr };
    ThreadPolicy  mThreadPolicy { ThreadPolicy::AnyThread };
    Graph        *mGraph { nullptr };
    Bus          *mBus { nullptr };
    std::string  mName;

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
        unlink();
        mNext = other.get();
        mNext->mLinksCount += 1;
        return true;
    }
    /**
     * @brief Disconnect the connection
     * 
     */
    void unlink() {
        if (mNext) {
            mNext->mLinksCount -= 1;
            mNext = nullptr;
        }
    }
    bool isLinked() const {
        if (mType == Input) {
            return mLinksCount > 0;
        }
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
    auto properties() -> PropertyMap & {
        return mProperties;
    }
    /**
     * @brief Indexing the property map
     * 
     * @param name 
     * @return Property& 
     */
    auto property(std::string_view name) -> Property & {
        return mProperties[std::string(name)];
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

    PropertyMap   mProperties;
    Atomic<int>   mLinksCount {0};    //< Numof Output pad linked with (for Input Pad)
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
    Graph(const Graph &) = delete;
    ~Graph();

    /**
     * @brief Add a new element into Graph, ownship transfered to Graph
     * 
     * @param element 
     */
    void addElement(Element *element);
    /**
     * @brief Remove a element in Graph, it will be deleted 
     * 
     * @param element 
     */
    void removeElement(Element *element);
    /**
     * @brief Check the graph has circle
     * 
     * @return true 
     * @return false 
     */
    bool hasCycle() const;
    /**
     * @brief Generate node topological sorting
     * 
     * @return std::vector<Element *> 
     */
    auto topologicalSort() const -> std::vector<Element *>;
    /**
     * @brief Get the debug info of the graph
     * 
     * @return std::string 
     */
    auto toDocoument() const -> std::string;
    /**
     * @brief Add a new element into Graph, ownship transfered to Graph
     * 
     * @param element Box<> &&
     */
    void addElement(Box<Element> &&element) {
        addElement(element.release());
    }

    template <typename ...Args>
    void addElements(Args &&...args) {
        Element *array [] = { args... };
        for (auto element : array) {
            addElement(element);
        }
    }
    
    /**
     * @brief Register a interface into the Graph
     * 
     * @tparam T The interface type
     * @param ptr 
     */
    template <typename T>
    void registerInterface(T *ptr) {
        _registerInterface(typeid(T), ptr);
    }
    /**
     * @brief Unregister a interface into the Graph
     * 
     * @tparam T 
     * @param ptr 
     */
    template <typename T>
    void unregisterInterface(T *ptr) {
        _unregisterInterface(typeid(T), ptr);
    }
    /**
     * @brief Try to Get the registered interface in Queue
     * 
     * @tparam T The interface type
     * @return T* 
     */
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

    /**
     * @brief Set the Graph to run
     * @note This method doesnot take the ownship of the Graph, it just borrow
     * 
     * @param graph The graph ptr
     */
    void setGraph(View<Graph> graph);
    /**
     * @brief Change the pipeline state
     * 
     * @param state 
     */
    void setState(State state);

    void start() {
        setState(State::Running);
    }
    void stop() {
        setState(State::Stopped);
    }
    void pause() {
        setState(State::Paused);
    }
    Bus *bus();
private:
    void _main(std::latch &);
    void _prepare();
    void _doInit();
    void _raiseError(Error err);
    void _broadcastState(State state); //< Set All element state, blocking
    void _wakeup();

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