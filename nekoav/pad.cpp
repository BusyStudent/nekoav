#define _NEKO_SOURCE
#include "time.hpp"
#include "pad.hpp"

NEKO_NS_BEGIN

// Pad here
Pad::Pad(Element *master, Type type, std::string_view name) : mElement(master), mType(type), mName(name) {
    NEKO_ASSERT(master != nullptr);
}
Pad::~Pad() {

}

Error Pad::push(View<Resource> resourceView) {
    if (mType == Input) {
        // You can not push a Input pad
        return Error::InvalidArguments;
    }
    if (!mNext) {
        return Error::NoLink;
    }
    if (!mNext->mCallback) {
        return Error::InvalidState;
    }
    // NEKO_TRACE_TIME(duration) {
        return mNext->mCallback(resourceView);
    // }
}
Error Pad::pushEvent(View<Event> eventView) {
    if (mType == Input) {
        // You can not push a Input pad
        return Error::InvalidArguments;
    }
    if (!mNext) {
        return Error::NoLink;
    }
    if (!mNext->mEventCallback) {
        return Error::InvalidState;
    }
    NEKO_TRACE_TIME(duration) {
        return mNext->mEventCallback(eventView);
    }
}
Error Pad::link(View<Pad> pad) {
    if (!pad) {
        return Error::InvalidArguments;
    }
    if (mType == Input) {
        // You can not link an Input pad
        return Error::InvalidArguments;
    }
    mNext = pad.get();
    return Error::Ok;
}
Error Pad::unlink() {
    mNext = nullptr;
    return Error::Ok;
}
void Pad::setCallback(Callback &&callback) {
    NEKO_ASSERT(mType == Input);
    mCallback = std::move(callback);
}
void Pad::setEventCallback(EventCallback &&callback) {
    NEKO_ASSERT(mType == Input);
    mEventCallback = std::move(callback);
}
void Pad::setName(std::string_view name) {
    mName = name;
}

NEKO_NS_END