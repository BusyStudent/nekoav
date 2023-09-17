#pragma once

#include "defs.hpp"

NEKO_NS_BEGIN

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
private:
    Atomic<State> mState = State::Preparing;
};

class Pad : public Object {
public:
    
private:
    Weak<Element> mElement;
    Weak<Pad>     mNext;
    Weak<Pad>     mPrev;
};

class Graph : public Object {
public:
    virtual void addElement(Arc<Element> element) = 0;
};

class Factory : public Object {
public:
    virtual Arc<Element> createElement(const char *name) = 0;
};

NEKO_NS_END