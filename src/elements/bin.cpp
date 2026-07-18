#include <nekoav/elements/bin.hpp>
#include <nekoav/element.hpp>
#include <nekoav/error.hpp>
#include <queue>
#include "log.hpp"

namespace nekoav {

// MARK: Bin
Bin::Bin(std::string_view name) : Element(ElementType::Bin, name) {

}

Bin::~Bin() {

}

// Emm? maybe we should make setState to virtual ?
auto Bin::addElement(Element::Ptr element) -> void {
    if (!element || element->mParent) { // Didn't add an empty element or an element already in a bin
        return;
    }
    assert(!element->mIsPipeline); // Can't add an pipeline to a bin
    // Set the member belong the bin
    element->mParent = this;
    element->setClock(clock());
    element->setContext(context());
    mChildren.emplace_back(std::move(element));
    mSorted = false;
    onTopologyChange();
}

auto Bin::addElementSync(Element::Ptr element) -> IoTask<void> {
    if (!element || element->mParent) {
        co_return Err(Error::InvalidArguments);
    }
    addElement(element);
    // Async state here
    if (auto res = co_await element->setState(state()); !res) {
        removeElement(element);
        co_return Err(res.error());
    }
    co_return {};
}

auto Bin::removeElement(Element::Ptr element) -> bool {
    if (!element) {
        return false;
    }
    auto it = std::ranges::find(mChildren, element);
    if (it == mChildren.end()) {
        return false;
    }
    if ((*it)->state() != State::Null) {
        NEKOAV_ERROR("[Bin] '{}' tried to remove an element not in Null state, please use setState to set it to Null state first", name());
        return false;
    }

    // Remove the member belong the bin
    (*it)->mParent = nullptr;
    (*it)->setClock({});
    (*it)->setContext({});
    mChildren.erase(it);
    mSorted = false;
    onTopologyChange();
    return true;
}

auto Bin::syncElements() -> IoTask<void> {
    return setChildrenState(state());
}

auto Bin::clear() -> IoTask<void> {
    auto res = co_await setChildrenState(State::Null);
    mChildren.clear();
    co_return res;
}

auto Bin::sendEvent(Event event) -> IoTask<void> {
    std::vector<IoTask<void> > tasks {};
    for (auto &child : mChildren) {
        tasks.emplace_back(child->sendEvent(event));
    }
    auto res = co_await ilias::whenAll(std::move(tasks));
    auto it = std::ranges::find_if(res, [](auto &r) { return !r; });
    if (it != res.end()) {
        co_return Err(it->error());
    }
    co_return {};
}

auto Bin::dumpInfoInternal(FILE * where, int level) -> void {
    Element::dumpInfoInternal(where, level);
    ::fprintf(where, "%*s  Children:\n", level, "");
    for (auto &child : mChildren) {
        child->dumpInfoInternal(where, level + 4);
    }
}

auto Bin::onInitialize() -> IoTask<void> {
    NEKOAV_INFO("[Bin] '{}' initializing children", name());
    return setChildrenState(State::Ready);
}

auto Bin::onPrepare() -> IoTask<void> {
    NEKOAV_INFO("[Bin] '{}' preparing children", name());
    return setChildrenState(State::Paused);
}

auto Bin::onRun() -> IoTask<void> {
    NEKOAV_INFO("[Bin] '{}' running children", name());
    return setChildrenState(State::Running);
}

auto Bin::onPause() -> IoTask<void> {
    NEKOAV_INFO("[Bin] '{}' pausing children", name());
    return setChildrenState(State::Paused);
}

auto Bin::onStop() -> IoTask<void> {
    NEKOAV_INFO("[Bin] '{}' stopping children", name());
    return setChildrenState(State::Ready);
}

auto Bin::onTeardown() -> IoTask<void> {
    NEKOAV_INFO("[Bin] '{}' tearing down children", name());
    return setChildrenState(State::Null);
}

auto Bin::setChildrenState(State newState) -> IoTask<void> {
    // Check if we need to sort
    if (!mSorted) {
        if (!topologicalSort()) {
            co_return Err(Error::InvalidTopology);
        }
        mSorted = true;
        NEKOAV_INFO("[Bin] '{}' topological sort done", name());
    }
    // Check we are init(forward) or shutdown(backword)
    static_assert(std::to_underlying(State::Running) > std::to_underlying(State::Null));
    bool forward = std::to_underlying(newState) > std::to_underlying(state());
    if (forward) { // Forward
        for (auto &child : mChildren | std::views::reverse) { // From sink to source
            if (auto res = co_await child->setState(newState); !res) {
                co_return Err(res.error());
            }
        }
    }
    else { // Backward
        for (auto &child : mChildren) { // From source to sink
            if (auto res = co_await child->setState(newState); !res) { // Backward will ignore the error
                NEKOAV_WARN("[Bin] '{}' child '{}' failed to set state to '{}', error: {}", name(), child->name(), newState, res.error().message());
            }
        }
    }
    co_return {};
}

auto Bin::sinks(FindChildren option) const -> std::vector<Element::Ptr> {
    std::vector<Element::Ptr> sinks {};
    auto findSinks = [&](auto self, const Bin *bin) -> void {
        for (auto &child : bin->mChildren) {
            if (child->isSink()) { // Has input but no output
                sinks.push_back(child);
            }
            if (option != FindChildren::Recursively) {
                continue;
            }
            if (!child->isBin()) {
                continue;
            }
            self(self, static_cast<const Bin *>(child.get()));
        }
    };
    findSinks(findSinks, this);
    return sinks;
}

auto Bin::topologicalSort() -> bool {
    if (mChildren.empty()) {
        return true; // No children, no-op
    }

    // Init inDegrees...
    auto inDegrees = std::unordered_map<Element *, size_t>{};
    for (auto &child : mChildren) {
        inDegrees[child.get()] = 0;
    }

    for (auto &child : mChildren) {
        for (auto &output : child->outputs()) {
            if (output.isLinked()) {
                inDegrees[output.peerElement()] += 1;
            }
        }
    }

    // Topological sort
    auto sorted = std::vector<Element::Ptr>{};
    auto queue = std::queue<Element *>{};
    for (auto &[element, degree] : inDegrees) {
        if (degree == 0) {
            queue.push(element);
        }
    }

    while (!queue.empty()) {
        auto curElement = queue.front();
        queue.pop();

        sorted.push_back(curElement->shared_from_this());
        for (auto &output : curElement->outputs()) {
            if (!output.isLinked()) {
                continue;
            }
            auto peerElement = output.peerElement();
            auto &peerInDegree = inDegrees[peerElement];
            peerInDegree -= 1;
            if (peerInDegree == 0) {
                queue.push(peerElement);
            }
        }
    }

    // Check
    if (sorted.size() != mChildren.size()) {
        NEKOAV_ERROR("[Bin] '{}' topological sort failed, cycle detected", name());
        return false; // Circle detected
    }
    else {
        mChildren = std::move(sorted);
        return true;
    }
}

auto Bin::onTopologyChange() -> void {
    NEKOAV_DEBUG("[Bin] '{}' topology changed", name());
}

auto Bin::onChildMessage(Message message) -> void {
    postMessage(std::move(message));
}

} // namespace nekoav