#define _NEKO_SOURCE
#include "elements.hpp"
#include "time.hpp"
#include "pad.hpp"
#include "log.hpp"
#include "libc.hpp"
#include <algorithm>

#ifdef __cpp_lib_format
    #include <format>
#endif

NEKO_NS_BEGIN

Element::Element() {

}
Element::~Element() {
    removeAllInputs();
    removeAllOutputs();
}

Error Element::setState(State newState) {
    if (state() == newState) {
        NEKO_DEBUG(name());
        NEKO_DEBUG("Same state");
        NEKO_DEBUG(state());
        NEKO_DEBUG(newState);
        return Error::Ok;
    }
    auto vec = ComputeStateChanges(state(), newState);
    if (vec.empty()) {
        return Error::InvalidArguments;
    }
    for (auto change : vec) {
        if (auto err = changeState(change); err != Error::Ok) {
            NEKO_DEBUG(name());
            NEKO_DEBUG("Error to");
            NEKO_DEBUG(err);
            return err;
        }
    }
    mState = newState;
    return Error::Ok;
}
Error Element::setBus(EventSink *newBus) {
    if (state() != State::Null) {
        return Error::InvalidState;
    }
    mBus = newBus;
    return Error::Ok;
}
Error Element::setContext(Context *newContext) {
    if (state() != State::Null) {
        return Error::InvalidState;
    }
    mContext = newContext;
    return Error::Ok;
}
void  Element::setName(std::string_view name) {
    mName = name;
}
std::string Element::name() const {
    if (!mName.empty()) {
        return mName;
    }
    char buffer [256] = {0};
    auto typename_ = libc::typenameof(typeid(*this));
    ::snprintf(buffer, sizeof(buffer), "%s-%p", typename_, this);
    return std::string(buffer);
}
Pad *Element::findInput(std::string_view name) const {
    auto it = std::find_if(mInputs.begin(), mInputs.end(), [&](auto &p) { return p->name() == name; });
    if (it == mInputs.end()) {
        return nullptr;
    }
    return *it;
}
Pad *Element::findOutput(std::string_view name) const {
    auto it = std::find_if(mOutputs.begin(), mOutputs.end(), [&](auto &p) { return p->name() == name; });
    if (it == mOutputs.end()) {
        return nullptr;
    }
    return *it;
}
Pad *Element::addInput(std::string_view name) {
    auto pad = new Pad(this, Pad::Input, name);
    addPad(pad);
    return pad;
}
Pad *Element::addOutput(std::string_view name) {
    auto pad = new Pad(this, Pad::Output, name);
    addPad(pad);
    return pad;
}
void Element::addPad(Pad *pad) {
    if (!pad) {
        return;
    }
    if (pad->type() == Pad::Input) {
        mInputs.push_back(pad);
    }
    else {
        mOutputs.push_back(pad);
    }
}
void Element::removePad(Pad *pad) {
    if (!pad) {
        return;
    }
    if (pad->type() == Pad::Input) {
        auto it = std::find(mInputs.begin(), mInputs.end(), pad);
        if (it != mInputs.end()) {
            mInputs.erase(it);
            delete pad;
        }
    }
    else {
        auto it = std::find(mOutputs.begin(), mOutputs.end(), pad);
        if (it != mOutputs.end()) {
            mOutputs.erase(it);
            delete pad;
        }
    }
}
void Element::removeAllInputs() {
    for (const auto ins : mInputs) {
        delete ins;
    }
    mInputs.clear();
}
void Element::removeAllOutputs() {
    for (const auto outs : mOutputs) {
        delete outs;
    }
    mOutputs.clear();
}

// Another
Error LinkElements(std::initializer_list<View<Element> > elements) {
    if (elements.size() < 2) {
        return Error::InvalidArguments;
    }
    for (size_t i = 0; i < elements.size() - 1; ++i) {
        // auto srcElement = elements[i];
        // auto dstElement = elements[i + 1];
        auto srcElement = *(elements.begin() + i);
        auto dstElement = *(elements.begin() + i + 1);

        auto srcPad = srcElement->findOutput("src");
        auto dstPad = dstElement->findInput("sink");

        if (!srcPad || !dstPad) {
            return Error::InvalidArguments;
        }

        if (auto err = srcPad->link(dstPad); err != Error::Ok) {
            return err;
        }
    }
    return Error::Ok;
}
Error LinkElement(View<Element> src, std::string_view srcPad, View<Element> dst, std::string_view dstPad) {
    if (!src || !dst || srcPad.empty() || dstPad.empty()) {
        return Error::InvalidArguments;
    }
    auto spad = src->findOutput(srcPad);
    auto dpad = dst->findInput(dstPad);
    if (!spad || !dpad) {
        return Error::InvalidArguments;
    }
    return spad->link(dpad);
}

NEKO_NS_END