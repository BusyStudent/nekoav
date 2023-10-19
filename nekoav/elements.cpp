#define _NEKO_SOURCE
#include "elements.hpp"
#include "threading.hpp"
#include "latch.hpp"
#include "bus.hpp"
#include <algorithm>
#include <set>

#ifndef NEKO_NO_EXCEPTIONS
    #include <exception>
#endif


NEKO_NS_BEGIN

Element::Element() {

}
Element::~Element() {
    removeAllInputs();
    removeAllOutputs();
}
void Element::setState(State state) {
    assert(mWorkthread != nullptr);
    // Check is same thread
    if (Thread::currentThread() != mWorkthread) {
        Error err;
        mWorkthread->postTask(std::bind(&Element::setState, this, state));
        return;
    }
    if (mState == state) {
        return;
    }
    mState = state;
    stateChanged(state);

    Error err = Error::Ok;
    switch (state) {
        case State::Ready: { 
            err = init(); 
            mIsInitialized = true;
            break;
        }
        case State::Running: { 
            if (mThreadPolicy == ThreadPolicy::SingleThread && !mIsThreadRunning) {
                // Require a single thread, and it doesnot start
                mWorkthread->postTask(std::bind(&Element::_run, this));
            }
            else {
                err = resume();
            }
            break;
        }
        case State::Paused: { 
            err = pause(); 
            break;
        }
        case State::Stopped: { 
            if (mThreadPolicy == ThreadPolicy::AnyThread) {
                // Any thread teardown at now
                // or teardown at _run
                err = teardown(); 
            }
            break;
        }
        default: break;
    }
    if (err != Error::Ok) {
        bus()->postMessage(ErrorMessage::make(err, this));
    }
}


Pad *Element::addInput(const char *name) {
    auto pad = new Pad(Pad::Input);
    mInputPads.push_back(pad);
    pad->setElement(this);
    if (name) {
        pad->setName(name);
    }
    return pad;
}
void Element::removeInput(Pad *pad) {
    if (!pad) {
        return;
    }
    auto it = std::find(mInputPads.begin(), mInputPads.end(), pad);
    if (it != mInputPads.end()) {
        mInputPads.erase(it);
        // pad->setElement(nullptr);
        delete pad;
    }
}

Pad *Element::addOutput(const char *name) {
    auto pad = new Pad(Pad::Output);
    mOutputPads.push_back(pad);
    pad->setElement(this);
    if (name) {
        pad->setName(name);
    }
    return pad;
}
void Element::removeOutput(Pad *pad) {
    if (!pad) {
        return;
    }
    auto it = std::find(mOutputPads.begin(), mOutputPads.end(), pad);
    if (it != mOutputPads.end()) {
        mOutputPads.erase(it);
        // pad->setElement(nullptr);
        delete pad;
    }
}
void Element::removeAllInputs() {
    for (auto &pad : mInputPads) {
        pad->setElement(nullptr);
        delete pad;
    }
    mInputPads.clear();
}
void Element::removeAllOutputs() {
    for (auto &pad : mOutputPads) {
        pad->setElement(nullptr);
        delete pad;
    }
    mOutputPads.clear();
}
void Element::dispatchTask() {
    assert(Thread::currentThread() == mWorkthread);
    mWorkthread->dispatchTask();
}
void Element::waitTask() {
    assert(Thread::currentThread() == mWorkthread);
    mWorkthread->waitTask();
}

void Element::_run() {
    mIsThreadRunning = true;
    auto err = run();
    mIsThreadRunning = false;

    if (err == Error::NoImpl) {
        abort();
    }
    teardown(); //< destroy current event
    if (err != Error::Ok) {
        bus()->postMessage(ErrorMessage::make(err, this));
    }
}

bool Element::linkWith(std::string_view sorceName, View<Element> sink, std::string_view sinkName) {
    const auto &sources = outputs();
    const auto &sinks = sink->inputs();
    auto siter = std::find_if(sources.begin(), sources.end(), [&](const auto &e) { return e->name() == sorceName; });
    if (siter == sources.end()) {
        return false;
    }
    auto diter = std::find_if(sinks.begin(), sinks.end(), [&](const auto &e) { return e->name() == sinkName; });
    if (diter == sinks.end()) {
        return false;
    }
    return (*siter)->link(*diter);
}

Error Pad::send(View<Resource> view) {
    if (!mNext) {
        return Error::NoConnection;
    }
    auto nextElement = mNext->mElement;
    assert(nextElement);

    if (nextElement->mState == State::Stopped) {
        return Error::NoConnection;
    }

    // Got elements, check threads
    if (Thread::currentThread() != nextElement->mWorkthread) {
        Latch latch {1};
        Error errcode = Error::Ok;
        nextElement->mWorkthread->postTask([&]() {
            errcode = nextElement->processInput(*mNext, view);
            latch.count_down();
        });
        latch.wait();
        return errcode;
    }

    // Call the next
    return nextElement->processInput(*mNext, view);
}

