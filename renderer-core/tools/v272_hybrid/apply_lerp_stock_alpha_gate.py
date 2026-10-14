#!/usr/bin/env python3
"""V272 LERP-ONLY control: restore the exact stock DSR postmerge SpecTex.a gate.

This is a falsification build, NOT a canonical PTDE fix. Only the R19
HemEnvLerp shader materializer is modified transiently during native
construction. Stable HemEnv shaders, source routing, PTDE DDS sidecars,
A/B blend, Fog/LightScattering, and game EXE remain unchanged.

Original R19 forces the late SpecTex.a gate to literal 1.0. Here the
exact original operand is retained. All preceding R15/R16/R17 edits
remain intact, which isolates this one suspected overgloss operator.
"""
import argparse
from pathlib import Path

TARGET = Path("renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
START = "bool apply_exact_ptde_lerp_materialworkflow_r15_r16_r17("
STOP = "bool pmetal_lerp_materialworkflow_postcondition("
TAG = "V272_LERP_STOCK_SPECTEX_ALPHA_GATE"

OLD_GATE = """    words[post_coeff_word] = 0x00004001u;
    words[post_coeff_word + 1u] = 0x3f800000u;"""
NEW_GATE = """    // V272_LERP_STOCK_SPECTEX_ALPHA_GATE: preserve original DSR
    // postmerge SpecTex.a operand for the exact R19 Lerp receiver only.
    // This must never write the stable HemEnv materializer.
    const auto v272_lerp_stock_coeff_word = words[post_coeff_word];
    const auto v272_lerp_stock_coeff_register =
        words[post_coeff_word + 1u];
    if (v272_lerp_stock_coeff_word != 0x00100006u ||
        v272_lerp_stock_coeff_register != *spec_sample_register)
        return false;"""

OLD_ASSERT = """        record_materialworkflow_assertion(
            state, post_coeff_word, 0x00004001u) &&
        record_materialworkflow_assertion(
            state, post_coeff_word + 1u, 0x3f800000u);"""
NEW_ASSERT = """        record_materialworkflow_assertion(
            state, post_coeff_word, v272_lerp_stock_coeff_word) &&
        record_materialworkflow_assertion(
            state, post_coeff_word + 1u,
            v272_lerp_stock_coeff_register);"""

def patch(original: str) -> str:
    s = original.replace("\r\n", "\n")
    if s.count(START) != 1 or s.count(STOP) != 1:
        raise AssertionError("R19 exact Lerp MaterialWorkflow function boundary drift")
    begin, end = s.index(START), s.index(STOP)
    if begin >= end:
        raise AssertionError("R19 MaterialWorkflow boundaries reversed")
    body = s[begin:end]
    if TAG in body:
        if body.count(TAG) != 1 or OLD_GATE in body or OLD_ASSERT in body:
            raise AssertionError("R19 experimental operator partially patched")
        if body.count("v272_lerp_stock_coeff_word") != 3:
            raise AssertionError("R19 native operand postcondition corrupted")
        return s
    if body.count(OLD_GATE) != 1 or body.count(OLD_ASSERT) != 1:
        raise AssertionError("R19 postmerge alpha preimage missing or ambiguous")
    if body.count("apply_exact_ptde_lerp_materialworkflow_r15_r16_r17(") != 1:
        raise AssertionError("R19 Lerp receiver route drift")
    replaced = body.replace(OLD_GATE, NEW_GATE, 1).replace(OLD_ASSERT, NEW_ASSERT, 1)
    out = s[:begin] + replaced + s[end:]
    if out[:begin] != s[:begin] or out[begin+len(replaced):] != s[end:]:
        raise AssertionError("non-Lerp source altered")
    return out

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    before = TARGET.read_text(encoding="utf-8")
    after = patch(before)
    if args.apply:
        if after != before:
            TARGET.write_text(after, encoding="utf-8", newline="\n")
    elif TAG not in before or after != before:
        raise RuntimeError("Lerp-only alpha-gate trial was not materialized")
    print("V272_LERP_STOCK_SPECTEX_ALPHA_GATE_" + ("APPLIED" if args.apply else "VERIFIED"))
    print("V272_STABLE_HEMENV_SOURCE_UNCHANGED=1")
    print("V272_LERP_STOCK_ALPHA_GATE_TRIAL_PIXEL=OPEN")

if __name__ == "__main__":
    main()
