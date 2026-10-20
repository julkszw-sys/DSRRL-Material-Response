#pragma once
// V230 diagnostic-only, bounded fail-open ledger. No shader/resource/CB mutations.
// This file deliberately lives in include/ so the production release source
// cleanup never silently strips the diagnostic-only ReShade messages.
#include <reshade.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime::v230_failopen_diag {

enum class stage : std::uint8_t {
    create_stable_ok, create_stable_materialize_reject,
    create_stable_register_reject, create_lerp_ok,
    create_lerp_materialize_reject, create_lerp_register_reject,
    draw_identity_reject, draw_owner_reject, mr_registry_missing,
    mr_decision_inactive, pointlight_decision_inactive,
    pmetal_candidate_reject, pmetal_feature_disabled,
    pmetal_lerp_feature_disabled, pmetal_semantic_reject,
    pmetal_source_reject, pmetal_blend_reject,
    pmetal_context_reject, pmetal_shader_missing,
    pmetal_probe_reject, pmetal_spec_rgb_reject,
    pmetal_device_reject, pmetal_cb_create_reject,
    pmetal_cb_map_reject, pmetal_shadow_reject,
    pmetal_transaction_reject, pmetal_prepared_ok,
    probe_invalid_input, probe_null_srv, probe_snapshot_reject,
    probe_sampler_or_srv_missing, probe_native_binding_reject,
    draw_batch_reject, draw_transaction_reject, draw_issued,
    native_wrapper_armed, disabled_by_policy, count
};

struct description { const char *name; const char *layer; bool failure; };
inline constexpr std::array<description,
    static_cast<std::size_t>(stage::count)> k_descriptions = {{
    {"create_stable_ok","SHADER_CREATE",false},
    {"create_stable_materialize_reject","SHADER_CREATE",true},
    {"create_stable_register_reject","SHADER_CREATE",true},
    {"create_lerp_ok","SHADER_CREATE",false},
    {"create_lerp_materialize_reject","SHADER_CREATE",true},
    {"create_lerp_register_reject","SHADER_CREATE",true},
    {"draw_identity_reject","DRAW_ROUTING",true},
    {"draw_owner_reject","MATERIAL_OWNER",true},
    {"mr_registry_missing","MATERIAL_RESPONSE",true},
    {"mr_decision_inactive","MATERIAL_RESPONSE",true},
    {"pointlight_decision_inactive","POINTLIGHT",true},
    {"pmetal_candidate_reject","ENVSPEC_DRAW",true},
    {"pmetal_feature_disabled","ENVSPEC_POLICY",true},
    {"pmetal_lerp_feature_disabled","ENVSPEC_POLICY",true},
    {"pmetal_semantic_reject","ENVSPEC_SEMANTICS",true},
    {"pmetal_source_reject","ENVSPEC_LIGHTBANK",true},
    {"pmetal_blend_reject","ENVSPEC_SOURCE",true},
    {"pmetal_context_reject","ENVSPEC_D3D11",true},
    {"pmetal_shader_missing","ENVSPEC_SHADER",true},
    {"pmetal_probe_reject","ENVSPEC_PROBE",true},
    {"pmetal_spec_rgb_reject","MATERIAL_SPECRGB",true},
    {"pmetal_device_reject","ENVSPEC_D3D11",true},
    {"pmetal_cb_create_reject","ENVSPEC_B12",true},
    {"pmetal_cb_map_reject","ENVSPEC_B12",true},
    {"pmetal_shadow_reject","ENVSPEC_SHADOW",true},
    {"pmetal_transaction_reject","ENVSPEC_TX_PREP",true},
    {"pmetal_prepared_ok","ENVSPEC_DRAW",false},
    {"probe_invalid_input","ENVSPEC_PROBE",true},
    {"probe_null_srv","ENVSPEC_PROBE",true},
    {"probe_snapshot_reject","ENVSPEC_PROBE",true},
    {"probe_sampler_or_srv_missing","ENVSPEC_PROBE",true},
    {"probe_native_binding_reject","ENVSPEC_PROBE",true},
    {"draw_batch_reject","DRAW_BATCH",true},
    {"draw_transaction_reject","D3D11_TRANSACTION",true},
    {"draw_issued","D3D11_TRANSACTION",false},
    {"native_wrapper_armed","D3D11_NATIVE_WRAPPER",false},
    {"disabled_by_policy","POLICY_INTENTIONAL",false},
}};
inline constexpr std::size_t k_count = k_descriptions.size();
inline std::array<std::atomic<std::uint64_t>,k_count> g_counts{};
inline std::array<std::atomic<std::uint64_t>,k_count> g_last_report{};
inline std::array<std::atomic<std::uint32_t>,k_count * 3u * 2u> g_once{};
inline std::atomic<std::uint64_t> g_frames{0};
inline std::atomic<bool> g_banner{false};

