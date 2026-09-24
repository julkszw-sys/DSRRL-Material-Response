#include "dsrrl/core/receiver_registry.hpp"

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

bool receiver_registry::register_receiver(const receiver_descriptor &descriptor)
{
    if (descriptor.receiver_id == 0 ||
        descriptor.fast_hash == 0 ||
        digest_is_zero(descriptor.exact_sha256) ||
        (descriptor.capabilities & ~all_operator_bits) != 0u)
        return false;

    std::lock_guard lock(mutex_);

    for (const auto &existing : receivers_) {
        if (existing.receiver_id == descriptor.receiver_id)
            return existing.fast_hash == descriptor.fast_hash &&
                   existing.exact_sha256 == descriptor.exact_sha256 &&
                   existing.consumer_family_hash == descriptor.consumer_family_hash &&
                   existing.capabilities == descriptor.capabilities;
    }

    receivers_.push_back(descriptor);
    return true;
}

std::optional<receiver_descriptor> receiver_registry::resolve(
    std::uint64_t fast_hash,
    const sha256_digest &exact_sha256) const
{
    std::lock_guard lock(mutex_);
    std::optional<receiver_descriptor> result;

    for (const auto &candidate : receivers_) {
        if (candidate.fast_hash != fast_hash ||
            candidate.exact_sha256 != exact_sha256)
            continue;

        if (result.has_value())
            return std::nullopt;

        result = candidate;
    }

    return result;
}

std::size_t receiver_registry::size() const noexcept
{
    std::lock_guard lock(mutex_);
    return receivers_.size();
}

} // namespace dsrrl::core
