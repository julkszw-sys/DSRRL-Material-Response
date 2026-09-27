#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace dsrrl::runtime {
namespace {
thread_local std::optional<operators::material_response::material_identity> g_current{};
std::atomic<std::uint64_t> g_selector_events{0};
std::atomic<std::uint64_t> g_accepted_callers{0};
std::atomic<std::uint64_t> g_owner_enriched{0};
std::atomic<std::uint64_t> g_owner_authenticated{0};
std::atomic<std::uint64_t> g_fail_open{0};

struct owner_auth_cache_entry {
    core::sha256_digest flver_sha256{};
    std::uint32_t material_slot = 0u;
    std::uint64_t semantic_name_hash = 0u;
    bool occupied = false;
    bool authenticated = false;
};

constexpr std::size_t k_owner_auth_cache_slots = 32u;
thread_local std::array<
    owner_auth_cache_entry,
    k_owner_auth_cache_slots>
    g_owner_auth_cache{};

std::size_t owner_auth_cache_index(
    const core::sha256_digest &digest,
    std::uint32_t material_slot,
    std::uint64_t semantic_name_hash) noexcept
{
    std::uint64_t h =
        0xcbf29ce484222325ULL;
    for (const auto byte : digest) {
        h ^= byte;
        h *= 0x100000001b3ULL;
    }

    h ^= material_slot;
    h *= 0x100000001b3ULL;
    h ^= semantic_name_hash;
    h *= 0x100000001b3ULL;

    return static_cast<std::size_t>(
        h % k_owner_auth_cache_slots);
}

bool owner_tuple_authenticated_cached(
    const operators::material_response::
        material_identity &identity) noexcept
{
    auto &cached =
        g_owner_auth_cache[
            owner_auth_cache_index(
                identity.flver_sha256,
                identity.material_slot,
                identity.semantic_name_hash)];

    if (cached.occupied &&
        cached.material_slot ==
            identity.material_slot &&
        cached.semantic_name_hash ==
            identity.semantic_name_hash &&
        cached.flver_sha256 ==
            identity.flver_sha256)
        return cached.authenticated;

    const bool authenticated =
        operators::material_response::generated::
            dsr_flver_owner_tuple_authenticated(
                identity.flver_sha256,
                identity.material_slot,
                identity.semantic_name_hash);

    cached = {
        identity.flver_sha256,
        identity.material_slot,
        identity.semantic_name_hash,
        true,
        authenticated
    };
    return authenticated;
}
}

void material_owner_selection_clear() noexcept
{
    g_current.reset();
}

bool material_owner_selection_publish(
    const operators::material_response::material_identity &identity) noexcept
{
    telemetry::hot_count(g_selector_events);
    if (!identity.valid ||
        !identity.owner_tuple_exact ||
        !identity.material_slot_valid ||
        identity.semantic_name_hash == 0u) {
        telemetry::hot_count(g_fail_open);
        g_current.reset();
        return false;
    }

    telemetry::hot_count(g_owner_enriched);

    if (!owner_tuple_authenticated_cached(
            identity)) {
        telemetry::hot_count(g_fail_open);
        g_current.reset();
        return false;
    }

    telemetry::hot_count(g_owner_authenticated);
    telemetry::hot_count(g_accepted_callers);
    g_current = identity;
    return true;
}

bool material_owner_selection_consume(
    operators::material_response::material_identity &identity) noexcept
{
    identity = {};
    if (!g_current.has_value())
        return false;

    identity = *g_current;
    g_current.reset();
    return true;
}

material_owner_selection_telemetry material_owner_selection_stats() noexcept
{
    return {
        g_selector_events.load(),
        g_accepted_callers.load(),
        g_owner_enriched.load(),
        g_owner_authenticated.load(),
        g_fail_open.load()
    };
}

void material_owner_selection_reset_stats() noexcept
{
    g_current.reset();
    g_selector_events.store(0);
    g_accepted_callers.store(0);
    g_owner_enriched.store(0);
    g_owner_authenticated.store(0);
    g_fail_open.store(0);
    g_owner_auth_cache = {};
}

} // namespace dsrrl::runtime