inline void hit(stage what, std::uint32_t rx = 0u,
                std::uint32_t family = 0u, std::uint32_t route = 0u,
                std::uint64_t owner = 0u, std::uint32_t slot = 0u,
                std::uint32_t detail = 0u) noexcept
{
#if defined(DSRRL_V230_FAILOPEN_DIAGNOSTICS)
    const auto id = static_cast<std::size_t>(what);
    if (id >= k_count) return;
    g_counts[id].fetch_add(1u,std::memory_order_relaxed);
    // Per-reason x family x exact P_Metal-or-other x receiver, bounded.
    // Unknown RX uses bit0, RX33..35 bits1..3.
    const std::uint32_t rx_bit = rx >= 33u && rx <= 35u
        ? (1u << (rx-32u)) : 1u;
    const auto bucket = id * 6u + (family == 1u ? 3u : 0u)
                         + (route == 345u ? 1u : route != 0u ? 2u : 0u);
    if ((g_once[bucket].fetch_or(rx_bit,std::memory_order_relaxed)
         & rx_bit) != 0u) return;
    char line[456]{};
    const auto &d = k_descriptions[id];
    std::snprintf(line,sizeof(line),
        "[DSRRL FAILOPEN V230] event=%s layer=%s kind=%s family=%u rx=%u route=%u owner=%016llx slot=%u detail=%u first=1",
        d.name,d.layer,d.failure ? "REJECT" : "OBSERVED",
        static_cast<unsigned>(family),static_cast<unsigned>(rx),
        static_cast<unsigned>(route),static_cast<unsigned long long>(owner),
        static_cast<unsigned>(slot),static_cast<unsigned>(detail));
    reshade::log::message(d.failure ? reshade::log::level::warning :
                                  reshade::log::level::info,line);
#else
    (void)what;(void)rx;(void)family;(void)route;(void)owner;(void)slot;(void)detail;
#endif
}

// Probe identity stage census, separate first-hits per rejection substage.
// No D3D objects are inspected or changed in this log-only facility.
inline constexpr std::array<const char *,12> k_probe_names{{
    "none", "invalid_slot_or_input", "device_missing",
    "packedgi_unavailable", "sampler_unavailable",
    "stock_t12_unmapped", "stock_t14_unmapped",
    "probe_ordinal_out_of_range", "ptde_cube_a_not_resident",
    "ptde_cube_b_not_resident", "native_object_null",
    "ptde_cube_materialization_failed"
}};
inline std::array<std::atomic<std::uint64_t>,12> g_probe_reason_count{};
inline std::array<std::atomic<std::uint64_t>,12> g_probe_reason_reported{};
inline std::array<std::atomic<std::uint32_t>,12> g_probe_reason_first{};

inline void probe_reason(std::uint32_t reason, std::uint32_t initial,
                         std::uint32_t slot, bool blended,
                         std::uint32_t probe_a, std::uint32_t probe_b) noexcept
{
#if defined(DSRRL_V230_FAILOPEN_DIAGNOSTICS)
    if (reason == 0u || reason >= k_probe_names.size()) return;
    g_probe_reason_count[reason].fetch_add(1u,std::memory_order_relaxed);
    const auto scope_bit = (slot<4u ? slot*2u : 8u)+(blended?1u:0u);
    if ((g_probe_reason_first[reason].fetch_or(1u<<scope_bit,
            std::memory_order_relaxed)&(1u<<scope_bit))!=0u) return;
    char line[368]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL PROBE ROOT] failure=%s stage=%u initial_stage=%u slot=%u blend=%u probe_a=%u probe_b=%u",
        k_probe_names[reason],reason,initial,slot,blended?1u:0u,probe_a,probe_b);
    reshade::log::message(reshade::log::level::warning,line);
