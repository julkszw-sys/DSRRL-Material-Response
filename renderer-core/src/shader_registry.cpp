#include "dsrrl/core/shader_registry.hpp"

namespace dsrrl::core {

bool shader_registry::register_recipe(const shader_recipe &recipe)
{
    if (recipe.key.source_hash == 0 ||
        recipe.key.receiver_id == 0 ||
        recipe.replacement_hash == 0)
        return false;

    std::lock_guard lock(mutex_);
    const auto [it, inserted] = recipes_.emplace(recipe.key, recipe);
    if (inserted)
        return true;

    return it->second.replacement_hash == recipe.replacement_hash &&
           it->second.carrier_abi == recipe.carrier_abi;
}

std::optional<shader_recipe> shader_registry::resolve(const shader_key &key) const
{
    std::lock_guard lock(mutex_);
    const auto it = recipes_.find(key);
    if (it == recipes_.end())
        return std::nullopt;
    return it->second;
}

std::size_t shader_registry::size() const noexcept
{
    std::lock_guard lock(mutex_);
    return recipes_.size();
}

} // namespace dsrrl::core
