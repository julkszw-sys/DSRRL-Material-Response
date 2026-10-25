#pragma once
// Read-only V382 diagnostic. Source and renderer operation are unchanged.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <reshade.hpp>
#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"

namespace dsrrl::runtime {
inline void v382_source_owner_witness(
    const operators::material_response::material_identity &material,
    const pmetal_envspec_source &source,
    unsigned source_kind, bool fallback_tls, bool selector_valid,
    unsigned consumer_thread) noexcept
{
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
    if (!telemetry::effect_enabled() ||
        !material.valid || !material.owner_tuple_exact ||
        !material.material_slot_valid)
        return;
    // Observation filter only. Row 0/5 is NOT an admission/override rule.
    if (source.row_id_a != 0u && source.row_id_a != 5u &&
        source.row_id_b != 0u && source.row_id_b != 5u)
        return;

    struct entry {
        std::atomic<std::uint64_t> owner{0u};
        std::atomic<std::uint64_t> last_source{0u};
    };
    static std::array<entry,2048u> observed{};
    static std::atomic<unsigned> log_count{0u};
    const std::uint64_t owner_key = (
        material.flver_identity_hash ^
        (material.semantic_name_hash * 0x9e3779b97f4a7c15ull) ^
        (static_cast<std::uint64_t>(material.route_index) << 33u) ^
        (static_cast<std::uint64_t>(material.material_slot) << 17u)) | 1ull;
    const std::uint64_t source_key = (
        source.bank_signature_a ^
        (source.bank_signature_b * 0x9e3779b97f4a7c15ull) ^
        (static_cast<std::uint64_t>(source.row_id_a) << 37u) ^
        (static_cast<std::uint64_t>(source.row_id_b) << 23u) ^
        (static_cast<std::uint64_t>(source_kind) << 11u) ^
        (fallback_tls ? 0x7edcba98ull : 0u)) | 1ull;
    entry *chosen = nullptr;
    for (std::size_t i=0u; i<8u; ++i) {
        auto &candidate = observed[(owner_key+i)%observed.size()];
        auto key = candidate.owner.load(std::memory_order_relaxed);
        if (key == 0u) {
            std::uint64_t empty=0u;
            if (candidate.owner.compare_exchange_strong(
                    empty,owner_key,std::memory_order_relaxed))
                key=owner_key;
            else
                key=empty;
        }
        if (key == owner_key) {chosen=&candidate;break;}
    }
    if (chosen==nullptr)return; // bounded sampling; never a renderer veto
    const auto previous=chosen->last_source.exchange(
        source_key,std::memory_order_relaxed);
    if (previous==source_key)return;
    const auto count=log_count.fetch_add(1u,std::memory_order_relaxed)+1u;
    if (count>4096u)return; // bounded output, not an eligibility check

    char line[650]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V382 SOURCE OWNER READONLY] n=%u flver=%016llx "
        "semantic=%016llx route=%u slot=%u kind=%u "
        "hook_transport=%s hook_selector_valid=%u tid=%u "
        "bank=%016llx row=%u bank_b=%016llx row_b=%u "
        "serial=%llu generation=%llu beta=%.9g "
        "c86=%.8g,%.8g,%.8g linear_valid=%u "
        "actor_join_proven=0 pixel=OPEN",
        count,
        static_cast<unsigned long long>(material.flver_identity_hash),
        static_cast<unsigned long long>(material.semantic_name_hash),
        static_cast<unsigned>(material.route_index),
        static_cast<unsigned>(material.material_slot), source_kind,
        source_kind!=2u?"source1":fallback_tls?"tls":"global",
        selector_valid?1u:0u,consumer_thread,
        static_cast<unsigned long long>(source.bank_signature_a),
        source.row_id_a,
        static_cast<unsigned long long>(source.bank_signature_b),
        source.row_id_b,
        static_cast<unsigned long long>(source.serial),
        static_cast<unsigned long long>(source.generation),
        static_cast<double>(source.beta),
        static_cast<double>(source.envdiffuse_a[0]),
        static_cast<double>(source.envdiffuse_a[1]),
        static_cast<double>(source.envdiffuse_a[2]),
        source.envdiffuse_linear_valid?1u:0u);
    reshade::log::message(reshade::log::level::warning,line);
#else
    (void)material; (void)source; (void)source_kind;
    (void)fallback_tls; (void)selector_valid; (void)consumer_thread;
#endif
}
} // namespace dsrrl::runtime
