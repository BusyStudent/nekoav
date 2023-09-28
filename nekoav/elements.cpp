#define _NEKO_SOURCE
#include "elements.hpp"
#include "threading.hpp"
#include "latch.hpp"

NEKO_NS_BEGIN

Element::Element() {

}
Element::~Element() {
    
}
void Element::setState(State state) {
    // Check is same thread
    if (Thread::currentThread() != mWorkthread) {
        mWorkthread->sendTask(std::bind(&Element::setState, this, state));
        return;
    }
    mState = state;
    onStateChange(state);
}

int Pad::write(ResourceView view) {
    auto self = mElement.lock();
    auto next = mNext.lock();
    if (!next) {
        // Next is no longer exists
        mNext.reset();
        return NoConnection;
    }
    auto nextElement = next->mElement.lock();
    assert(nextElement);

    // Got elements, check threads
    if (Thread::currentThread() == nextElement->mWorkthread) {
        Latch latch {1};
        int errcode = Ok;
        nextElement->mWorkthread->postTask([&]() {
            errcode = nextElement->processInput(next.get(), view);
            latch.count_down();
        });
        latch.wait();
        return errcode;
    }

    // Call the next
    return nextElement->processInput(next.get(), view);
}


NEKO_NS_END