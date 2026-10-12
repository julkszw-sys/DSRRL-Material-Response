#!/usr/bin/env python3
"""V223 opt-in shared-LightBank fallback for EXACT nonmetal EnvSpec materials.

Generated-source overlay only, applied AFTER V203/V204/V220. The owner
reports that equipment materials share the LightBank and that original P_Metal
works with the retail native single/blend packer fallback. Probe that claim
without changing the original P_Metal path, shader, resource registry, or
material authority. No global fallback for unrelated materials is permitted.

This is an EXPERIMENT, not PTDE pixel equivalence. A source hit is only the
first consumer gate: probe, companion t1, replacement and draw still fail open.
"""
from pathlib import Path
import argparse

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "renderer-core/src/runtime/pmetal_env_source_runtime.cpp"
ANCHOR = """#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)
    // Unkeyed R43 latest_hook_source may only serve original exact P_Metal.
"""
INSERT = """#if defined(DSRRL_V223_EXACT_NONMETAL_SHARED_LIGHTBANK_FALLBACK)
    // Compare the existing P_Metal native retail single/blend packer
    // signal on only the fully authenticated nonmetal material subset.
    // V220's keyed producer always takes precedence. This experiment
    // changes the source *only* after the keyed attempt failed.
    if (sidecar_20261010::exact_nonmetal_material(material)) {
        pmetal_envspec_source shared_source{};
        const bool shared_available =
            latest_hook_source(shared_source);
        // Use the same source-payload contract as the already-live
        // original P_Metal path. The draw subsequently validates beta
        // and all independent probe / t1 / shader resource gates.
        static std::atomic<std::uint32_t> shared_hit{0u};
        static std::atomic<std::uint32_t> shared_miss{0u};
        auto &seen = shared_available ? shared_hit : shared_miss;
        if (seen.fetch_add(1u, std::memory_order_relaxed) < 32u) {
            char line[560]{};
            std::snprintf(
                line, sizeof(line),
                "[DSRRL V223 SHARED LIGHTBANK] "
                "route=%u slot=%u owner_exact=1 keyed_source=0 "
                "shared_source=%u hook_tls=%u "
                "serial=%llu generation=%llu beta=%.9g "
                "bank_a=%016llx bank_b=%016llx row_a=%u row_b=%u "
                "envdiffuse_valid=%u "
                "next=standard_source_probe_spec_gates pixel=OPEN",
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned>(material.material_slot),
                shared_available ? 1u : 0u,
                g_hook_source_tls.valid ? 1u : 0u,
                static_cast<unsigned long long>(
                    shared_source.serial),
                static_cast<unsigned long long>(
                    shared_source.generation),
                static_cast<double>(shared_source.beta),
                static_cast<unsigned long long>(
                    shared_source.bank_signature_a),
                static_cast<unsigned long long>(
                    shared_source.bank_signature_b),
                static_cast<unsigned>(shared_source.row_id_a),
                static_cast<unsigned>(shared_source.row_id_b),
                shared_source.envdiffuse_linear_valid ? 1u : 0u);
            reshade::log::message(reshade::log::level::info, line);
        }
        if (shared_available) {
            out = shared_source;
            telemetry::hot_count(g_consumer_ok);
            return true;
        }
    }
#endif

"""

def patch(source: str) -> str:
    if source.count(INSERT + ANCHOR) == 1:
        return source
    if source.count(ANCHOR) != 1:
        raise RuntimeError("V223 exact V203 fallback guard missing / drifted")
    if source.count("const bool v220_nonmetal_join =") != 1:
        raise RuntimeError("V223 requires V220 keyed nonmetal producer first")
    if source.count("if (latest_hook_source(out)) {") != 1:
        raise RuntimeError("V223 original P_Metal packer fallback changed")
    if source.count("sidecar_20261010::exact_nonmetal_material(material)") < 1:
        raise RuntimeError("V223 exact full-owner material authority missing")
    updated = source.replace(ANCHOR, INSERT + ANCHOR, 1)
    if updated.count("if (latest_hook_source(out)) {") != 1:
        raise RuntimeError("V223 must preserve original P_Metal fallback")
    if updated.index("const bool v220_nonmetal_join =") > updated.index(
            "[DSRRL V223 SHARED LIGHTBANK]"):
        raise RuntimeError("V223 fallback must follow keyed source")
    if updated.index("[DSRRL V223 SHARED LIGHTBANK]") > updated.index(
            "Unkeyed R43 latest_hook_source may only serve original exact P_Metal"):
        raise RuntimeError("V223 must stay before original P_Metal-only guard")
    return updated

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    original = SOURCE.read_text(encoding="utf-8").replace("\r\n", "\n")
    updated = patch(original)
    if args.apply and updated != original:
        SOURCE.write_text(updated, encoding="utf-8", newline="\n")
    elif not args.apply and updated != original:
        raise RuntimeError("V223 opt-in generated source not applied")
    print("V223_EXACT_NONMETAL_SHARED_LIGHTBANK_" +
          ("APPLIED" if args.apply else "VERIFIED"))
    print("V223_PMETAL_SOURCE_UNCHANGED=TRUE")
    print("V223_NONMETAL_FALLBACK_EXPERIMENTAL_PIXEL_OPEN")

if __name__ == "__main__":
    main()
