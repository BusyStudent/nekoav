#pragma once

#include <nekoav/element.hpp>
#include <ilias/sync/mpsc.hpp>
#include <vector>

namespace nekoav {

// MARK: Bin
enum class FindChildren {
    Recursively, // Contains children of children
    Directly     // Only contains direct children
};

/**
 * @brief The Bin element can contain multiple child elements and manage them as a single unit.
 * 
 */
class NEKOAV_API Bin : public Element {
public:
    using Ptr = std::shared_ptr<Bin>;

    Bin(std::string_view name = {});
    ~Bin();

    /**
     * @brief Add an element to the bin
     * 
     * @param element The shared_ptr of the element (if nullptr, no-op)
     */
    auto addElement(Element::Ptr element) -> void;

    /**
     * @brief Add many elements to the bin
     * 
     * @tparam Args 
     * @param elements 
     */
    template <typename ...Args>
    auto addElements(Args &&...elements) -> void {
        (addElement(std::forward<Args>(elements)), ...);
    }

    /**
     * @brief Add an element to the bin and sync the new Element to the bin state
     * 
     * @param element The shared_ptr of the element (if nullptr, no-op)
     * @return IoTask<void> 
     */
    auto addElementSync(Element::Ptr element) -> IoTask<void>;

    /**
     * @brief Remove an element from the bin
     * 
     * @param element The shared_ptr of the element (if nullptr, no-op)
     * @return true We successfully removed the element
     * @return false Not removed (maybe the element is not in the bin)
     */
    auto removeElement(Element::Ptr element) -> bool;

    /**
     * @brief Sync the state of all child elements to the current state of the bin.
     * 
     * @return IoTask<void> 
     */
    auto syncElements() -> IoTask<void>;

    /**
     * @brief Set all child elements to the Null state and clear all child elements
     * 
     * @return IoTask<void> 
     */
    auto clear() -> IoTask<void>;

    /**
     * @brief Send the event to all child elements
     * 
     * @param event The event to be sent
     * @return IoTask<void> 
     */
    auto sendEvent(Event event) -> IoTask<void> override;

    /**
     * @brief Check the bin is empty
     * 
     * @return true 
     * @return false 
     */
    auto empty() const -> bool { return mChildren.empty(); }

    /**
     * @brief Get all sinks child elements
     * 
     * @return std::vector<Element::Ptr> 
     */
    auto sinks(FindChildren option = FindChildren::Recursively) const -> std::vector<Element::Ptr>;
protected:
    // Sort, return false on Cycle detected
    auto topologicalSort() -> bool;

    // State management
    auto onInitialize() -> IoTask<void> override;
    auto onPrepare() -> IoTask<void> override;
    auto onRun() -> IoTask<void> override;
    auto onPause() -> IoTask<void> override;
    auto onStop() -> IoTask<void> override;
    auto onTeardown() -> IoTask<void> override;

    // When an message was post by children
    // The default implementation is to post the message to the parent, override it if you want to filter it
    // This methold implemented must be thread-safe
    virtual auto onChildMessage(Message message) -> void;

    // When topology changes, this method will changed
    virtual auto onTopologyChange() -> void;
private:
    // Dump
    auto dumpInfoInternal(FILE *where, int level) -> void override;
    auto setChildrenState(State newState) -> IoTask<void>;

    // Child elements
    // from Source -> Sink
    std::vector<Element::Ptr> mChildren;
    bool                      mSorted = false;
friend class Pipeline;
friend class Element;
friend class Pad;
};

} // namespace nekoav