#include <nekoav/context.hpp>
#include "log.hpp"

namespace nekoav {

Context::Context() = default;
Context::~Context() = default;

auto Context::findObject(std::string_view name) const -> std::shared_ptr<void> {
    auto it = mSlots.find(name);
    if (it == mSlots.end()) {
        return {};
    }
    return it->second;
}

auto Context::insertObject(std::string_view name, std::shared_ptr<void> object) -> void {
    NEKOAV_INFO("[Context] Inserting '{}': {}", name, object.get());
    mSlots.insert({std::string {name}, object});
}

} // namespace nekoav