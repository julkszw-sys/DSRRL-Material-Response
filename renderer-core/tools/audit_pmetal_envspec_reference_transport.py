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

    # Reference-only mode must retain the exact V13 P_Metal donor lifetime
    # cut while avoiding visible U/L/D123 evaluation on the producer path.
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
        "V13 source semantics are materialized at this retail",
        "steady producer-side P_Metal donor cut",
    )
    require(
        ul_cpp,
        "read_exact_pmetal_env_source(\n                        source_a,",
        "blend producer-side P_Metal donor A",
    )
    require(
        ul_cpp,
        "read_exact_pmetal_env_source(\n                            source_b,",
        "blend producer-side P_Metal donor B",
    )
    require(
        ul_cpp,
        "if (g_reference_only_transport.load(\n            std::memory_order_acquire)) {\n        return g_blend_orig != nullptr",
        "blend helper reference-only bypass",
    )

    # The obsolete standalone P_Metal blend hook stays disabled, but V13
    # donor materialization must happen in the already-required retail
    # steady/blend producer taps before source ownership ends.
    require(
        ul_cpp,
        "g_pmetal_env_hook_armed.store(false);",
        "standalone P_Metal blend hook remains disabled",
    )
    require(
        ul_cpp,
        "token.pmetal_env_ready =\n        producer.have_pmetal_env;",
        "immutable producer donor published with reference token",
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
        "invalidate_selected_reference_token_for_owner(",
        "material-gated invalidation of stale P_Metal state",
    )
    require(
        ul_cpp,
        "exact_pmetal_material_selection(",
        "exact P_Metal material gate",
    )

    selector_begin = ul_cpp.index(
        "void upper_lower_draw_runtime::selector_event("
    )
    pmetal_begin = ul_cpp.index(
        "void upper_lower_draw_runtime::pmetal_material_event("
    )
    if selector_begin < 0 or pmetal_begin <= selector_begin:
        fail("cannot isolate selector/P_Metal material event blocks")
    selector_block = ul_cpp[selector_begin:pmetal_begin]
    if "invalidate_selected_reference_token_for_" in selector_block:
        fail(
            "generic LightBank selector still invalidates persistent P_Metal state"
        )

    next_method = ul_cpp.find(
        "bool upper_lower_draw_runtime::",
        pmetal_begin + 1,
    )
    if next_method < 0:
        fail("cannot isolate P_Metal material event block")
    pmetal_block = ul_cpp[pmetal_begin:next_method]
    invalidate_at = pmetal_block.find(
        "invalidate_selected_reference_token_for_owner("
    )
    materialize_at = pmetal_block.find(
        "materialize_selected_pmetal_env_source("
    )
    publish_at = pmetal_block.find(
        "publish_selected_reference_token("
    )
    gate_at = pmetal_block.find(
        "exact_pmetal_material_selection("
    )
    if (
        gate_at < 0
        or invalidate_at < 0
        or materialize_at < 0
        or publish_at < 0
    ):
        fail("P_Metal material-gated state transition is incomplete")
    if not (
        gate_at < invalidate_at < materialize_at < publish_at
    ):
        fail(
            "P_Metal selected-state invalidation/materialization/publication "
            "ordering is unsafe"
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

    # The consumer-visible selector is a packed engine value. Retail DSR
    # record-table addressing uses only the low byte after the non-negative
    # sentinel check (steady 0x140563BA2, blend 0x140563CFB/0x140563D04).
    # The full selector remains part of freshness identity.
    require(
        ul_cpp,
        "bool retail_lightbank_record_index(",
        "retail LightBank selector normalization",
    )
    require(
        ul_cpp,
        "static_cast<std::uint8_t>(\n                selector)",
        "retail low-byte selector semantics",
    )
    require(
        ul_cpp,
        "read_exact_pmetal_env_source(",
        "exact PTDE donor source resolver",
    )
    require(
        ul_cpp,
        "materialize_selected_pmetal_env_source(",
        "P_Metal selector-material source materializer",
    )

    selected_source_begin = ul_cpp.index(
        "bool upper_lower_draw_runtime::selected_pmetal_env_source("
    )
    selected_source_end = ul_cpp.index(
        "pmetal_env_source_diagnostic",
        selected_source_begin + 1,
    )
    if selected_source_begin < 0 or selected_source_end <= selected_source_begin:
        fail("cannot isolate selected P_Metal EnvSpec source consumer")
    selected_source_block = ul_cpp[selected_source_begin:selected_source_end]
    if "read_exact_pmetal_env_source(" in selected_source_block:
        fail(
            "draw-time P_Metal source consumer still dereferences engine "
            "LightBank source pointers"
        )
    require(
        selected_source_block,
        "out = token.pmetal_env;",
        "immutable P_Metal source handoff to draw",
    )

    # Bank identity must retain the exact historical V13 FNV equation while
    # avoiding the integrated whole-region assumption that produced
    # SIGNATURE_INVALID on live type=4/count=64 banks.
    require(
        ul_cpp,
        "if (!safe_read(entry, row_id) ||\n            !safe_read(entry + 8u, name_offset))",
        "region-safe V13 bank row identity",
    )
    require(
        ul_cpp,
        "if (!safe_read(name_byte, ch))",
        "region-safe V13 bank name identity",
    )

    if "DSRRL_EXPERIMENTAL_PMETAL_PTDE_FIRELINK_DRAWPARAM" in ul_cpp:
        fail("obsolete hardcoded Firelink EnvSpec diagnostic still present")
    if "read_hardcoded_ptde_firelink_pmetal_env_source" in ul_cpp:
        fail("obsolete hardcoded Firelink donor resolver still present")

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
    print("  producer payload=owner+selector tuple + immutable V13 P_Metal A/B donor")
    print("  selected state=exact selector + exact actual P_Metal material")
    print("  persistent P_Metal invalidation=material-gated; generic selectors preserve state")
    print("  selector row address=retail low byte; full selector retained for identity")
    print("  P_Metal source decode=exact V13 bank signature + PTDE donor at live producer cut")
    print("  draw source=immutable decoded A/B+beta+bank/row; no late source-pointer dereference")
    print("  HemEnvLerp U/L-off=stock b0 operands preserved; no b13 requirement")


if __name__ == "__main__":
    main()
