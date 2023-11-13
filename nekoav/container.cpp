#define _NEKO_SOURCE
#include "container.hpp"
#include "factory.hpp"
#include "pad.hpp"

NEKO_NS_BEGIN

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
    Error detachElement(View<Element> element) override {
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
    auto elements = GetElements(container);
    if (elements.empty()) {
        return std::string();
    }

    std::map<Element *, std::string> elementIds;
    std::string currentId {"a"};
    auto randId = [&]() {
        currentId.back() += 1;
        if (currentId.back() == 'z') {
            currentId.push_back('a');
        }
        return currentId;
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
        if (!elementIds.contains(el)) {
            elementIds[el] = randId();
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
            result += elementIds[el];
            result += markElement(el);

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

NEKO_REGISTER_ELEMENT(Container, ContainerImpl);

NEKO_NS_END