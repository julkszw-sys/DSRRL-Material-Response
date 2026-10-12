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

FLVER_OLD = """  telemetry::hot_count(g_selector_events);
  material_owner_selection_clear();
  pmetal_env_source_selector_clear();

#ifndef DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE"""
FLVER_NEW = """  telemetry::hot_count(g_selector_events);
  material_owner_selection_clear();
  // V249: An unrelated FLVER selector must not wipe the P_Metal producer
  // TLS. An exact P_Metal selector still clears and re-begins its own
  // material-local transaction in pmetal_env_source_selector_event().
  // Uninstall/reset still call pmetal_env_source_selector_clear().
  // This intentionally affects ONLY the P_Metal selector TLS transport.
  // No material authority is granted by preserving the record.
  
#ifndef DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE"""

SOURCE_OLD = """    if (pmetal_producer_state_latest(
            material,
            epoch,
            out)) {
        telemetry::hot_count(
            g_consumer_ok);
        return true;
    }

    // Narrow fallback carrier recovered from the attested retail LightBank
    // single/blend packers (V13 semantic cut). It is consumed only by an
    // already-authenticated exact P_Metal draw. The retail packer is a state
    // producer: retain its latest generation until the next exact source
    // update, matching the validated V13 lifetime without exposing it to U/L.
    if (latest_hook_source(out)) {
        telemetry::hot_count(
            g_consumer_ok);
        return true;
    }"""
SOURCE_NEW = """    // V249 exact P_Metal producer/hook comparison. The FLVER selector
    // publication must NEVER silently override an unmatched retail
    // LightBank source. This is a positive-control transport repair,
    // not an assumption that cache equality implies PTDE pixel equality.
    pmetal_envspec_source main_source{};
    const bool main_ready =
        pmetal_producer_state_latest(material, epoch, main_source);
    pmetal_envspec_source hook_source{};
    const bool hook_ready =
        latest_hook_source(hook_source);

    const bool source_equal =
        main_ready && hook_ready &&
        same_hook_source_payload(main_source, hook_source) &&
        main_source.phn_k135 == hook_source.phn_k135 &&
        main_source.phn_k135_valid == hook_source.phn_k135_valid;

    // Bounded transition observations. Only route 345 P_Metal reaches
    // this branch. No QPC, disk IO, GPU readback, D3D mutation or per-draw
    // string construction after the first eight samples per outcome.
    const unsigned stage =
        main_ready
            ? hook_ready
                ? source_equal ? 0u : 1u
                : 2u
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
        reshade::log::message(
            reshade::log::level::info, detail);
    }

    if (source_equal || (main_ready && !hook_ready)) {
        out = main_source;
        telemetry::hot_count(g_consumer_ok);
        return true;
    }

    // The original verified P_Metal hook wins on a semantic mismatch.
    // A mismatch is evidence, not permission to substitute guessed
    // main-channel data for an operator that already rendered properly.
    if (hook_ready) {
        out = hook_source;
        telemetry::hot_count(g_consumer_ok);
        return true;
    }"""

def replace_exact(src: str, old: str, new: str, name: str) -> str:
    if src.count(new) == 1 and src.count(old) == 0:
        return src
    if src.count(old) != 1 or src.count(new) != 0:
        raise RuntimeError(f"V249 {name}: expected exactly one unpatched anchor")
    return src.replace(old, new, 1)

def patch_flver(src: str) -> str:
    patched = replace_exact(src, FLVER_OLD, FLVER_NEW, "FLVER selector-entry")
    if patched.count("pmetal_env_source_selector_clear();") != 1:
        raise RuntimeError("V249 requires teardown clear to remain")
    if patched.count("pmetal_env_source_selector_event(") != 1:
        raise RuntimeError("V249 requires authentic exact P_Metal event")
    return patched

def patch_source(src: str) -> str:
    patched = replace_exact(src, SOURCE_OLD, SOURCE_NEW, "PMetal latest source")
    if patched.count("    pmetal_producer_state_clear();") < 1:
        raise RuntimeError("V249 exact P_Metal event must still invalidate TLS")
    if patched.count("    pmetal_producer_state_begin(") != 1:
        raise RuntimeError("V249 material-scoped begin missing")
    if patched.count("    pmetal_producer_state_publish(") != 2:
        raise RuntimeError("V249 exact selector publish paths missing")
    if patched.count("same_hook_source_payload(main_source, hook_source)") != 1:
        raise RuntimeError("V249 semantic equality guard lost")
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
