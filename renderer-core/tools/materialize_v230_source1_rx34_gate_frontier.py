#!/usr/bin/env python3
"""Read-only-behavior diagnostic for exact RX34 PTDE Source1 draw.

Source baseline must be DSRRL 232ac131 (v230 pre-release).
Logs prepare FRONTIER for the sole owner FLVER3a82e4f6 route345 slot2 RX34.
Never changes a condition, source signal, shader, texture, or D3D state.
Mutation performed on source C++ for pinned one-addon Windows CI build.
"""
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
s=p.read_text(encoding="utf-8")
def swap(a,b):
    global s
    n=s.count(a)
    if n!=1:raise SystemExit(f"source drift {n} anchors: {a[:150]!r}")
    s=s.replace(a,b,1)
swap(
"""void log_prepare_stage_once(
    std::uint32_t bit,""",
"""// Exact owner-only diagnostic. Bounded to avoid interfering with draw throughput.
// A Source1 HIT proves source publication/lookup but does NOT prove prepare()
// reaches material request, replacement PS, probe or b12 upload.
inline void source1_rx34_frontier(
    const char *stage,
    const mr::material_identity &material,
    const mr::decision &decision) noexcept
{
#if defined(DSRRL_SOURCE1_RX34_GATE_FRONTIER)
    if (decision.receiver_id != 34u ||
        material.route_index != 345u ||
        !material.material_slot_valid ||
        material.material_slot != 2u ||
        !material.owner_tuple_exact ||
        !hashing::matches_hex(
            material.flver_sha256,
            "3a82e4f6e45f13a05ac40418ba6ad12ff9acae9cf01e19937bd5c5f45ff2636c"))
        return;
    static std::atomic<std::uint32_t> emitted{0u};
    const auto seq=emitted.fetch_add(1u,std::memory_order_relaxed);
    if (seq >= 100u) return;
    char line[384]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL SOURCE1 RX34 FRONTIER] n=%u stage=%s rx=%u route=%u slot=%u exact=%u active=%u",
        seq+1u,stage,
        static_cast<unsigned>(decision.receiver_id),
        static_cast<unsigned>(material.route_index),
        static_cast<unsigned>(material.material_slot),
        material.owner_tuple_exact ? 1u : 0u,
        decision.active ? 1u : 0u);
    reshade::log::message(reshade::log::level::warning,line);
#else
    (void)stage; (void)material; (void)decision;
#endif
}

void log_prepare_stage_once(
    std::uint32_t bit,""")
swap(
"""    pmetal_envspec_receiver_family family) noexcept
{
#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)""",
"""    pmetal_envspec_receiver_family family) noexcept
{
    source1_rx34_frontier(stage,material,decision);
#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)""")
swap(
"""    prepared = {};
    effect_latch(effect_entry_seen_);

    if (cmd_list == nullptr ||""",
"""    prepared = {};
    effect_latch(effect_entry_seen_);
    source1_rx34_frontier("prepare_entry",material,decision);

    if (cmd_list == nullptr ||""")
swap(
"""    if (!core_.features().enabled(
             core::operator_id::env_spec)) {
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_feature);
        return false;
    }""",
"""    if (!core_.features().enabled(
             core::operator_id::env_spec)) {
        effect_fail(
            effect_fail_mask_,
            k_effect_fail_feature);
        source1_rx34_frontier("env_spec_feature_disabled",material,decision);
        return false;
    }""")
swap(
"""    effect_latch(effect_source_ready_);

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)""",
"""    effect_latch(effect_source_ready_);
    source1_rx34_frontier("source_latest_ready",material,decision);

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)""")
swap(
"""        return false;
    }
    effect_latch(effect_probe_ready_);

    if (!g_resource_mode_logged.exchange(""",
"""        return false;
    }
    effect_latch(effect_probe_ready_);
    source1_rx34_frontier("probe_ready",material,decision);

    if (!g_resource_mode_logged.exchange(""")
swap(
"""    if (!material_ready ||
        !prepared.material_resources.spec_rgb) {
        if (shader != nullptr)""",
"""    source1_rx34_frontier(
        !material_ready ? "material_resources_not_ready" :
        !prepared.material_resources.spec_rgb ? "spec_rgb_not_prepared" :
        "spec_rgb_prepared",
        material, decision);
    if (!material_ready ||
        !prepared.material_resources.spec_rgb) {
        if (shader != nullptr)""")
swap(
"""    effect_latch(effect_spec_rgb_ready_);
#endif

    const bool stable_envdiffuse_consumer_diag =""",
"""    effect_latch(effect_spec_rgb_ready_);
    source1_rx34_frontier("spec_rgb_stage_ready",material,decision);
#endif

    const bool stable_envdiffuse_consumer_diag =""")
swap(
"""        effect_fail(
            effect_fail_mask_,
            k_effect_fail_device);
        return false;
    }

#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)""",
"""        effect_fail(
            effect_fail_mask_,
            k_effect_fail_device);
        source1_rx34_frontier("device_null",material,decision);
        return false;
    }

#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)""")
swap(
"""            quarantined_.store(true);
            effect_fail(
                effect_fail_mask_,
                k_effect_fail_device);
            return false;
        }

        const auto found =""",
"""            quarantined_.store(true);
            effect_fail(
                effect_fail_mask_,
                k_effect_fail_device);
            source1_rx34_frontier("device_identity_mismatch",material,decision);
            return false;
        }

        const auto found =""")
swap(
"""                effect_fail(
                    effect_fail_mask_,
                    k_effect_fail_b12);
                return false;
            }

            b12_context_cache entry{};""",
"""                effect_fail(
                    effect_fail_mask_,
                    k_effect_fail_b12);
                source1_rx34_frontier("b12_create_fail",material,decision);
                return false;
            }

            b12_context_cache entry{};""")
swap(
"""            effect_fail(
                effect_fail_mask_,
                k_effect_fail_b12);
            return false;
        }

        std::memcpy(
            mapped.pData,""",
"""            effect_fail(
                effect_fail_mask_,
                k_effect_fail_b12);
            source1_rx34_frontier("b12_map_fail",material,decision);
            return false;
        }

        std::memcpy(
            mapped.pData,""")
p.write_text(s,encoding="utf-8")
print("SOURCE1_RX34_GATE_FRONTIER_SOURCE_PATCH_PASS owner-scoped logging only")
