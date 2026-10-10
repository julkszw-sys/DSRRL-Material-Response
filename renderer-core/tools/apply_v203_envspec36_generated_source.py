#!/usr/bin/env python3
"""Deterministic v203-only diagnostic overlay AFTER pinned R43 source materializer.

Do not edit the tracked pmetal_env_source_runtime.cpp: it is a certified
materializer INPUT checked against git blob a6904844c97c712027ad79bbcd136066952a7116.
The production script regenerates the source from pinned donor; only then
can this opt-in experiment add the separate exact 36-owner material route.
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PATH = ROOT / "renderer-core/src/runtime/pmetal_env_source_runtime.cpp"
INCLUDE = '#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"'
NEW_INCLUDE = (INCLUDE +
    '\n#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)' +
    '\n#include "dsrrl/runtime/v203_equipment_envspec36_gate.hpp"' +
    '\n#endif')
FUNCTION_ANCHOR = 'bool retail_lightbank_record_index('
HELPER = """// Source-complete v203 exact equipment alternative; production P_Metal
// decision remains the original selector when this experiment is OFF.
inline bool exact_envspec36_or_pmetal_source(
    const operators::material_response::material_identity &material) noexcept
{
    if (exact_pmetal_material_selection(material))
        return true;
#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)
    return v203_equipment::exact_envspec36_material(material);
#else
    return false;
#endif
}

"""
OLD_USE = '!exact_pmetal_material_selection(\n            material)'
NEW_USE = '!exact_envspec36_or_pmetal_source(\n            material)'
FALLBACK = '    if (latest_hook_source(out)) {'
GUARD = """#if defined(DSRRL_V203_EQUIPMENT_ENVSPEC36_DIAG)
    // Unkeyed R43 latest_hook_source may only serve original exact P_Metal.
    // Nonmetal equipment must resolve synchronized per-material producer state.
    if (!exact_pmetal_material_selection(material)) {
        telemetry::hot_count(g_consumer_fail);
        return false;
    }
#endif
"""

def exactly(s: str, phrase: str, expected: int):
    seen = s.count(phrase)
    if seen != expected:
        raise AssertionError((phrase[:80], expected, seen))

def candidate(s: str) -> str:
    # The pinned source donor and release hardening must have run first.
    exactly(s, "[DSRRL PMETAL ZERO RE]", 1)
    exactly(s, INCLUDE, 1)
    exactly(s, FUNCTION_ANCHOR, 1)
    exactly(s, OLD_USE, 2)
    exactly(s, FALLBACK, 1)
    exactly(s, "v203_equipment_envspec36_gate.hpp", 0)
    s = s.replace(INCLUDE, NEW_INCLUDE, 1)
    s = s.replace(FUNCTION_ANCHOR, HELPER + FUNCTION_ANCHOR, 1)
    s = s.replace(OLD_USE, NEW_USE)
    s = s.replace(FALLBACK, GUARD + FALLBACK, 1)
    return s

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply",action="store_true")
    args = ap.parse_args()
    current = PATH.read_text(encoding="utf-8").replace("\r\n", "\n")
    if "v203_equipment_envspec36_gate.hpp" in current:
        # Idempotence: reconstruct original by exact inverse, not name guessing.
        original = current.replace(NEW_INCLUDE, INCLUDE, 1)
        original = original.replace(HELPER + FUNCTION_ANCHOR, FUNCTION_ANCHOR, 1)
        original = original.replace(NEW_USE, OLD_USE)
        original = original.replace(GUARD + FALLBACK, FALLBACK, 1)
        exactly(original, "v203_equipment_envspec36_gate.hpp", 0)
        if candidate(original) != current:
            raise AssertionError("v203 overlay drift")
        print("V203_ENVSPEC36_GENERATED_SOURCE_VERIFIED")
    elif args.apply:
        PATH.write_text(candidate(current),encoding="utf-8",newline="\n")
        print("V203_ENVSPEC36_GENERATED_SOURCE_APPLIED")
    else:
        raise AssertionError("v203 opt-in source was not materialized")

if __name__=="__main__":
    main()