Graph::Graph() {

}
Graph::~Graph() {
    
}
void Graph::addElement(Element *element) {
    if (!element) {
        return;
    }
    element->setGraph(this);
    mElements.push_back(element);
}
void Graph::removeElement(Element *element) {
    auto it = std::find(mElements.begin(), mElements.end(), element);
    if (it != mElements.end()) {
        mElements.erase(it);
        delete element;
    }
}
bool Graph::hasCycle() const {
   // we will visit a source element only once
    std::set<Element* > visited;
    std::vector<Element* > sourcesElement;
    for (const auto &element : mElements) {
        if (element->inputs().empty()) {
            sourcesElement.push_back(element);
        }
    }

    std::function<bool(Element *)> dfs = 
        [&dfs, &visited](Element *curElement) -> bool 
    {
        if (visited.find(curElement) != visited.end()) {
            return false;
        }
        visited.insert(curElement);

        for (const auto &pad : curElement->outputs()) {
            auto next = pad->nextElement();
            if (!next) {
                // Not connection
                continue;
            }
            if (dfs(next)) {
                // Detected
                return true;
            }
        }
        return false;
    };

    while (!sourcesElement.empty()) {
        if (dfs(sourcesElement.back())) {
            return true;
        }
        sourcesElement.pop_back();
    }
    return false;
}
void Graph::_registerInterface(std::type_index info, void *ptr) {
    std::lock_guard locker(mMutex);
    mInterfaces[info] = ptr;
}
void Graph::_unregisterInterface(std::type_index info, void *ptr) {
    std::lock_guard locker(mMutex);
    auto mpit = mInterfaces.find(info);
    if (mpit == mInterfaces.end()) {
        return;
    }
    mInterfaces.erase(mpit);
}
void *Graph::_queryInterface(std::type_index info) const {
    std::lock_guard locker(mMutex);
    auto mpit = mInterfaces.find(info);
    if (mpit == mInterfaces.end()) {
        return nullptr;
    }
    return mpit->second;
}

class PipelineImpl {
public:
    std::vector<Thread *>  mThreadPool;
    Thread                *mSharedThread {nullptr};
    Thread                *mPipelineThread {nullptr}; //< Pipelines thread
    Bus                    mBus;
};

Pipeline::Pipeline() : d(new PipelineImpl) {
    d->mBus.addWatcher([this](const Message &msg, bool &){
        if (msg.sender() == this) {
            return;
        }
        if (msg.type() == Message::ErrorOccurred) {
            // We should stop
            
        }
    });
}
Pipeline::~Pipeline() {
    stop();
}

void Pipeline::setGraph(Graph *graph) {
    mGraph = graph;
}

void Pipeline::setState(State state) {
    switch (state) {
        case State::Stopped: return stop();
        case State::Running: return start();
        case State::Paused: return pause();
    }
}

void Pipeline::start() {
    switch (mState) {
        case State::Running: return;
        case State::Error: return;
        case State::Paused: {
            // Restore
            _resume();
            break;
        }
        case State::Stopped: {
            // Starting new 
            _start();
            break;
        }
    }
}
void Pipeline::stop() {
    if (mState == State::Stopped) {
        return;
    }
    for (const auto &elem : *mGraph) {
        elem->setState(State::Stopped);
    }
    for (const auto &thrd : d->mThreadPool) {
        delete thrd;
    }
    d->mThreadPool.clear();
    mGraph->unregisterInterface<Pipeline>(this);
    mState = State::Stopped;
}
void Pipeline::pause() {
    if (mState == State::Running) {
        for (const auto &elem : *mGraph) {
            elem->setState(State::Paused);
        }
        mState = State::Paused;
    }
}

void Pipeline::_resume() {
    if (mState == State::Paused) {
        for (const auto &elem : *mGraph) {
            elem->setState(State::Running);
        }
        mState = State::Running;
    }
}

void Pipeline::_start() {
    // New state

    // Make a global shared thread
    d->mSharedThread = new Thread;
    d->mThreadPool.push_back(d->mSharedThread);

    // For the map
    assert(mGraph);
    mGraph->registerInterface<Pipeline>(this);

    for (const auto &elem : *mGraph) {
        Thread *thread = nullptr;

        if (elem->threadPolicy() == ThreadPolicy::SingleThread) {
            // Require single thread
            thread = new Thread();
            d->mThreadPool.push_back(thread);            
        }
        else {
            // Put it to shared thread
            thread = d->mSharedThread;
        }
        elem->setThread(thread);
        elem->setBus(bus());

        elem->setState(State::Ready);
    }

    for (const auto &elem : *mGraph) {
        elem->setState(State::Running);
    }

    mState = State::Running;
}

Bus *Pipeline::bus() {
    return &d->mBus;
}

NEKO_NS_END