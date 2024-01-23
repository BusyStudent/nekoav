#define _NEKO_SOURCE
#include "event.hpp"
#include "time.hpp"
#include "libc.hpp"
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
    if (mType == pad->mType) {
        // Can not link in same types pad
        return Error::InvalidArguments;
    }
    if (mType == Input) {
        // Exchange
        return pad->link(this);
    }
    mNext = pad.get();
    mNext->mPrev = this;
    return Error::Ok;
}
Error Pad::unlink() {
    if (mPrev) {
        NEKO_ASSERT(mType == Input);
        mPrev->mNext = nullptr;
        mPrev = nullptr;
    }
    if (mNext) {
        NEKO_ASSERT(mType == Output);
        mNext->mPrev = nullptr;
        mNext = nullptr;
    }
    return Error::Ok;
}
void Pad::setCallback(Callback &&callback) {
    NEKO_ASSERT(mType == Input);
    mCallback = std::move(callback);
}
void Pad::setEventCallback(EventCallback &&callback) {
    // NEKO_ASSERT(mType == Input);
    mEventCallback = std::move(callback);
}
void Pad::setName(std::string_view name) {
    mName = name;
}
std::string Pad::toDocoument() const {
    std::string ret;
    libc::sprintf(&ret, "Pad %p\n", this);
    libc::sprintf(&ret, "    name: %s\n", mName.c_str());
    libc::sprintf(&ret, "    type: %s\n", mType == Input ? "input" : "output");
    libc::sprintf(&ret, "    next: %p\n", mNext);
    libc::sprintf(&ret, "    prev: %p\n", mPrev);
    ret += "    properties:\n";

    // Print properties
    for (const auto &[key, value] : mProperties) {
        libc::sprintf(&ret, "        %s: %s\n", key.c_str(), value.toDocoument().c_str());
    }
    return ret;
}

NEKO_NS_END