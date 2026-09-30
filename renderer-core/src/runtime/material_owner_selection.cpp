#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <optional>

namespace dsrrl::runtime {
namespace {
thread_local std::optional<operators::material_response::material_identity> g_current{};
std::atomic<std::uint64_t> g_selector_events{0};
std::atomic<std::uint64_t> g_accepted_callers{0};
std::atomic<std::uint64_t> g_owner_enriched{0};
std::atomic<std::uint64_t> g_owner_authenticated{0};
std::atomic<std::uint64_t> g_actual_material_authenticated{0};
std::atomic<std::uint64_t> g_fail_open{0};

struct owner_auth_cache_entry {
    core::sha256_digest flver_sha256{};
    std::uint32_t material_slot = 0u;
    std::uint64_t semantic_name_hash = 0u;
    bool occupied = false;
    bool authenticated = false;
};

constexpr std::size_t k_owner_auth_cache_slots = 256u;
thread_local std::array<
    owner_auth_cache_entry,
    k_owner_auth_cache_slots>
    g_owner_auth_cache{};

std::uint64_t owner_cache_entropy(
    const operators::material_response::
        material_identity &identity) noexcept
{
    if (identity.flver_identity_hash != 0u)
        return identity.flver_identity_hash;

    std::uint64_t folded = 0u;
    static_assert(
        sizeof(folded) <=
        core::sha256_digest{}.size());
    std::memcpy(
        &folded,
        identity.flver_sha256.data(),
        sizeof(folded));
    return folded;
}

std::size_t owner_auth_cache_index(
    const operators::material_response::
        material_identity &identity) noexcept
{
    // The legacy 64-bit token is optional. When absent, a cheap 64-bit fold
    // from the authoritative SHA is bucket entropy only; the complete SHA-256
    // remains the cache-hit authority below.
    std::uint64_t h =
        owner_cache_entropy(identity) ^
        (static_cast<std::uint64_t>(identity.material_slot) *
         0x9E3779B185EBCA87ULL);
    h ^= identity.semantic_name_hash +
         0x9E3779B97F4A7C15ULL +
         (h << 6u) +
         (h >> 2u);

    return static_cast<std::size_t>(
        h & (k_owner_auth_cache_slots - 1u));
}

bool owner_tuple_authenticated_cached(
    const operators::material_response::
        material_identity &identity) noexcept
{
    auto &cached =
        g_owner_auth_cache[
            owner_auth_cache_index(
                identity)];

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

    const bool exact_runtime_material =
        operators::material_response::
            exact_runtime_material_response_identity(
                identity);

    const bool flver_shape_valid =
        identity.valid &&
        identity.owner_tuple_exact &&
        identity.material_slot_valid &&
        identity.semantic_name_hash != 0u;

    if (!exact_runtime_material &&
        !flver_shape_valid) {
        telemetry::hot_count(g_fail_open);
        g_current.reset();
        return false;
    }

    telemetry::hot_count(g_owner_enriched);

    if (!exact_runtime_material &&
        !owner_tuple_authenticated_cached(
            identity)) {
        telemetry::hot_count(g_fail_open);
        g_current.reset();
        return false;
    }

    if (exact_runtime_material)
        telemetry::hot_count(
            g_actual_material_authenticated);
    else
        telemetry::hot_count(
            g_owner_authenticated);

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
        g_actual_material_authenticated.load(),
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
    g_actual_material_authenticated.store(0);
    g_fail_open.store(0);
    g_owner_auth_cache = {};
}

} // namespace dsrrl::runtime
