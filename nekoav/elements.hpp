#pragma once

#include "resource.hpp"
#include "defs.hpp"
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
};
enum class State {
    Stopped,
    Running,
    Paused,
    Error,
};
enum ThreadPolicy {
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

    State state() const noexcept {
        return mState;
    }
    void setState(State state);
    void setThreadPolicy(ThreadPolicy policy) noexcept {
        mThreadPolicy = policy;
    }
    ThreadPolicy threadPolicy() const noexcept {
        return mThreadPolicy;
    }
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
protected:
    void addInput(const Arc<Pad> &pad);
    void addOutput(const Arc<Pad> &pad);
    void removeInput(const Arc<Pad> &pad);
    void removeOutput(const Arc<Pad> &pad);
    void dispatchTask();
    void waitTask();

    virtual void  stateChanged(State newState) { }
    virtual Error processInput(Pad *inputPad, ResourceView resourceView) { return Error::NoImpl; }
    virtual Error run() { return Error::NoImpl; }
private:
    void _threadMain(Latch *initlatch); //< Call from 

    Atomic<State> mState { State::Stopped };
    Thread       *mWorkthread { nullptr };
    ThreadPolicy  mThreadPolicy { AnyThread };

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

    explicit Pad(Type type) : mType(type) { }
    Pad(const Pad &) = delete;
    ~Pad() = default;

    bool connect(const Arc<Pad> &other) {
        if (mType != Output && other->mType != Input) {
            return false;
        }
        mNext = other;
        return true;
    }
    void disconnect() {
        mNext.reset();
    }
    void setName(std::string_view name) {
        mName = name;
    }
    void setElement(Element *element) {
        mElement = element;
    }

    Error write(ResourceView view);
    
    Type type() const noexcept {
        return mType;
    }
    Element *element() const noexcept {
        return mElement;
    }
    Element *nextElement() const noexcept {
        auto next = mNext.lock();
        if (next) {
            return next->element();
        }
        return nullptr;
    }

    static Arc<Pad> NewInput();
    static Arc<Pad> NewOutput();
private:
    Element      *mElement {nullptr}; //< Which element belong
    Weak<Pad>     mNext;
    Type          mType;
    std::string   mName;
};

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
    void queryInterface() const {
        void **array;
    }

    auto begin() const {
        return mElements.begin();
    }
    auto end() const {
        return mElements.end();
    }
private:
    void _registerInterface(const std::type_info &info, void *ptr);
    void _unregisterInterface(const std::type_info &info, void *ptr);
    
    std::map<std::type_index, std::vector<void*> > mInterfaces; //< Interfaces set
    std::vector<Arc<Element> >                     mElements; //< Container of the graph
};

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