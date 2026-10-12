#!/usr/bin/env python3
"""V226: material-keyed source only for exact nonmetal HemEnvLerp.

Applied AFTER V220 keyed producer, V223 shared-source fallback, V224 probe.
P_Metal (including Lerp) and nonmetal stable paths stay byte-identical.
A nonmetal Lerp may never consume the unkeyed latest_hook_source; only the
same FLVER+slot+MTD/epoch producer record can authorize its A/B/beta input.
Missing/invalid endpoints fail open to stock DSR. No probe guesses.
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "renderer-core/include/dsrrl/runtime/pmetal_env_source_runtime.hpp"
SOURCE = ROOT / "renderer-core/src/runtime/pmetal_env_source_runtime.cpp"
DRAW = ROOT / "renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"

HEADER_ANCHOR = "    bool latest(const operators::material_response::material_identity &material, pmetal_envspec_source &out) const noexcept;"
HEADER_NEW = HEADER_ANCHOR + """
#if defined(DSRRL_V226_NONMETAL_LERP_KEYED_SOURCE)
    bool latest_keyed_nonmetal_lerp(
        const operators::material_response::material_identity &material,
        pmetal_envspec_source &out) const noexcept;
#endif
"""

SOURCE_ANCHOR = "pmetal_env_source_runtime_telemetry\npmetal_env_source_runtime::telemetry() const noexcept"
SOURCE_NEW = """#if defined(DSRRL_V226_NONMETAL_LERP_KEYED_SOURCE)
bool pmetal_env_source_runtime::latest_keyed_nonmetal_lerp(
    const operators::material_response::material_identity &material,
    pmetal_envspec_source &out) const noexcept
{
    out = {};
    // This is not a P_Metal path and cannot accept an unkeyed hook record.
    if (!sidecar_20261010::exact_nonmetal_material(material) ||
        !g_selector_enabled.load(std::memory_order_acquire))
        return false;

    const auto epoch = g_selector_epoch.load(std::memory_order_acquire);
    const bool joined = pmetal_producer_state_latest(
        v220_exact_nonmetal_source_key(material), epoch, out);
    if (!joined ||
        g_selector_epoch.load(std::memory_order_acquire) != epoch) {
        out = {};
        return false;
    }

    // Lerp requires a finite, normalized blend scalar and two authenticated
    // bank endpoints. A stable-only donor cannot authorize the Lerp consumer.
    const auto finite3 = [](const std::array<float,3> &v) noexcept {
        return std::isfinite(v[0]) && std::isfinite(v[1]) &&
               std::isfinite(v[2]);
    };
    if (!std::isfinite(out.beta) ||
        out.beta < 0.0f || out.beta > 1.0f ||
        out.bank_signature_a == 0u ||
        out.bank_signature_b == 0u ||
        !finite3(out.a) || !finite3(out.b) ||
        !out.envdiffuse_linear_valid ||
        !finite3(out.envdiffuse_a) ||
        !finite3(out.envdiffuse_b)) {
        out = {};
        return false;
    }

    return true;
}
#endif

""" + SOURCE_ANCHOR

DRAW_ANCHOR = """    pmetal_envspec_source source{};
    if (!source_.latest(material, source) ||
        !std::isfinite(source.beta)) {"""
DRAW_NEW = """    pmetal_envspec_source source{};
#if defined(DSRRL_V226_NONMETAL_LERP_KEYED_SOURCE)
    // Never borrow a previously packed LightBank from another material when
    // switching profiles. Exact keyed producer or stock DSR, not stale beta.
    const bool v226_keyed_lerp =
        family == pmetal_envspec_receiver_family::hemenvlerp &&
        sidecar_20261010::exact_nonmetal_material(material);
    const bool v226_source_ok = v226_keyed_lerp
        ? source_.latest_keyed_nonmetal_lerp(material, source)
        : source_.latest(material, source);
    if (v226_keyed_lerp) {
        static std::atomic<std::uint32_t> pass_logged{0u};
        static std::atomic<std::uint32_t> fail_logged{0u};
        auto &budget = v226_source_ok ? pass_logged : fail_logged;
        if (budget.fetch_add(1u, std::memory_order_relaxed) < 12u) {
            char line[400]{};
            std::snprintf(line, sizeof(line),
                "[DSRRL V226 NONMETAL LERP SOURCE] "
                "rx=%u route=%u slot=%u keyed=%u "
                "beta=%.9g bank_a=%016llx bank_b=%016llx "
                "row_a=%u row_b=%u "
                "outcome=%s pixel=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned>(material.material_slot),
                v226_source_ok ? 1u : 0u,
                static_cast<double>(source.beta),
                static_cast<unsigned long long>(source.bank_signature_a),
                static_cast<unsigned long long>(source.bank_signature_b),
                static_cast<unsigned>(source.row_id_a),
                static_cast<unsigned>(source.row_id_b),
                v226_source_ok ? "next_probe_spec_shader_gates"
                               : "stock_dsr_fail_open");
            reshade::log::message(reshade::log::level::info, line);
        }
    }
    if (!v226_source_ok || !std::isfinite(source.beta)) {
#else
    if (!source_.latest(material, source) ||
        !std::isfinite(source.beta)) {
#endif"""

def replace_once(src: str, old: str, new: str, name: str) -> str:
    if src.count(new) == 1:
        return src
    if src.count(old) != 1:
        raise ValueError(f"V226 drift in {name}: expected unique anchor")
    return src.replace(old, new, 1)

def patch(header: str, source: str, draw: str):
    if "v220_exact_nonmetal_source_key" not in source or (
        "[DSRRL V223 SHARED LIGHTBANK]" not in source or
        "prepare_exact_native_resource_join_v224" not in draw):
        raise ValueError("V226 must be generated after V220/V223/V224")
    header = replace_once(header, HEADER_ANCHOR, HEADER_NEW, "header")
    source = replace_once(source, SOURCE_ANCHOR, SOURCE_NEW, "source")
    draw = replace_once(draw, DRAW_ANCHOR, DRAW_NEW, "draw")
    if "if (latest_hook_source(out)) {" not in source:
        raise ValueError("P_Metal fallback absent")
    if source.count("pmetal_producer_state_latest(") < 2:
        raise ValueError("V220 exact keyed producer missing")
    return header, source, draw

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    arg=ap.parse_args()
    files=(HEADER,SOURCE,DRAW)
    original=tuple(p.read_text(encoding="utf-8").replace("\r\n","\n")
                   for p in files)
    updated=patch(*original)
    if arg.apply:
        for p,a,b in zip(files,original,updated):
            if a!=b: p.write_text(b,encoding="utf-8",newline="\n")
    elif original!=updated:
        raise RuntimeError("V226 generated source not materialized")
    print("V226_LERP_KEYED_SOURCE_"+("APPLIED" if arg.apply else "VERIFIED"))
    print("V226_PMETAL_AND_NONMETAL_STABLE_PRESERVED=1")
    print("V226_PIXEL=OPEN")

if __name__=="__main__":
    main()
