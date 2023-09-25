#pragma once

#include "defs.hpp"
#include <thread>
#include <string>

NEKO_NS_BEGIN

class Element;
class Thread;
class Graph;
class Pad;

/**
 * @brief The smallest executable unit of the system
 * @details This is a interface of the minimum executable unit of the system. 
 * all code want to run in this system should inherit.
 * 
 */
class Element : public Object {
public:
    enum State {
        Preparing,
        Running,
        Paused,
        Stopped,
        Error,
    };

    State state() const noexcept {
        return mState;
    }
protected:
    virtual void onStateChange(State oldState) = 0;
private:
    Atomic<State> mState { State::Preparing };
    Thread       *mWorkthread { nullptr };
};

class Pad : public Object {
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
    }
private:
    Weak<Element> mElement; //< Which element belong
    Weak<Pad>     mNext;
    Type          mType;
    std::string   mName;
};

class Graph : public Object {
public:
    virtual void addElement(Arc<Element> element) = 0;
};

class Pipeline : public Object {
public:

};

class Factory : public Object {
public:
    virtual Arc<Element> createElement(const char *name) = 0;
};

NEKO_NS_END