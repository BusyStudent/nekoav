#define _NEKO_SOURCE
#include "elements.hpp"
#include "threading.hpp"
#include "latch.hpp"
#include <algorithm>
#include <set>

#ifndef NEKO_NO_EXCEPTIONS
    #include <exception>
#endif


NEKO_NS_BEGIN

Element::Element() {

}
Element::~Element() {
    
}
void Element::setState(State state) {
    assert(mWorkthread != nullptr);
    // Check is same thread
    if (Thread::currentThread() != mWorkthread) {
        mWorkthread->sendTask(std::bind(&Element::setState, this, state));
        return;
    }
    mState = state;
    stateChanged(state);
    switch (state) {
        case State::Stopped : teardown(); break;
        case State::Prepare : initialize(); break;
        case State::Running : resume(); break;
        case State::Paused : pause(); break;
        default: break;
    }
}


void Element::addInput(const Arc<Pad> &pad) {
    if (!pad) {
        return;
    }
    assert(pad->type() == Pad::Input);
    mInputPads.push_back(pad);
    pad->setElement(this);
}
void Element::removeInput(const Arc<Pad> &pad) {
    if (!pad) {
        return;
    }
    auto it = std::find(mInputPads.begin(), mInputPads.end(), pad);
    if (it != mInputPads.end()) {
        mInputPads.erase(it);
        pad->setElement(nullptr);
    }
}

void Element::addOutput(const Arc<Pad> &pad) {
    if (!pad) {
        return;
    }
    assert(pad->type() == Pad::Output);
    mOutputPads.push_back(pad);
    pad->setElement(this);
}
void Element::removeOutput(const Arc<Pad> &pad) {
    if (!pad) {
        return;
    }
    auto it = std::find(mOutputPads.begin(), mOutputPads.end(), pad);
    if (it != mOutputPads.end()) {
        mOutputPads.erase(it);
        pad->setElement(nullptr);
    }
}
void Element::dispatchTask() {
    assert(Thread::currentThread() == mWorkthread);
    mWorkthread->dispatchTask();
}
void Element::waitTask() {
    assert(Thread::currentThread() == mWorkthread);
    mWorkthread->waitTask();
}

void Element::_threadMain(Latch *latch) {
    // Wait another element ready
    setState(State::Prepare);
    latch->arrive_and_wait();
    latch = nullptr; //< No needed

    setState(State::Running);
    if (run() == Error::NoImpl) {
        abort();
    }
}
inline bool Element::linkWith(const char *sorceName, View<Element> sink, const char *sinkName) {
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
    return (*siter)->connect(*diter);
}

Error Pad::write(View<Resource> view) {
    auto next = mNext.lock();
    if (!next) {
        // Next is no longer exists
        mNext.reset();
        return Error::NoConnection;
    }
    auto nextElement = next->mElement;
    assert(nextElement);

    // Got elements, check threads
    if (Thread::currentThread() != nextElement->mWorkthread) {
        Latch latch {1};
        Error errcode = Error::Ok;
        nextElement->mWorkthread->postTask([&]() {
            errcode = nextElement->processInput(*next, view);
            latch.count_down();
        });
        latch.wait();
        return errcode;
    }

    // Call the next
    return nextElement->processInput(*next.get(), view);
}

Arc<Pad> Pad::NewInput(const char *name) {
    auto pad = make_shared<Pad>(Input);
    if (name) {
        pad->setName(name);
    }
    return pad;
}
Arc<Pad> Pad::NewOutput(const char *name) {
    auto pad = make_shared<Pad>(Output);
    if (name) {
        pad->setName(name);
    }
    return pad;
}

Graph::Graph() {

}
Graph::~Graph() {
    
}
void Graph::addElement(const Arc<Element> &element) {
    mElements.push_back(element);
}
void Graph::removeElement(const Arc<Element> &element) {
    auto it = std::find(mElements.begin(), mElements.end(), element);
    if (it != mElements.end()) {
        mElements.erase(it);
    }
}
bool Graph::hasCycle() const {
   // we will visit a source element only once
    std::set<Element* > visited;
    std::vector<Element* > sourcesElement;
    for (const auto &element : mElements) {
        if (element->inputs().empty()) {
            sourcesElement.push_back(element.get());
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
    mInterfaces[info].push_back(ptr);
}
void Graph::_unregisterInterface(std::type_index info, void *ptr) {
    auto mpit = mInterfaces.find(info);
    if (mpit == mInterfaces.end()) {
        return;
    }
    auto &[_, vec] = *mpit;
    auto iter = std::find(vec.begin(), vec.end(), ptr);
    if (iter != vec.end()) {
        vec.erase(iter);
    }
}
void Graph::_queryInterface(std::type_index info, void ***arr, size_t *n) {
    auto mpit = mInterfaces.find(info);
    if (mpit == mInterfaces.end()) {
        *arr = nullptr;
        *n = 0;
        return;
    }
    auto &[_, vec] = *mpit;
    *arr = vec.data();
    *n = vec.size();
}

class PipelineImpl {
public:
    std::vector<Thread *>  mThreadPool;
    std::vector<Element *> mWorkingElements; //< Elements require a indenpent thread
    Thread                *mSharedThread {nullptr};
};

Pipeline::Pipeline() : d(new PipelineImpl) {

}
Pipeline::~Pipeline() {
    stop();
}

void Pipeline::setGraph(Graph *graph) {
    mGraph = graph;
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
    for (const auto &elem : *mGraph) {
        elem->setState(State::Stopped);
        elem->setThread(nullptr);
    }
    for (const auto &thrd : d->mThreadPool) {
        delete thrd;
    }
    d->mThreadPool.clear();
    d->mWorkingElements.clear();
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
        if (elem->threadPolicy() == ThreadPolicy::SingleThread) {
            // Require single thread
            d->mWorkingElements.push_back(elem.get());
        }
        else {
            // Put it to shared thread
            elem->setThread(d->mSharedThread);
            elem->setState(State::Prepare);
            elem->setState(State::Running);
        }
    }

    // Allocate numof thread
    size_t numWorkingElements = d->mWorkingElements.size();
    assert(numWorkingElements);

    Latch latch {ptrdiff_t(numWorkingElements)}; //< Wait for all ready

    for (size_t n = 0; n < numWorkingElements; n++) {
        auto thrd = new Thread();
        d->mThreadPool.push_back(thrd);
        d->mWorkingElements[n]->setThread(thrd);

        // Call main 
        thrd->postTask(std::bind(&Element::_threadMain, d->mWorkingElements[n], &latch));
    }

    latch.wait();

    mState = State::Running;
}

NEKO_NS_END