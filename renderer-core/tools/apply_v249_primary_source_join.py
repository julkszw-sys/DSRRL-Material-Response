#!/usr/bin/env python3
"""V249: preserve exact P_Metal TLS across unrelated FLVER selectors.

The original P_Metal selector still calls pmetal_producer_state_clear()
before begin/publish; renderer reset and uninstall still clear records.
At actual draw, compare authoritative synchronized source with original
LightBank packer source. Only promote main source when equal or the hook is
unavailable; otherwise preserve the owner-proven hook and record divergence.
Do not change shader, SRV, CB ABI, receiver, sampler or numeric lighting.
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FLVER = ROOT / "renderer-core/src/runtime/flver_engine_hooks.cpp"
SOURCE = ROOT / "renderer-core/src/runtime/pmetal_env_source_runtime.cpp"

FLVER_OLD = """ telemetry::hot_count(g_selector_events);
 material_owner_selection_clear();
 pmetal_env_source_selector_clear();

#ifndef DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE"""
FLVER_NEW = """ telemetry::hot_count(g_selector_events);
 material_owner_selection_clear();
 // V249: no unconditional P_Metal producer TLS clear on unrelated FLVER
 // selectors. An exact material still clears/starts a transaction inside
 // pmetal_env_source_selector_event(), while reset/uninstall still clear.
 // No material authorizations are granted by preserving this TLS record.

#ifndef DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE"""

# The V220 generated source includes a first-choice *nonmetal* join.
# It previously invoked the lookup even for P_Metal, so exact P_Metal
# bypassed any V249 primary-versus-hook comparison on that fast path.
NONMETAL_OLD = """    const bool v220_nonmetal_join =
        pmetal_producer_state_latest("""
NONMETAL_NEW = """    const bool v220_nonmetal_join =
        sidecar_20261010::exact_nonmetal_material(material) &&
        pmetal_producer_state_latest("""

# Insert AFTER V223's independent nonmetal fallback, BEFORE V203's strict
# route345 guard + original latest_hook_source(out). Do not remove either.
SOURCE_OLD = """#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)
    // Unkeyed R43 latest_hook_source may only serve original exact P_Metal."""
SOURCE_NEW = """    // V249: exact original P_Metal only. Nonmetal V220/V223 paths above
    // remain separate. Compare the synchronized material/epoch record with
    // the owner-proven retail LightBank packer before preferring it.
    if (exact_pmetal_material_selection(material)) {
        pmetal_envspec_source main_source{};
        const bool main_ready =
            pmetal_producer_state_latest(
                material, epoch, main_source);
        pmetal_envspec_source hook_source{};
        const bool hook_ready =
            latest_hook_source(hook_source);

        const bool source_equal =
            main_ready && hook_ready &&
            same_hook_source_payload(main_source, hook_source) &&
            main_source.phn_k135 == hook_source.phn_k135 &&
            main_source.phn_k135_valid == hook_source.phn_k135_valid;
        // Stage 0 = both equal; 1 = mismatch; 2 = main only;
        // 3 = hook only; 4 = neither. First 8 of each category only.
        const unsigned stage =
            main_ready
                ? hook_ready ? (source_equal ? 0u : 1u) : 2u
                : hook_ready ? 3u : 4u;
        static std::atomic<std::uint32_t> trace_by_stage[5]{};
        if (trace_by_stage[stage].fetch_add(
                1u, std::memory_order_relaxed) < 8u) {
            char detail[720]{};
            std::snprintf(
                detail, sizeof(detail),
                "[DSRRL V249 PMETAL SOURCE JOIN] route=%u slot=%u "
                "stage=%u main=%u hook=%u exact_payload=%u "
                "main_beta=%.9g hook_beta=%.9g "
                "main_bank=%016llx/%016llx hook_bank=%016llx/%016llx "
                "main_row=%u/%u hook_row=%u/%u main_generation=%llu "
                "hook_generation=%llu tid=%lu pixel=OPEN",
                static_cast<unsigned>(material.route_index),
                static_cast<unsigned>(material.material_slot),
                stage,
                main_ready ? 1u : 0u,
                hook_ready ? 1u : 0u,
                source_equal ? 1u : 0u,
                static_cast<double>(main_source.beta),
                static_cast<double>(hook_source.beta),
                static_cast<unsigned long long>(main_source.bank_signature_a),
                static_cast<unsigned long long>(main_source.bank_signature_b),
                static_cast<unsigned long long>(hook_source.bank_signature_a),
                static_cast<unsigned long long>(hook_source.bank_signature_b),
                static_cast<unsigned>(main_source.row_id_a),
                static_cast<unsigned>(main_source.row_id_b),
                static_cast<unsigned>(hook_source.row_id_a),
                static_cast<unsigned>(hook_source.row_id_b),
                static_cast<unsigned long long>(main_source.generation),
                static_cast<unsigned long long>(hook_source.generation),
                static_cast<unsigned long>(GetCurrentThreadId()));
            reshade::log::message(reshade::log::level::info, detail);
        }
        if (source_equal || (main_ready && !hook_ready)) {
            out = main_source;
            telemetry::hot_count(g_consumer_ok);
            return true;
        }
        // A mismatching main record has no authority to override the
        // proven hook signal. This is fail-open within the PTDE bridge.
        if (hook_ready) {
            out = hook_source;
            telemetry::hot_count(g_consumer_ok);
            return true;
        }
    }

#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)
    // Unkeyed R43 latest_hook_source may only serve original exact P_Metal."""

