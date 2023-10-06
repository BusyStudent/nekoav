#pragma once

#include "resource.hpp"
#include "defs.hpp"
#include <thread>
#include <string>
#include <vector>

NEKO_NS_BEGIN

class Resource;
class Element;
class Thread;
class Graph;
class Pad;

enum Errcode {
    Ok = 0,
    NoConnection,
    NoImpl,
};

/**
 * @brief The smallest executable unit of the system
 * @details This is a interface of the minimum executable unit of the system. 
 * all code want to run in this system should inherit.
 * 
 */
class NEKO_API Element : public Object {
public:
    enum State {
        Preparing,
        Running,
        Paused,
        Stopped,
        Error,
    };

    Element();
    Element(const Element &) = delete;
    ~Element();

    State state() const noexcept {
        return mState;
    }
    void setState(State state);

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
    virtual void onStateChange(State newState) = 0;
    virtual int  onProcessInput(Pad *inputPad, ResourceView resourceView) { return NoImpl; }
    virtual int  onExecute() {return NoImpl; }
private:
    Atomic<State> mState { State::Preparing };
    Thread       *mWorkthread { nullptr };

    std::vector<Arc<Pad> > mInputPads;
    std::vector<Arc<Pad> > mOutputPads;
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
    int  write(ResourceView view);
    Type type() const noexcept {
        return mType;
    }
private:
    Weak<Element> mElement; //< Which element belong
    Weak<Pad>     mNext;
    Type          mType;
    std::string   mName;
};

class NEKO_API Graph  {
public:
    void addElement(const Arc<Element> &element);
    void removeElement(const Arc<Element> & element);
private:
    std::vector<Arc<Element> > mElements; //< Container of the graph
};

class NEKO_API Pipeline {
public:

private:
    Graph *graph { nullptr };
};

class Factory : public Object {
public:
    virtual Arc<Element> createElement(const char *name) = 0;
};

NEKO_NS_END