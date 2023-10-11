#pragma once

#include "resource.hpp"
#include "property.hpp"
#include <typeindex>
#include <typeinfo>
#include <string>
#include <vector>
#include <map>

NEKO_NS_BEGIN

class PipelineImpl;
class Pipeline;
class Resource;
class Element;
class Thread;
class Graph;
class Pad;

enum class Error {
    Ok = 0,
    NoConnection,
    NoImpl,
    UnsupportedFormat,

    Unknown,
};
enum class State {
    Stopped,
    Prepare,
    Running,
    Paused,
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
class NEKO_API Element : public Object {
public:
    Element();
    Element(const Element &) = delete;
    ~Element();

    /**
     * @brief Get the state
     * 
     * @return State 
     */
    State state() const noexcept {
        return mState;
    }
    /**
     * @brief Set the State (blocking)
     * 
     * @param state 
     */
    void setState(State state);
    /**
     * @brief Set the Thread Policy object
     * 
     * @param policy 
     */
    void setThreadPolicy(ThreadPolicy policy) noexcept {
        mThreadPolicy = policy;
    }
    /**
     * @brief Get the thread policy
     * 
     * @return ThreadPolicy 
     */
    ThreadPolicy threadPolicy() const noexcept {
        return mThreadPolicy;
    }
    /**
     * @brief Set the Thread object
     * 
     * @param thread 
     */
    void setThread(Thread *thread) noexcept {
        mWorkthread = thread;
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
    /**
     * @brief Link the output pad to target pad
     * 
     * @param source The output pad name in self
     * @param sink The dst element 
     * @param sinkName The target input pad
     * @return true 
     * @return false 
     */
    bool linkWith(const char *source, View<Element> sink, const char *sinkName);
protected:
    void addInput(const Arc<Pad> &pad);
    void addOutput(const Arc<Pad> &pad);
    void removeInput(const Arc<Pad> &pad);
    void removeOutput(const Arc<Pad> &pad);
    void dispatchTask();
    void waitTask();

    virtual void  stateChanged(State newState) { }
    virtual Error processInput(Pad &inputPad, View<Resource> resourceView) { return Error::NoImpl; }
    virtual Error initialize() { return Error::NoImpl; }
    virtual Error teardown() { return Error::NoImpl; }
    virtual Error resume() { return Error::NoImpl; }
    virtual Error pause() { return Error::NoImpl; }
    virtual Error run() { return Error::NoImpl; }
private:
    void _threadMain(Latch *initlatch); //< Call from 

    Atomic<State> mState { State::Stopped };
    Thread       *mWorkthread { nullptr };
    ThreadPolicy  mThreadPolicy { ThreadPolicy::AnyThread };

    std::vector<Arc<Pad> > mInputPads;
    std::vector<Arc<Pad> > mOutputPads;
friend class Pipeline;
friend class Pad;
};

/**
 * @brief The pad of the element, it used to send data to another elements
 * 
 */
class NEKO_API Pad : public Object {
public:
    enum Type {
        Input,
        Output,
    };

    /**
     * @brief Construct a new Pad object
     * 
     * @param type The pad type
     */
    explicit Pad(Type type) : mType(type) { }
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
    bool connect(const Arc<Pad> &other) {
        if (mType != Output && other->mType != Input) {
            return false;
        }
        mNext = other;
        return true;
    }
    /**
     * @brief Disconnect the connection
     * 
     */
    void disconnect() {
        mNext.reset();
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
    Error write(View<Resource> view);
    /**
     * @brief Get the type of the pad
     * 
     * @return Type 
     */
    Type type() const noexcept {
        return mType;
    }
    /**
     * @brief Get the element of the pad
     * 
     * @return Element* 
     */
    Element *element() const noexcept {
        return mElement;
    }
    /**
     * @brief Get the connected element of the pad
     * 
     * @return Element* 
     */
    Element *nextElement() const noexcept {
        auto next = mNext.lock();
        if (next) {
            return next->element();
        }
        return nullptr;
    }
    /**
     * @brief Get the property map of the pad
     * 
     * @return Property& 
     */
    Property &properties() noexcept {
        return mProperties;
    }
    /**
     * @brief Indexing the property map
     * 
     * @param name 
     * @return Property& 
     */
    Property &property(std::string_view name) {
        return mProperties[name];
    }

    /**
     * @brief Construct a new input pad
     * 
     * @param name The pad name (default in nullptr)
     * @return Arc<Pad> 
     */
    static Arc<Pad> NewInput(const char *name = nullptr);
    /**
     * @brief Construct a new output pad
     * 
     * @param name The pad name (default in nullptr)
     * @return Arc<Pad> 
     */
    static Arc<Pad> NewOutput(const char *name = nullptr);
private:
    Property      mProperties {Property::NewMap()};
    Element      *mElement {nullptr}; //< Which element belong
    Weak<Pad>     mNext;
    Type          mType;
    std::string   mName;
};

/**
 * @brief Container of the Elemenet
 * 
 */
class NEKO_API Graph  {
public:
    Graph();
    ~Graph();

    void addElement(const Arc<Element> &element);
    void removeElement(const Arc<Element> & element);
    bool hasCycle() const;

    template <typename T>
    void registerInterface(T *ptr) {
        _registerInterface(typeid(T), ptr);
    }
    template <typename T>
    void unregisterInterface(T *ptr) {
        _unregisterInterface(typeid(T), ptr);
    }
    template <typename T>
    auto queryInterface() const {
        void **array;
        size_t n = 0;
        _queryInterface(typeid(T), &array, &n);
        return std::make_pair(
            reinterpret_cast<T **>(array),
            n
        );
    }

    auto begin() const {
        return mElements.begin();
    }
    auto end() const {
        return mElements.end();
    }
private:
    void _registerInterface(std::type_index idx, void *ptr);
    void _unregisterInterface(std::type_index idx, void *ptr);
    void _queryInterface(std::type_index idx, void ***arr, size_t *n) const;
    
    std::map<std::type_index, std::vector<void*> > mInterfaces; //< Interfaces set
    std::vector<Arc<Element> >                     mElements; //< Container of the graph
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
    void start();
    void pause();
    void stop();
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
    virtual Arc<Element> createElement(const char *name) const = 0;
    virtual Arc<Element> createElement(const std::type_info &info) const = 0;
    template <typename T>
    inline  Arc<Element> createElement() const {
        return createElement(typeid(T));
    }
protected:
    ElementFactory() = default;
    ~ElementFactory() = default;
};


NEKO_NS_END