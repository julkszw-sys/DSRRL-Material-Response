#!/usr/bin/env python3
"""V222 bounded *read-only* EnvSpec probe frontier at owner-observed RX34 draws.

V221 contains a retry which only runs if the command-list SRV shadow is ready,
but a snapshot-missing draw already uses PSGetShaderResources and may reject
silently. The owner V221 log saw RX34 route160 slot3 source=1 then probe_reject
without any V221 retry log. Do not infer the failed component.

This overlay adds independent diagnostic observations AFTER the V221 retry and
BEFORE fail-open. No shader or pixel transaction is activated by V222. The
existing native probe identity lookup is reused for diagnostic evidence only;
the registry remains exact SHA-authenticated and unchanged. P_Metal excluded.
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"

ANCHOR = """    if (!env_resource_ready) {
        if (shader != nullptr)
            shader->Release();
        telemetry::hot_count(probe_rejects_);"""

REPLACE = """#if defined(DSRRL_V222_EXACT_PROBE_REJECT_FRONTIER)
    // Owner-observed exact nonmetal probe reject anchors, RX34 only.
    // Neither anchor authorizes a rendering bridge. V222 is read-only.
    if (!env_resource_ready &&
        material.valid && material.owner_tuple_exact &&
        decision.receiver_id == 34u &&
        ((material.route_index == 160u &&
          material.material_slot == 3u) ||
         (material.route_index == 88u &&
          material.material_slot == 2u))) {
        static std::atomic<std::uint32_t> route160_seen{0u};
        static std::atomic<std::uint32_t> route88_seen{0u};
        auto &count = material.route_index == 160u
            ? route160_seen : route88_seen;
        if (count.fetch_add(1u, std::memory_order_relaxed) < 4u) {
            const bool exact_draw =
                sidecar_20261010::exact_nonmetal_draw(material, decision);
            ID3D11ShaderResourceView *live_env[3]{};
            context->PSGetShaderResources(
                12u, probe_b_required ? 3u : 1u, live_env);
            const bool native_a_present = live_env[0] != nullptr;
            const bool native_b_present = !probe_b_required ||
                live_env[2] != nullptr;
            const bool a_same = shadow_env_ready &&
                live_env[0] == shadow_env[0];
            const bool b_same = shadow_env_ready &&
                (!probe_b_required ||
                 live_env[2] == shadow_env[2]);
            const auto native_a_key =
                reinterpret_cast<std::uintptr_t>(live_env[0]);
            const auto native_b_key =
                reinterpret_cast<std::uintptr_t>(
                    probe_b_required ? live_env[2] : live_env[0]);
            for (auto *view : live_env)
                if (view != nullptr)
                    view->Release();

            // Reuses the existing EXACT DSR-native-probe lookup as a
            // temporary diagnostic, never as an EnvSpec replacement.
            // A native registry hit proves resource identity separately
            // from PackedGI cube readiness. Do not apply this on unknown
            // materials and do not change the render decision.
            prepared_envspec_resources native_probe{};
            const bool native_probe_known = exact_draw &&
                env_resources_.prepare_native_dsr(
                    context, env_semantics.envspc_slot,
                    probe_b_required, native_probe);
            const auto probe_a_diag = native_probe.probe_a;
            const auto probe_b_diag = native_probe.probe_b;
            if (native_probe_known)
                env_resources_.release(native_probe);
            const auto resource_stats = env_resources_.telemetry();

            char line[900]{};
            std::snprintf(
                line, sizeof(line),
                "[DSRRL V222 PROBE REJECT FRONTIER] "
                "rx=%u route=%u slot=%u env_slot=%u "
                "exact_draw=%u shadow_ready=%u "
                "shadow_a=%u shadow_b=%u native_a=%u native_b=%u "
                "same_a=%u same_b=%u native_a_key=%llx native_b_key=%llx "
                "probe_b_required=%u native_probe_known=%u "
                "native_probe_a=%u native_probe_b=%u "
                "registered_native=%llu registered_views=%llu "
                "pack_ready=%u sampler_ready=%u "
                "original_probe_reject=1 swap=0 pixel=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned>(material.material_slot),
                static_cast<unsigned>(env_semantics.envspc_slot),
                exact_draw ? 1u : 0u,
                shadow_env_ready ? 1u : 0u,
                shadow_env[0] != nullptr ? 1u : 0u,
                (!probe_b_required || shadow_env[2] != nullptr) ? 1u : 0u,
                native_a_present ? 1u : 0u,
                native_b_present ? 1u : 0u,
                a_same ? 1u : 0u, b_same ? 1u : 0u,
                static_cast<unsigned long long>(native_a_key),
                static_cast<unsigned long long>(native_b_key),
                probe_b_required ? 1u : 0u,
                native_probe_known ? 1u : 0u,
                static_cast<unsigned>(probe_a_diag),
                static_cast<unsigned>(probe_b_diag),
                static_cast<unsigned long long>(
                    resource_stats.native_matches),
                static_cast<unsigned long long>(
                    resource_stats.view_matches),
                resource_stats.pack_ready ? 1u : 0u,
                resource_stats.sampler_ready ? 1u : 0u);
            reshade::log::message(reshade::log::level::info, line);
        }
    }
#endif

    if (!env_resource_ready) {
        if (shader != nullptr)
            shader->Release();
        telemetry::hot_count(probe_rejects_);"""

def patch(source: str) -> str:
    if source.count(REPLACE) == 1:
        return source
    if source.count(ANCHOR) != 1:
        raise RuntimeError("V222 EnvSpec apply source drift / unexpected fail-open structure")
    if source.count('"[DSRRL V221 PROBE JOIN] "') != 1:
        raise RuntimeError("V222 requires materialized V221 probe fallback")
    result = source.replace(ANCHOR, REPLACE, 1)
    assert result.count('"[DSRRL V222 PROBE REJECT FRONTIER] "') == 1
    assert result.count("if (!env_resource_ready) {") == 1
    assert result.count("env_resources_.prepare_native_dsr(") == 2
    assert result.count("sidecar_20261010::exact_nonmetal_draw(") >= 2
    return result

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()
    original = SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
    updated = patch(original)
    if args.apply and updated != original:
        SRC.write_text(updated, encoding="utf-8", newline="\n")
    elif not args.apply and updated != original:
        raise RuntimeError("V222 read-only diagnosis not applied")
    print("V222_PROBE_REJECT_FRONTIER_" + ("APPLIED" if args.apply else "VERIFIED"))
    print("V222_READ_ONLY_PMETAL_UNCHANGED_FAIL_OPEN_UNCHANGED")
    print("V222_PIXEL_BEHAVIOR_OPEN")

if __name__ == "__main__":
    main()
