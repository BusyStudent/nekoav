#define _NEKO_SOURCE
#include "time.hpp"
#include "pad.hpp"

NEKO_NS_BEGIN

// Pad here
Pad::Pad(Element *master, Type type, std::string_view name) : mElement(master), mType(type), mName(name) {
    assert(master != nullptr);
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
    NEKO_TRACE_TIME {
        return mNext->mCallback(resourceView);
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
    assert(mType == Input);
    mCallback = std::move(callback);
}
void Pad::setName(std::string_view name) {
    mName = name;
}

NEKO_NS_END