def replace_exact(src: str, old: str, new: str, name: str) -> str:
    if src.count(new) == 1:
        return src
    if src.count(old) != 1 or src.count(new) != 0:
        raise RuntimeError(f"V249 {name}: expected exactly one unpatched anchor")
    return src.replace(old, new, 1)

def patch_flver(src: str) -> str:
    patched = replace_exact(src, FLVER_OLD, FLVER_NEW, "FLVER selector-entry")
    if patched.count("pmetal_env_source_selector_clear();") != 1:
        raise RuntimeError("V249 requires teardown clear to remain")
    if patched.count("        pmetal_env_source_selector_event(") != 1:
        raise RuntimeError("V249 requires authentic exact P_Metal event")
    return patched

def patch_source(src: str) -> str:
    patched = replace_exact(src, NONMETAL_OLD, NONMETAL_NEW,
                            "V220 exact nonmetal-only first choice")
    patched = replace_exact(patched, SOURCE_OLD, SOURCE_NEW,
                            "PMetal guarded source comparison")
    if patched.count("if (latest_hook_source(out)) {") != 1:
        raise RuntimeError("V249 must keep original P_Metal hook fallback")
    if patched.count("    pmetal_producer_state_clear();") < 1:
        raise RuntimeError("V249 exact P_Metal selector must invalidate TLS")
    if patched.count("    pmetal_producer_state_begin(") < 1:
        raise RuntimeError("V249 exact P_Metal begin missing")
    if patched.count("    pmetal_producer_state_publish(") < 2:
        raise RuntimeError("V249 exact source publication paths missing")
    if patched.count("same_hook_source_payload(main_source, hook_source)") != 1:
        raise RuntimeError("V249 semantic equality guard missing")
    if patched.count("if (sidecar_20261010::exact_nonmetal_material(material))") < 1:
        raise RuntimeError("V249 must preserve nonmetal gate")
    return patched

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    a = parser.parse_args()
    for path, transform in ((FLVER, patch_flver),(SOURCE,patch_source)):
        before = path.read_text(encoding="utf-8").replace("\r\n", "\n")
        after = transform(before)
        if a.apply and after != before:
            path.write_text(after, encoding="utf-8", newline="\n")
        elif not a.apply and after != before:
            raise RuntimeError("V249 patch not materialized: " + str(path))
    print("V249_PMETAL_SELECTOR_SCOPED_TLS_"+("APPLIED" if a.apply else "VERIFIED"))
    print("V249_MAIN_SOURCE_VERIFIED_AGAINST_HOOK_FAIL_OPEN_CONTRACT")

if __name__ == "__main__":
    main()
