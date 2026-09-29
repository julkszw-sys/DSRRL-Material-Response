#!/usr/bin/env python3
import argparse
from pathlib import Path


def fail(msg: str) -> None:
    raise RuntimeError(msg)


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        fail(f"{label}: missing {needle!r}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-dir", required=True)
    ns = ap.parse_args()
    root = Path(ns.source_dir).resolve()

    integrated = (root / "integrated/integrated_addon.cpp").read_text(encoding="utf-8")
    ul_h = (root / "include/dsrrl/runtime/upper_lower_draw_runtime.hpp").read_text(encoding="utf-8")
    ul_cpp = (root / "src/runtime/upper_lower_draw_runtime.cpp").read_text(encoding="utf-8")
    env_cpp = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
    flver_cpp = (root / "src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
    lerp_cpp = (root / "src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")
    lerp_h = (root / "include/dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp").read_text(encoding="utf-8")

    # P_Metal source transport is independent of visible U/L activation.
    require(
        integrated,
        "const bool pmetal_envspec_enabled =",
        "P_Metal EnvSpec feature gate",
    )
    require(
        integrated,
        "const bool lightbank_reference_transport_required =\n        upper_lower_enabled ||\n        pmetal_envspec_enabled;",
        "LightBank transport independent of U/L",
    )
    require(
        integrated,
        "g_upper_lower.install(\n             !upper_lower_enabled)",
        "reference-only install when U/L is disabled",
    )

    # Installing the shared carrier must never silently re-enable U/L inside
    # the EnvSpec composite.
    require(
        integrated,
        "const bool envspec_ul_verified =\n        g_core.features().enabled(\n            dsrrl::core::operator_id::upper_lower)",
        "EnvSpec U/L composition feature isolation",
    )

    # Reference-only mode must carry exact source/selector tuples but avoid
    # heavy U/L/D123 evaluation on the producer hot path.
    require(
        ul_h,
        "bool install(bool reference_only = false) noexcept;",
        "reference-only API",
    )
    require(
        ul_cpp,
        "g_reference_only_transport",
        "reference-only runtime flag",
    )
    require(
        ul_cpp,
        "P_Metal EnvSpec requires only the exact source/selector",
        "steady reference-only branch",
    )
    require(
        ul_cpp,
        "if (g_reference_only_transport.load(\n                    std::memory_order_acquire))\n                return result;",
        "blend packer reference-only branch",
    )
    require(
        ul_cpp,
        "if (g_reference_only_transport.load(\n            std::memory_order_acquire)) {\n        return g_blend_orig != nullptr",
        "blend helper reference-only bypass",
    )

    # The old producer-local P_Metal semantic decode stays disabled. EnvSpec
    # now resolves the source lazily only after the exact draw consumer gate.
    require(
        ul_cpp,
        "g_pmetal_env_hook_armed.store(false);",
        "no shared producer-local P_Metal decode",
    )
    require(
        env_cpp,
        "selected_pmetal_env_source(",
        "consumer-local P_Metal source decode",
    )
    require(
        env_cpp,
        "k_effect_fail_source = 1u << 4u;",
        "source frontier telemetry",
    )

    # P_Metal selected state requires the conjunction of exact selector state
    # and exact actual material identity from the same FLVER selector callback.
    require(
        ul_h,
        "upper_lower_pmetal_material_event_bridge(",
        "P_Metal selector-material join API",
    )
    require(
        ul_cpp,
        "invalidate_selected_reference_token_for_producer(",
        "selector invalidation of stale P_Metal state",
    )
    require(
        ul_cpp,
        "exact_pmetal_material_selection(",
        "exact P_Metal material gate",
    )
    require(
        ul_cpp,
        "g_draw_reference_token.fingerprint.owner !=",
        "selector/material owner continuity",
    )
    require(
        flver_cpp,
        "upper_lower_pmetal_material_event_bridge(",
        "FLVER selector material authorization bridge",
    )

    if ul_cpp.count("publish_selected_reference_token(") != 2:
        fail("P_Metal selected state has an unexpected publication surface")

    # Focused diagnostic: only the exact P_Metal selected-state path may
    # substitute the immutable PTDE Firelink LightBank EnvSpec donor values.
    # It must remain opt-in and must not mutate global LightBank/DrawParam state.
    require(
        ul_cpp,
        "DSRRL_EXPERIMENTAL_PMETAL_PTDE_FIRELINK_DRAWPARAM",
        "P_Metal-only hardcoded PTDE DrawParam diagnostic opt-in",
    )
    require(
        ul_cpp,
        "k_ptde_firelink_lightbank_signature =\n        0xa710f288bd3aca82ULL;",
        "exact PTDE Firelink donor bank",
    )
    require(
        ul_cpp,
        "read_hardcoded_ptde_firelink_pmetal_env_source(",
        "hardcoded PTDE EnvSpec source",
    )
    require(
        ul_cpp,
        "hardcoded_ptde_drawparam\n                ? read_hardcoded_ptde_firelink_pmetal_env_source(",
        "P_Metal source-equation substitution",
    )

    # HemEnvLerp EnvSpec must remain usable while visible U/L is intentionally
    # OFF. The U/L-off payload preserves stock b0[7]/b0[8] operands and does
    # not require/bind b13; the composed PTDE-b13 variant remains available
    # only when the U/L feature is explicitly enabled.
    require(
        lerp_cpp,
        "const bool compose_upper_lower =\n        features.enabled(\n            core::operator_id::upper_lower);",
        "Lerp materialization feature split",
    )
    require(
        lerp_cpp,
        "outcome.upper_lower_preserved_stock =\n        !compose_upper_lower;",
        "stock-U/L Lerp outcome",
    )
    require(
        lerp_cpp,
        "if (compose_upper_lower) {\n        for (const auto &[slot_word, source_register]",
        "conditional b13 operand remap",
    )
    require(
        env_cpp,
        "!(outcome.upper_lower_composed ^\n          outcome.upper_lower_preserved_stock)",
        "exclusive Lerp U/L mode registration",
    )
    require(
        env_cpp,
        "hemenvlerp &&\n        lerp_upper_lower_composed",
        "runtime b13 only for composed Lerp",
    )
    if "core::operator_id::upper_lower) ||\n         !core_.features().enabled(\n             core::operator_id::terminal_sat_rgb" in env_cpp:
        fail("HemEnvLerp still hard-requires the visible U/L feature")

    print("DSRRL_PMETAL_ENVSPEC_REFERENCE_TRANSPORT_PASS")
    print("  U/L visible operator=OFF remains stock when not explicitly enabled")
    print("  LightBank reference carrier=ON for P_Metal EnvSpec")
    print("  producer payload=owner+sourceA/B+selectorA/B+beta only")
    print("  selected state=exact selector + exact actual P_Metal material")
    print("  P_Metal source decode=consumer-local after exact material/semantic gate")
    print("  HemEnvLerp U/L-off=stock b0 operands preserved; no b13 requirement")


if __name__ == "__main__":
    main()
