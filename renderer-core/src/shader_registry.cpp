#include "dsrrl/core/shader_registry.hpp"

#include <algorithm>

namespace dsrrl::core {
namespace {

bool digest_is_zero(const sha256_digest &digest) noexcept
{
    return std::all_of(
        digest.begin(),
        digest.end(),
        [](std::uint8_t value) { return value == 0u; });
}

} // namespace

bool shader_registry::register_recipe(const shader_recipe &recipe)
{
    if (recipe.key.source_hash == 0 ||
        digest_is_zero(recipe.key.source_sha256) ||
        recipe.key.receiver_id == 0 ||
        (recipe.key.enabled_operators & ~all_operator_bits) != 0u ||
        recipe.key.enabled_operators == 0u ||
        recipe.replacement_hash == 0 ||
        digest_is_zero(recipe.replacement_sha256) ||
        recipe.carrier_abi == 0u)
        return false;

    std::lock_guard lock(mutex_);
    const auto [it, inserted] = recipes_.emplace(recipe.key, recipe);
    if (inserted)
        return true;

    return it->second.replacement_hash == recipe.replacement_hash &&
           it->second.replacement_sha256 == recipe.replacement_sha256 &&
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