#else
    (void)reason;(void)initial;(void)slot;(void)blended;(void)probe_a;(void)probe_b;
#endif
}

inline void report(const char *phase) noexcept
{
#if defined(DSRRL_V230_FAILOPEN_DIAGNOSTICS)
    if (!g_banner.exchange(true,std::memory_order_relaxed))
        reshade::log::message(reshade::log::level::info,
            "[DSRRL FAILOPEN V230] enabled=1 scope=instrumented_gates"
            " EnvDiffuse=STOCK_DSR EnvSpec=PTDE lightbank_row_a14_m14_6=170"
            " counter=attempts not distinct draws; POLICY is not a failure");
    for(std::size_t i=0;i<k_count;++i) {
        const auto total=g_counts[i].load(std::memory_order_relaxed);
        if (total==0u) continue;
        const auto old=g_last_report[i].exchange(total,std::memory_order_relaxed);
        if (old==total && phase[0]!='U') continue;
        char msg[280]{};
        const auto &d=k_descriptions[i];
        std::snprintf(msg,sizeof(msg),
            "[DSRRL FAILOPEN V230 SUMMARY] phase=%s event=%s layer=%s kind=%s total=%llu delta=%llu",
            phase,d.name,d.layer,d.failure?"REJECT":"OBSERVED",
            static_cast<unsigned long long>(total),
            static_cast<unsigned long long>(total >= old ? total-old : 0u));
        reshade::log::message(reshade::log::level::info,msg);
    }
#if defined(DSRRL_V230_FAILOPEN_DIAGNOSTICS)
    for (std::size_t k=1u;k<k_probe_names.size();++k) {
        const auto total=g_probe_reason_count[k].load(std::memory_order_relaxed);
        if (!total) continue;
        const auto previous=g_probe_reason_reported[k].exchange(total,std::memory_order_relaxed);
        if (previous==total && phase[0]!='U') continue;
        char line[300]{};
        std::snprintf(line,sizeof(line),
            "[DSRRL PROBE ROOT SUMMARY] phase=%s failure=%s count=%llu delta=%llu",
            phase,k_probe_names[k],static_cast<unsigned long long>(total),
            static_cast<unsigned long long>(total>=previous?total-previous:0u));
        reshade::log::message(reshade::log::level::info,line);
    }
#endif
    const auto created=g_counts[static_cast<std::size_t>(stage::create_stable_ok)].load()+
                       g_counts[static_cast<std::size_t>(stage::create_lerp_ok)].load();
    const auto draws=g_counts[static_cast<std::size_t>(stage::pmetal_prepared_ok)].load();
    if (created==0u || draws==0u) {
        char msg[280]{};
        std::snprintf(msg,sizeof(msg),
            "[DSRRL FAILOPEN V230 FRONTIER] phase=%s shader_created=%llu pmetal_draw_ready=%llu note=%s",
            phase,static_cast<unsigned long long>(created),
            static_cast<unsigned long long>(draws),
            created==0u?"NO_SHADER_CREATE_HIT_OR_REGISTERED":"NO_ENVSPEC_DRAW_READY");
        reshade::log::message(reshade::log::level::warning,msg);
    }
#else
    (void)phase;
#endif
}

inline void frame() noexcept
{
#if defined(DSRRL_V230_FAILOPEN_DIAGNOSTICS)
    const auto n=g_frames.fetch_add(1u,std::memory_order_relaxed)+1u;
    if (n==1u || (n%1800u)==0u) report("LIVE");
#endif
}

} // namespace dsrrl::runtime::v230_failopen_diag
