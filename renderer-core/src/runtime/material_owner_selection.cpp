#include "dsrrl/runtime/material_owner_selection.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include <atomic>
#include <optional>

namespace dsrrl::runtime {
namespace {
thread_local std::optional<operators::material_response::material_identity> g_current{};
std::atomic<std::uint64_t> g_selector_events{0};
std::atomic<std::uint64_t> g_accepted_callers{0};
std::atomic<std::uint64_t> g_owner_enriched{0};
std::atomic<std::uint64_t> g_owner_authenticated{0};
std::atomic<std::uint64_t> g_fail_open{0};
}

void material_owner_selection_clear() noexcept
{
    g_current.reset();
}

bool material_owner_selection_publish(
    const operators::material_response::material_identity &identity) noexcept
{
    ++g_selector_events;
    if (!identity.valid ||
        !identity.owner_tuple_exact ||
        !identity.material_slot_valid ||
        identity.semantic_name_hash == 0u) {
        ++g_fail_open;
        g_current.reset();
        return false;
    }

    ++g_owner_enriched;

    if (!operators::material_response::generated::
            dsr_flver_owner_tuple_authenticated(
                identity.flver_sha256,
                identity.material_slot,
                identity.semantic_name_hash)) {
        ++g_fail_open;
        g_current.reset();
        return false;
    }

    ++g_owner_authenticated;
    ++g_accepted_callers;
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
}

} // namespace dsrrl::runtime
