#!/usr/bin/env python3
"""V221 exact nonmetal probe: retry live PS t12/t14 when shadow lookup rejects.

This is an operator-local RESOURCE transport repair, not a loose probe
mapping. It never borrows P_Metal's probe or changes the SHA-authenticated
native-view -> PTDE PackedGI registration. If the *real current binding*
is also unrecognized, the original fail-open remains.
"""
from pathlib import Path
import argparse

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"

HEADER_ANCHOR = '#include "dsrrl/runtime/pixel_srv_shadow.hpp"'
HEADER_ADD = '''#include "dsrrl/runtime/pixel_srv_shadow.hpp"
#if defined(DSRRL_V221_NONMETAL_PROBE_CONTEXT_RETRY)
#include "dsrrl/runtime/v203_sidecar_operator_gate.hpp"
#endif'''

OLD = '''    if (!env_resource_ready) {
        if (shader != nullptr)
            shader->Release();
        telemetry::hot_count(probe_rejects_);'''

NEW = '''#if defined(DSRRL_V221_NONMETAL_PROBE_CONTEXT_RETRY)
    // Exact nonmetal may have a registered shader-resource view in native
    // context that is different from the per-command-list descriptor shadow.
    // Retry ONLY after a ready-but-rejected shadow and ONLY with an authenticated
    // full FLVER/slot/raw-MTD nonmetal receiver. P_Metal never enters this path.
    // prepare_bound() still demands the exact SHA-authenticated DSR BC6H probe
    // and the correct PTDE PackedGI slot; unknowns remain stock DSR.
    if (!env_resource_ready && !k_native_dsr_cubemap_feed &&
        shadow_env_ready &&
        sidecar_20261010::exact_nonmetal_material(material)) {
        ID3D11ShaderResourceView *native_views[3]{};
        context->PSGetShaderResources(
            12u, probe_b_required ? 3u : 1u, native_views);

        const bool a_present = native_views[0] != nullptr;
        const bool b_present = !probe_b_required ||
            native_views[2] != nullptr;
        const bool a_changed = native_views[0] != shadow_env[0];
        const bool b_changed = probe_b_required &&
            native_views[2] != shadow_env[2];

        const bool retry_ready =
            env_resources_.prepare_bound(
                native_views[0],
                probe_b_required ? native_views[2] : nullptr,
                env_semantics.envspc_slot,
                probe_b_required,
                prepared.env_resources);
        for (auto *view : native_views)
            if (view != nullptr)
                view->Release();

        env_resource_ready = retry_ready;

        // These first-hit witnesses concern *verified* nonmetal only.
        // An RX33/34 material hit is not bridge activation by itself.
        static std::atomic<std::uint32_t> retry_ok{0u};
        static std::atomic<std::uint32_t> retry_no{0u};
        auto &seen = retry_ready ? retry_ok : retry_no;
        if (seen.fetch_add(1u, std::memory_order_relaxed) < 8u) {
            char line[448]{};
            std::snprintf(
                line, sizeof(line),
                "[DSRRL V221 PROBE JOIN] "
                "rx=%u route=%u slot=%u env_slot=%u "
                "shadow_ready=1 native_a=%u native_b=%u "
                "a_changed=%u b_changed=%u "
                "exact_native_probe=%u probe_a=%u probe_b=%u "
                "ptde_resource=%u pixel=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned>(material.material_slot),
                static_cast<unsigned>(env_semantics.envspc_slot),
                a_present ? 1u : 0u,
                b_present ? 1u : 0u,
                a_changed ? 1u : 0u,
                b_changed ? 1u : 0u,
                retry_ready ? 1u : 0u,
                static_cast<unsigned>(prepared.env_resources.probe_a),
                static_cast<unsigned>(prepared.env_resources.probe_b),
                retry_ready ? 1u : 0u);
            reshade::log::message(
                reshade::log::level::info,
                line);
        }
    }
#endif

    if (!env_resource_ready) {
        if (shader != nullptr)
            shader->Release();
        telemetry::hot_count(probe_rejects_);'''

def replace_once(s: str, before: str, after: str) -> str:
    if s.count(after) == 1:
        return s
    if s.count(before) != 1:
        raise RuntimeError("V221 source drift: " + before[:85])
    return s.replace(before, after, 1)

def patch(s: str) -> str:
    s = replace_once(s, HEADER_ANCHOR, HEADER_ADD)
    s = replace_once(s, OLD, NEW)
    assert s.count('if (!env_resource_ready) {') == 1
    assert s.count('sidecar_20261010::exact_nonmetal_material(material)') == 1
    assert s.count('k_effect_fail_probe') >= 2
    assert s.count('"[DSRRL V221 PROBE JOIN] "') == 1
    return s

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    opts = parser.parse_args()
    original = SRC.read_text(encoding="utf-8").replace("\r\n", "\n")
    patched = patch(original)
    if opts.apply:
        if patched != original:
            SRC.write_text(patched, encoding="utf-8", newline="\n")
    elif patched != original:
        raise RuntimeError("V221 source not materialized")
    print("V221_NONMETAL_LIVE_CONTEXT_RETRY_" +
          ("APPLIED" if opts.apply else "VERIFIED"))
    print("V221_EXACT_PROBE_SHA_GATE_AND_PMETAL_PATH_PRESERVED")
    print("V221_PTDE_PIXEL=OPEN")

if __name__ == "__main__":
    main()
