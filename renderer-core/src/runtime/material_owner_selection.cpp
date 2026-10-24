#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <optional>
#include <cstdio>


namespace dsrrl::runtime {
namespace {
thread_local std::optional<operators::material_response::material_identity> g_current{};
std::atomic<std::uint64_t> g_selector_events{0};
std::atomic<std::uint64_t> g_accepted_callers{0};
std::atomic<std::uint64_t> g_owner_enriched{0};
std::atomic<std::uint64_t> g_owner_authenticated{0};
std::atomic<std::uint64_t> g_actual_material_authenticated{0};
std::atomic<std::uint64_t> g_fail_open{0};
std::atomic<warrior_9360_log_sink> g_warrior_log_sink{nullptr};

#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
struct warrior_9360_model {
    const char *name;
    core::sha256_digest exact_flver;
};
// From source-verified DSR parts/*.partsbnd FLVER bytes, not a
// texture/MTD-name prefix heuristic and NOT a positive bridge allowlist.
constexpr std::array<warrior_9360_model, 8u> k_warrior_9360_models{{
    { "AM_F_9360", {{ 0x1eu, 0x1au, 0x21u, 0xb2u, 0xcau, 0x8du, 0x0eu, 0xe0u, 0x58u, 0x22u, 0xedu, 0xd8u, 0xa8u, 0x98u, 0xa0u, 0xd3u, 0x37u, 0xb2u, 0xe0u, 0x46u, 0x87u, 0xa0u, 0x58u, 0x87u, 0xf1u, 0x19u, 0xaeu, 0xf0u, 0x88u, 0x57u, 0x16u, 0x6fu }} },
    { "AM_M_9360", {{ 0xa1u, 0x15u, 0xd8u, 0x9eu, 0xb9u, 0xf7u, 0x02u, 0x65u, 0x03u, 0x8cu, 0x58u, 0xccu, 0x3au, 0x48u, 0x28u, 0x5cu, 0x41u, 0x90u, 0x13u, 0xf4u, 0xb6u, 0x16u, 0x5bu, 0x07u, 0xcdu, 0x1cu, 0x9au, 0x17u, 0x24u, 0x49u, 0xfdu, 0xeeu }} },
    { "BD_F_9360", {{ 0xcfu, 0x79u, 0xf8u, 0x45u, 0x0du, 0xdau, 0xa8u, 0xf1u, 0xc2u, 0xa2u, 0xccu, 0xeau, 0x55u, 0x4au, 0x01u, 0x6du, 0xafu, 0x84u, 0xfbu, 0x04u, 0x27u, 0x83u, 0x08u, 0x49u, 0xceu, 0x7du, 0x26u, 0x59u, 0x2cu, 0xb7u, 0x59u, 0x89u }} },
    { "BD_M_9360", {{ 0xabu, 0x9du, 0xf5u, 0x1cu, 0x28u, 0x4bu, 0x67u, 0xf6u, 0x0fu, 0x40u, 0xbeu, 0x95u, 0x84u, 0x03u, 0xf7u, 0x90u, 0x6bu, 0x8fu, 0xb4u, 0x92u, 0xa1u, 0x58u, 0xc6u, 0xe9u, 0xf1u, 0x0bu, 0x94u, 0x4cu, 0x63u, 0x39u, 0xdau, 0x79u }} },
    { "HD_F_9360", {{ 0x59u, 0xc9u, 0x37u, 0xceu, 0xd4u, 0xaau, 0xf4u, 0x1bu, 0xa3u, 0xf6u, 0xdbu, 0x4au, 0x35u, 0x1du, 0x47u, 0xb4u, 0x1du, 0xabu, 0xd1u, 0x76u, 0x22u, 0x4eu, 0xccu, 0xa4u, 0x12u, 0x3bu, 0x72u, 0xa1u, 0x1du, 0x23u, 0xbau, 0xeeu }} },
    { "HD_M_9360", {{ 0xbfu, 0xbeu, 0xf6u, 0x3eu, 0x96u, 0x11u, 0xbdu, 0x8bu, 0x75u, 0x3fu, 0x81u, 0x97u, 0xe5u, 0xcau, 0xbeu, 0xabu, 0xbfu, 0x08u, 0xecu, 0x40u, 0x0eu, 0x09u, 0x8fu, 0x21u, 0x91u, 0x56u, 0x1bu, 0xdcu, 0x5cu, 0xe7u, 0x39u, 0x6bu }} },
    { "LG_F_9360", {{ 0xddu, 0x2bu, 0x10u, 0x6au, 0xabu, 0x42u, 0x34u, 0x17u, 0x96u, 0x0fu, 0xa9u, 0x62u, 0x39u, 0xeeu, 0xf6u, 0x9cu, 0xabu, 0x5bu, 0x8cu, 0x70u, 0xecu, 0x79u, 0x5au, 0xf3u, 0x2cu, 0xc8u, 0x78u, 0x38u, 0x6au, 0x3bu, 0xfbu, 0x1eu }} },
    { "LG_M_9360", {{ 0xadu, 0x7du, 0x88u, 0x8au, 0xa5u, 0xcau, 0xeeu, 0x97u, 0x94u, 0xd8u, 0xeau, 0x5eu, 0x39u, 0x26u, 0x5du, 0xf2u, 0xc9u, 0xd3u, 0xc7u, 0xc8u, 0xfau, 0xfcu, 0xdfu, 0x2du, 0x94u, 0xffu, 0xebu, 0x22u, 0xabu, 0x55u, 0x24u, 0x13u }} }
}};
constexpr std::size_t k_warrior_stages = 10u;
constexpr std::size_t k_warrior_slots = 8u;
std::array<std::atomic_bool,
    k_warrior_9360_models.size() * k_warrior_slots * k_warrior_stages>
    g_warrior_9360_logged{};

std::size_t warrior_model_id(const core::sha256_digest &sha) noexcept
{
    if (sha[0] == 0u) return k_warrior_9360_models.size();
    for (std::size_t i = 0u; i < k_warrior_9360_models.size(); ++i) {
        const auto &record = k_warrior_9360_models[i];
        if (record.exact_flver[0] == sha[0] &&
            record.exact_flver == sha)
            return i;
    }
    return k_warrior_9360_models.size();
}
#endif

struct owner_auth_cache_entry {
    core::sha256_digest flver_sha256{};
    std::uint32_t material_slot = 0u;
    std::uint64_t semantic_name_hash = 0u;
    bool occupied = false;
    bool authenticated = false;
};

constexpr std::size_t k_owner_auth_cache_sets = 128u;
constexpr std::size_t k_owner_auth_cache_ways = 4u;
constexpr std::size_t k_owner_auth_cache_slots =
    k_owner_auth_cache_sets *
    k_owner_auth_cache_ways;
thread_local std::array<
    owner_auth_cache_entry,
    k_owner_auth_cache_slots>
    g_owner_auth_cache{};
thread_local std::array<
    std::uint8_t,
    k_owner_auth_cache_sets>
    g_owner_auth_cache_victim{};

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

std::size_t owner_auth_cache_set(
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
        h & (k_owner_auth_cache_sets - 1u));
}

owner_auth_cache_entry *owner_auth_cache_hit(
    const operators::material_response::
        material_identity &identity) noexcept
{
    const auto set =
        owner_auth_cache_set(
            identity);
    const auto base =
        set *
        k_owner_auth_cache_ways;

    for (std::size_t way = 0u;
         way < k_owner_auth_cache_ways;
         ++way) {
        auto &entry =
            g_owner_auth_cache[
                base + way];
        if (entry.occupied &&
            entry.material_slot ==
                identity.material_slot &&
            entry.semantic_name_hash ==
                identity.semantic_name_hash &&
            entry.flver_sha256 ==
                identity.flver_sha256)
            return &entry;
    }

    return nullptr;
}

owner_auth_cache_entry &owner_auth_cache_slot(
    const operators::material_response::
        material_identity &identity) noexcept
{
    const auto set =
        owner_auth_cache_set(
            identity);
    const auto base =
        set *
        k_owner_auth_cache_ways;

    for (std::size_t way = 0u;
         way < k_owner_auth_cache_ways;
         ++way) {
        auto &entry =
            g_owner_auth_cache[
                base + way];
        if (!entry.occupied)
            return entry;
    }

    const auto victim =
        static_cast<std::size_t>(
            g_owner_auth_cache_victim[set]++ &
            static_cast<std::uint8_t>(
                k_owner_auth_cache_ways - 1u));

    return g_owner_auth_cache[
        base + victim];
}

bool owner_tuple_authenticated_cached(
    const operators::material_response::
        material_identity &identity) noexcept
{
    if (auto *cached =
            owner_auth_cache_hit(
                identity);
        cached != nullptr)
        return cached->authenticated;

    auto &cached =
        owner_auth_cache_slot(
            identity);

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

void warrior_9360_set_log_sink(warrior_9360_log_sink sink) noexcept
{
    g_warrior_log_sink.store(sink, std::memory_order_release);
}

bool material_owner_selection_peek(
    operators::material_response::material_identity &identity) noexcept
{
    identity = {};
    if (!g_current.has_value())
        return false;
    identity = *g_current;
    return true;
}

void warrior_9360_trace(
    warrior_9360_stage stage,
    const operators::material_response::material_identity &identity,
    std::uint32_t route_mask,
    std::uint32_t receiver_id,
    const operators::material_response::decision *decision) noexcept
{
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
    const auto model = warrior_model_id(identity.flver_sha256);
    const auto stage_idx = static_cast<std::size_t>(stage);
    if (model == k_warrior_9360_models.size() ||
        stage_idx >= k_warrior_stages)
        return;

    // The final bucket is reserved for invalid/unresolved slots; these
    // are precisely the selector failures that must NOT vanish from logs.
    // All 9360 FLVER models have known slot indices 0..6.
    const auto slot_bucket =
        identity.material_slot_valid &&
        identity.material_slot < k_warrior_slots - 1u
            ? static_cast<std::size_t>(identity.material_slot)
            : k_warrior_slots - 1u;
    const auto key = (model * k_warrior_slots +
        slot_bucket) * k_warrior_stages + stage_idx;
    if (g_warrior_9360_logged[key].exchange(
            true, std::memory_order_relaxed))
        return;

    static const char *const stage_names[k_warrior_stages] = {
        "selector_invalid", "selector_not_authenticated",
        "selector_published", "pre_route", "no_pipeline_route",
        "no_dynamic_route", "identity_reject", "identity_ready",
        "prepare_reject", "prepared"
    };
    char line[500]{};
    std::snprintf(line, sizeof(line),
        "[DSRRL WARRIOR 9360 FIRST CUT] model=%s slot=%u stage=%s "
        "route_mask=0x%02x mtd_route=%u rx=%u decision=%u "
        "owner_exact=%u slot_valid=%u actual_mtd=%u mat_valid=%u "
        "mtd_sha=%02x%02x%02x%02x pixel_pass=0",
        k_warrior_9360_models[model].name,
        static_cast<unsigned>(identity.material_slot),
        stage_names[stage_idx],
        static_cast<unsigned>(route_mask),
        static_cast<unsigned>(identity.route_index),
        static_cast<unsigned>(receiver_id),
        decision != nullptr ?
            static_cast<unsigned>(decision->reason) : 255u,
        identity.owner_tuple_exact ? 1u : 0u,
        identity.material_slot_valid ? 1u : 0u,
        identity.actual_material_exact ? 1u : 0u,
        identity.valid ? 1u : 0u,
        identity.raw_mtd_sha256[0],
        identity.raw_mtd_sha256[1],
        identity.raw_mtd_sha256[2],
        identity.raw_mtd_sha256[3]);
    if (const auto log =
            g_warrior_log_sink.load(std::memory_order_acquire))
        log(line);
#else
    (void)stage; (void)identity; (void)route_mask;
    (void)receiver_id; (void)decision;
#endif
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
        warrior_9360_trace(
            warrior_9360_stage::selector_invalid,
            identity, 0u, 0u);
        g_current.reset();
        return false;
    }

    telemetry::hot_count(g_owner_enriched);

    if (!exact_runtime_material &&
        !owner_tuple_authenticated_cached(
            identity)) {
        telemetry::hot_count(g_fail_open);
        warrior_9360_trace(
            warrior_9360_stage::selector_not_authenticated,
            identity, 0u, 0u);
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
    warrior_9360_trace(
        warrior_9360_stage::selector_published,
        identity, 0u, 0u);
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
    g_owner_auth_cache_victim = {};
}

} // namespace dsrrl::runtime
