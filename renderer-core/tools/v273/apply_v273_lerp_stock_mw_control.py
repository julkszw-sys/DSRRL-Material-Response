#!/usr/bin/env python3
"""V273 reversible LERP-ONLY MaterialWorkflow operator isolation on exact V272.

Construction-only controlled shader change: keep the *stock DSR*
MaterialWorkflow expressions in the HemEnvLerp pixel shader, rather than
performing the R19 R15/R16/R17 PTDE MaterialWorkflow rewrites. The exact
existing EnvSpec/EnvDiffuse A/B shader and sidecar routes are NOT modified.
Stable HemEnv is never compiled through this file: its materializer is
pmetal_rgba_materializer.cpp (untouched). Production default remains V272.

No proof of the pixel cause is implied. This is a bounded A/B experiment.
"""
import argparse
from pathlib import Path

LERP = Path("renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
INTEGRATED = Path("renderer-core/integrated/integrated_addon.cpp")
DEFINE = "#define DSRRL_V273_LERP_STOCK_MW_CONTROL 1"
BASE_IF = "#if defined(DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW)"
GUARDED_IF = BASE_IF + " && !defined(DSRRL_V273_LERP_STOCK_MW_CONTROL)"
HEADER = '#include "dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp"'
LOG_TAG = "[DSRRL V273 LERP DSR MW CONTROL]"
NATIVE_REGISTER = """                if (g_pmetal_envspec.
                        register_lerp_replacement(
                            envspec_lerp,
                            envspec_lerp_payload.data(),
                            envspec_lerp_payload.size()))
                    ++g_envspec_payload_materialize_ok;
                else
                    ++g_envspec_payload_materialize_fail;"""
CONTROL_REGISTER = """                if (g_pmetal_envspec.
                        register_lerp_replacement(
                            envspec_lerp,
                            envspec_lerp_payload.data(),
                            envspec_lerp_payload.size())) {
                    ++g_envspec_payload_materialize_ok;
                    static std::atomic_bool v273_control_logged{false};
                    if (!v273_control_logged.exchange(
                            true, std::memory_order_relaxed))
                        reshade::log::message(
                            reshade::log::level::info,
                            "[DSRRL V273 LERP DSR MW CONTROL] "
                            "rx_exact=1 lerp_r19_materialworkflow=STOCK_DSR "
                            "stable_hemenv=V272_UNMODIFIED "
                            "ptde_t10_c101=V272 ptde_env_ab=V272 "
                            "fog_ls_rt1=STOCK_DSR "
                            "construction=PASS runtime_pixel=OPEN");
                } else {
                    ++g_envspec_payload_materialize_fail;
                }"""

def transform_lerp(src: str) -> str:
    src = src.replace("\r\n", "\n")
    if src.count(DEFINE) == 1:
        if src.count(GUARDED_IF) != 6 or src.count(BASE_IF) != 6:
            raise AssertionError("V273 partial or drifted Lerp preprocessor contract")
        return src
    if src.count(HEADER) != 1 or src.count(BASE_IF) != 6:
        raise AssertionError("Unexpected R19 Lerp source / missing exact six gates")
    if "v271_preserves_stock_atmos_glow(" not in src:
        raise AssertionError("V271 native Fog/LightScattering/RT1 create guard missing")
    if "materialize_pmetal_rgba_lerp_receiver(" not in src:
        raise AssertionError("Wrong translation unit")
    out = src.replace(HEADER, HEADER+"\n"+DEFINE+"\n", 1)
    out = out.replace(BASE_IF, GUARDED_IF)
    # Critical: every change is local to this Lerp-specific translation unit.
    assert out.count(GUARDED_IF) == 6
    for unchanged in (
        "apply_exact_ptde_envdiffuse_lerp_consumer(",
        "k_ptde_rgba_envspec_chain",
        "materialize_spec_rgb_consumer(",
        "v271_preserves_stock_atmos_glow(",
    ):
        if src.count(unchanged) != out.count(unchanged):
            raise AssertionError("V273 accidentally altered independent Lerp island")
    return out

def transform_integrated(src: str) -> str:
    src = src.replace("\r\n", "\n")
    if LOG_TAG in src:
        if src.count(LOG_TAG) != 1 or CONTROL_REGISTER not in src:
            raise AssertionError("V273 create-time diagnostic registration drift")
        return src
    if src.count(NATIVE_REGISTER) != 1:
        raise AssertionError("Exact Lerp-only shader registration site missing")
    return src.replace(NATIVE_REGISTER, CONTROL_REGISTER, 1)

def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--apply", action="store_true")
    args = p.parse_args()
    before_lerp = LERP.read_text(encoding="utf-8")
    before_addon = INTEGRATED.read_text(encoding="utf-8")
    after_lerp = transform_lerp(before_lerp)
    after_addon = transform_integrated(before_addon)
    if args.apply:
        if before_lerp != after_lerp:
            LERP.write_text(after_lerp, encoding="utf-8", newline="\n")
        if before_addon != after_addon:
            INTEGRATED.write_text(after_addon, encoding="utf-8", newline="\n")
    elif before_lerp != after_lerp or before_addon != after_addon:
        raise AssertionError("V273 Lerp control not materialized")
    print("V273_LERP_STOCK_DSR_MATERIALWORKFLOW_" + ("APPLIED" if args.apply else "VERIFIED"))
    print("V273_R19_DISABLED_WINDOWS=6 of 6, LERP_ONLY")
    print("V273_STABLE_HEMENV_AND_OTHER_OPERATORS_UNCHANGED=SOURCE_SCOPED")
    print("V273_GAME_RUNTIME_AND_PTDE_PIXEL=OPEN")

if __name__ == "__main__":
    main()
