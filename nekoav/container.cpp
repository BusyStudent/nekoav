#define _NEKO_SOURCE
#include "container.hpp"
#include "factory.hpp"
#include "pad.hpp"
#include <map>

NEKO_NS_BEGIN

Vec<View<Element> > topologySort(const View<Container> &container) {
    std::map<View<Element>, int> inDeg;
    container->forElements([inDeg](View<Element> element) mutable {
        inDeg.insert(std::make_pair(element, 0));
        return true;
    });
    // TODO:这里outputs里面的节点是不是有可能不是这个图里面的节点
    for (auto [element, _] : inDeg) {
        auto inputs = element->outputs();
        for (auto pad : inputs) {
            auto next = pad->next();
            if (next != nullptr && next->element() != nullptr) {
                inDeg[next->element()] ++;
            }
        }
    }
    // Push all nodes with indegree 0 to the queue
    Vec<View<Element>> elements;
    for (const auto [key, value] : inDeg) {
        if (value == 0) {
            elements.push_back(key);
        }
    }
    // Traverse the queue and delete in-degree points
    int index = 0;
    while (index < elements.size()) {
        for (const auto &pad : elements[index]->outputs()) {
            auto next = pad->next();
            if (next != nullptr && next->element() != nullptr) {
                inDeg[next->element()] --;
                if (inDeg[next->element()] == 0) {
                    // Push node when indegree == 0
                    elements.push_back(next->element());
                }
            }
        }
        index ++;
    }
    // Topological sorting is completed if and only if the number of topological sorting nodes is equal to the number of source nodes.
    if (elements.size() == inDeg.size()) {
        return elements;
    }
    return Vec<View<Element>>();
}

class ContainerImpl final : public Container {
public:
    Error addElement(View<Element> element) override {
        if (!element) {
            return Error::InvalidArguments;
        }
        element->setBus(bus());
        element->setContext(context());
        mElements.push_back(element->shared_from_this());
        return Error::Ok;
    }
    Error removeElement(View<Element> element) override {
        if (!element) {
            return Error::InvalidArguments;
        }
        auto it = std::find_if(mElements.begin(), mElements.end(), [&](auto &&v) {
            return v.get() == element.get();
        });
        if (it == mElements.end()) {
            return Error::InvalidArguments;
        }
        element->setBus(nullptr);
        element->setContext(nullptr);
        mElements.erase(it);
        return Error::Ok;
    }
    Error forElements(const std::function<bool (View<Element>)> &cb) override {
        if (!cb) {
            return Error::InvalidArguments;
        }
        for (const auto &element : mElements) {
            if (!cb(element)) {
                break;
            }
        }
        return Error::Ok;
    }
    Error sendEvent(View<Event> event) override {
        for (const auto &element : mElements) {
            auto err = element->sendEvent(event);
            if (err != Error::Ok) {
                return err;
            }
        }
        return Error::Ok;
    }
    Error changeState(StateChange change) override {
        for (const auto &element : mElements) {
            auto err = element->setState(GetTargetState(change));
            if (err != Error::Ok) {
                return err;
            }
        }
        return Error::Ok;
    }
    size_t size() override {
        return mElements.size();
    }
private:
    Vec<Arc<Element> > mElements;
};

inline Vec<Element *> GetElements(View<Container> container) {
    Vec<Element *> elements;
    auto err = container->forElements([&](View<Element> e) {
        elements.push_back(e.get());
        return true;
    });
    if (err != Error::Ok) {
        elements.clear();
    }
    return elements;
} 

std::string DumpTopology(View<Container> container) {
    if (!container) {
        return std::string();
    }
    auto elements = topologySort(container);
    if (elements.empty()) {
        return std::string();
    }

    std::map<Element *, std::string> elementIds;
    auto randId = [a = 0]() mutable {
        a ++;
        return std::to_string(a);
    };
    auto markElement = [](Element *elem) {
        if (elem->inputs().empty()) {
            // Use circle
            return "((" + elem->name() + "))";
        }
        if (elem->outputs().empty()) {
            return '{' + elem->name() + '}';
        }
        return '[' + elem->name() + ']';
    };

    std::string result {"graph LR\n"};
    for (const auto &el : elements) {
        if (!elementIds.contains(el.get())) {
            elementIds[el.get()] = randId();
        }
        for (auto out : el->outputs()) {
            if (!out->isLinked()) {
                continue;
            }
            auto next = out->next()->element();
            if (!elementIds.contains(next)) {
                elementIds[next] = randId();
            }

            // Generate A[Square Rect] -- Link text --> B((Circle))
            result += "    ";
            result += elementIds[el.get()];
            result += markElement(el.get());

            result += " -- ";
            result += out->name();
            result += " to ";
            result += out->next()->name();
            
            result += " --> ";
            result += elementIds[next];
            result += markElement(next);
            result += "\n";
        }
    }

    return result;
}

Arc<Container> CreateContainer() {
    return make_shared<ContainerImpl>();
}

bool HasCycle(View<Container> container) {
    auto elements = topologySort(container);
    for (auto element : elements) {
        auto ptr = element.viewAs<Container>();
        if (ptr != nullptr) {
            if (HasCycle(ptr)) {
                return true;
            }
        }
    }
    if (elements.size() != container->size()) {
        return true;
    }
    return false;
}

NEKO_REGISTER_ELEMENT(Container, ContainerImpl);

NEKO_NS_END