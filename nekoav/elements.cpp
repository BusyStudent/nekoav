#define _NEKO_SOURCE
#include "elements.hpp"
#include "threading.hpp"
#include "utils.hpp"
#include "bus.hpp"
#include "log.hpp"
#include <algorithm>
#include <ranges>
#include <set>

#ifdef __cpp_lib_format
    #include <format>
#endif

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
void Element::setState(State state, std::latch *syncLatch) {
    assert(mWorkthread != nullptr);
    // Check is same thread
    if (Thread::currentThread() != mWorkthread) {
        mWorkthread->postTask(std::bind(&Element::setState, this, state, syncLatch));
        return;
    }
    if (mState == state) {
        if (syncLatch) {
            syncLatch->count_down();
        }
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
    if (err != Error::Ok && err != Error::NoImpl) {
        mState = State::Error;
        bus()->postMessage(ErrorMessage::make(err, this));
    }

    NEKO_LOG("Element {} state changed to {}", typeid(*this), mState.load());

    if (syncLatch) {
        syncLatch->count_down();
    }
}

Pad *Element::findInput(std::string_view name) const {
    auto iter = std::find_if(mInputPads.begin(), mInputPads.end(), [&](Pad *pad) { return pad->name() == name; });
    if (iter != mInputPads.end()) {
        return *iter;
    }
    return nullptr;
}
Pad *Element::findOutput(std::string_view name) const {
    auto iter = std::find_if(mOutputPads.begin(), mOutputPads.end(), [&](Pad *pad) { return pad->name() == name; });
    if (iter != mOutputPads.end()) {
        return *iter;
    }
    return nullptr;
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
void Element::waitTask(int timeout) {
    assert(Thread::currentThread() == mWorkthread);
    mWorkthread->waitTask(timeout);
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

bool Element::linkWith(std::string_view sourceName, View<Element> sink, std::string_view sinkName) {
    auto src = findOutput(sourceName);
    auto sk = sink->findInput(sinkName);
    if (!src || !sk) {
        return false;
    }
    return src->link(sk);
}

auto Element::toDocoument() const -> std::string {
    std::string ret;
    
#ifdef __cpp_lib_format
    // Format state name
    std::string_view stateName;
    switch (mState.load()) {
        case State::Error: stateName = "Error"; break;
        case State::Ready: stateName = "Ready"; break;
        case State::Running: stateName = "Running"; break;
        case State::Paused: stateName = "Paused"; break;
        case State::Stopped: stateName = "Stopped"; break;
    }
    auto formatPads = [](std::string &ret, Pad *pad) {
        std::format_to(std::back_inserter(ret), "  Pad: {}\n", pad->name());
        std::format_to(std::back_inserter(ret), "  Linked: {}\n", pad->isLinked());
        std::format_to(std::back_inserter(ret), "  Properties: \n");
        for (const auto &[key, value] : pad->properties()) {
            std::format_to(std::back_inserter(ret), "    {}: {}, \n", key, value.toDocoument());
        }
    };

    std::format_to(std::back_inserter(ret), "Thread: {:#010x}\n", uintptr_t(mWorkthread));
    std::format_to(std::back_inserter(ret), "State: {}\n", stateName);
    std::format_to(std::back_inserter(ret), "Inputs: {}\n", mInputPads.size());
    for (const auto &pad : mInputPads) {
        formatPads(ret, pad);
    }
    std::format_to(std::back_inserter(ret), "Outputs {}\n", mOutputPads.size());
    for (const auto &pad : mOutputPads) {
        formatPads(ret, pad);
    }
#endif

    return ret;
}

Error Pad::send(View<Resource> view) {
    if (!mNext) {
        return Error::NoConnection;
    }
    auto nextElement = mNext->mElement;
    assert(nextElement);

    if (nextElement->mState == State::Stopped || nextElement->mState == State::Error) {
        return Error::NoConnection;
    }

    // Got elements, check threads
    if (Thread::currentThread() != nextElement->mWorkthread) {
        std::latch latch {1};
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
    for (auto v : mElements) {
        delete v;
    }
}
void Graph::addElement(Element *element) {
    if (!element) {
        return;
    }
    std::lock_guard locker(mMutex);
    element->setGraph(this);
    mElements.push_back(element);
}
void Graph::removeElement(Element *element) {
    if (!element) {
        return;
    }
    std::lock_guard locker(mMutex);
    auto it = std::find(mElements.begin(), mElements.end(), element);
    if (it != mElements.end()) {
        mElements.erase(it);
        delete element;
    }
}
bool Graph::hasCycle() const {
   // we will visit a source element only once
    std::lock_guard locker(mMutex);
    auto size = topologicalSort().size();
    if (size == mElements.size()) {
        return false;
    }
    return true;
}

auto Graph::topologicalSort() const -> std::vector<Element *> {
    std::map<Element*, int> inDeg;

    // Init all element in map
    for (const auto &element : mElements) {
        inDeg[element] = 0;
    }
    // Count the in-degree of each node
    for (const auto &element : mElements) {
        for (const auto &pad : element->outputs()){
            if (pad->nextElement() != nullptr) {
                inDeg[pad->nextElement()] ++;
            }
        }
    }
    // Push all nodes with indegree 0 to the queue
    std::vector<Element* > sourcesElement;
    for (const auto [key, value] : inDeg) {
        if (value == 0) {
            sourcesElement.push_back(key);
        }
    }
    // Traverse the queue and delete in-degree points
    int index = 0;
    while (index < sourcesElement.size()) {
        for (const auto &pad : sourcesElement[index]->outputs()) {
            auto next = pad->nextElement();
            if (next != nullptr) {
                inDeg[next] --;
                if (inDeg[next] == 0) {
                    // Push node when indegree == 0
                    sourcesElement.push_back(next);
                }
            }
        }
        index ++;
    }
    // Topological sorting is completed if and only if the number of topological sorting nodes is equal to the number of source nodes.
    if (sourcesElement.size() == mElements.size()) {
        NEKO_LOG("{}", sourcesElement);
        return sourcesElement;
    }
    return std::vector<Element *>();
}
auto Graph::toDocoument() const -> std::string {
    auto sorted = topologicalSort();
    if (sorted.empty()) {
        return std::string();
    }

    std::string ret;
#ifdef __cpp_lib_format
    std::format_to(std::back_inserter(ret), "Graph Elements count {}\n", sorted.size());
    for (const auto elem : sorted) {
        std::format_to(std::back_inserter(ret), "Element impl type {}\n", typeid(*elem).name());
        for (auto view : std::views::split(elem->toDocoument(), '\n')) {
            ret += "  ";
            ret += std::string_view(view.begin(), view.end());
            ret += '\n';
        }
    }
#endif

    return ret;
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
    Bus                    mElementBus; //< From Element to Pipeline
    Bus                    mBus; //< From Pipeline to User

    std::thread            mThread; //< Thread for pipeline
};

Pipeline::Pipeline() : d(new PipelineImpl) {

}
Pipeline::~Pipeline() {
    stop();
}

void Pipeline::setGraph(Graph *graph) {
    mGraph = graph;
}

void Pipeline::setState(State state) {
    switch (state) {
        case State::Stopped: {
            if (mState != State::Stopped) {
                mState = State::Stopped;
                _wakeup();
                if (d->mThread.joinable()) {
                    d->mThread.join();
                }
                for (auto th : d->mThreadPool) {
                    delete th;
                }
                d->mThreadPool.clear();
                d->mSharedThread = nullptr;
            }
            return;
        }
        case State::Running: {
            if (mState == State::Stopped) {
                setState(State::Ready); //< First Init
            }
            if (mState == State::Ready) {
                setState(State::Paused);
            }
            if (mState == State::Paused) {
                mState = State::Running;
                _wakeup();
            }
            return;
        }
        case State::Paused: {
            if (mState == State::Running || mState == State::Ready) {
                // Can switch
                mState = State::Paused;
                _wakeup();
            }
            return;
        }
        case State::Ready: {
            if (mState != State::Stopped) {
                return;
            }
            _prepare(); // Prepare it
            return;
        }
    }
}

void Pipeline::_main(std::latch &latch) {
    NEKO_SetThreadName("NekoPipeline");
    NEKO_DEBUG("Init env");
    // Init here
    // 1. For all elements get require threads
    _doInit();

    // Done
    mState = State::Ready;
    latch.count_down();

    if (mGraph->hasCycle()) {
        // Error
        _raiseError(Error::InvalidTopology);
        return;
    }

    // Pull Message
    while (true) {
        Arc<Message> message;
        d->mElementBus.waitMessage(&message);
        if (message->type() == Message::ErrorOccurred) {
            // Enter Error
            _raiseError(static_cast<ErrorMessage*>(message.get())->error());
            return;
        }
        else if (message->type() == Message::PipelineWakeup) {
            // 
            switch (mState) {
                case State::Paused: _broadcastState(State::Paused); break;
                case State::Running: _broadcastState(State::Running); break;
                case State::Stopped: _broadcastState(State::Stopped); return; //< End it
            }
        }
        else {
            d->mBus.postMessage(message);
        }
    }
}

void Pipeline::_doInit() {
    // 1. For all elements get require threads
    auto threadElementsView = *mGraph | std::views::filter(
        [](auto elem) { return elem->threadPolicy() == ThreadPolicy::SingleThread; }
    );
    auto threadElement = std::vector(threadElementsView.begin(), threadElementsView.end());

    auto walkAndSet = [this](auto &&self, Element *currentElement, Thread *thread) {
        if (currentElement->threadPolicy() == ThreadPolicy::SingleThread) {
            return; //< Stop on this
        }
        if (currentElement->thread() == d->mSharedThread) {
            // Got it, has no workers
            NEKO_LOG("Set Element {} to thread {}", currentElement, thread);
            currentElement->setThread(thread);
        }
        if (currentElement->thread() != thread) {
            // Alreay walked by another element
            return;
        }

        for (const auto &pad : currentElement->outputs()) {
            if (pad->nextElement()) {
                self(self, pad->nextElement(), thread);
            }
        }
    };

    // Make a global shared thread
    d->mSharedThread = new Thread;
    d->mThreadPool.push_back(d->mSharedThread);

    // Prepare an shared thread for all
    for (auto element : *mGraph) {
        element->setThread(d->mSharedThread);
        element->setBus(&d->mElementBus);
    }

    // Allocate thread for Thread Element and below
    for (auto element : threadElement) {
        Thread *thread = new Thread;
        d->mThreadPool.push_back(thread);
        element->setThread(thread);

        NEKO_LOG("Alloc thread {}", thread);
        NEKO_LOG("Set Element {} to thread {}", element, thread);

        // Set Thread below
        walkAndSet(walkAndSet, element, thread);
    }

    // Begin init elements
    _broadcastState(State::Ready);
}
void Pipeline::_raiseError(Error err) {
    mState = State::Error;
    d->mBus.postMessage(ErrorMessage::make(err, this));
    // Shutdown all
    _broadcastState(State::Stopped);
}

void Pipeline::_prepare() {
    // Start pipeline thread
    std::latch latch {1};
    d->mThread = std::thread(&Pipeline::_main, this, std::ref(latch));
    latch.wait();
}
void Pipeline::_broadcastState(State state) {
    std::latch syncLatch {ptrdiff_t(mGraph->size())};
    for (auto element : *mGraph) {
        element->setState(state, &syncLatch);
    }
    syncLatch.wait();
}
void Pipeline::_wakeup() {
    auto msg = make_shared<Message>(Message::PipelineWakeup, this);
    d->mElementBus.postMessage(msg);
}

Bus *Pipeline::bus() {
    return &d->mBus;
}

NEKO_NS_END