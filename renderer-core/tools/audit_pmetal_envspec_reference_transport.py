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
    env_h = (root / "include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp").read_text(encoding="utf-8")
    env_cpp = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
    source_h = (root / "include/dsrrl/runtime/pmetal_env_source_runtime.hpp").read_text(encoding="utf-8")
    flver_cpp = (root / "src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
    lerp_cpp = (root / "src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf-8")
    lerp_h = (root / "include/dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp").read_text(encoding="utf-8")

    # EnvSpec source transport is independent from visible U/L.
    require(integrated, "g_pmetal_source;", "isolated source instance")
    require(integrated, "g_pmetal_source.install()", "isolated source activation")
    require(
        integrated,
        "const bool lightbank_reference_transport_required =\n        upper_lower_enabled;",
        "U/L transport remains U/L-only",
    )
    if "upper_lower_enabled ||\n        pmetal_envspec_enabled" in integrated:
        fail("EnvSpec still forces shared U/L transport")

    require(
        source_h,
        "class pmetal_env_source_runtime",
        "isolated source runtime API",
    )
    require(
        ul_cpp,
        "bool install_pmetal_source_only_carrier() noexcept",
        "isolated source-only installer",
    )

    source_install_begin = ul_cpp.index(
        "bool install_pmetal_source_only_carrier() noexcept"
    )
    source_install_end = ul_cpp.index(
        "\nbool restore_pmetal_source_only_carrier() noexcept",
        source_install_begin,
    )
    if source_install_begin < 0 or source_install_end <= source_install_begin:
        fail("cannot isolate P_Metal source-only install block")
    source_install = ul_cpp[source_install_begin:source_install_end]

    require(
        source_install,
        "g_pmetal_source_steady_hook",
        "dedicated steady source cut 0x563B80",
    )
    require(
        source_install,
        "g_pmetal_source_blend_hook",
        "dedicated blended source cut 0x563C30",
    )
    require(
        source_install,
        "g_pmetal_source_only_enabled.store(",
        "source-only runtime activation latch",
    )
    for forbidden in (
        "g_hooks[0]",
        "g_hooks[1]",
        "g_hooks[2]",
        "g_hooks[3]",
        "g_hooks[4]",
        "g_pmetal_env_hook",
        "g_steady_eval_tail_hook",
        "g_steady_cache_builder_hook",
        "g_upper_lower.install",
    ):
        if forbidden in source_install:
            fail(
                "isolated P_Metal EnvSpec carrier widened into shared U/L "
                f"hook surface: {forbidden}"
            )

    enabled_at = source_install.find(
        "g_pmetal_source_only_enabled.store("
    )
    steady_arm_at = source_install.find(
        "arm_hook(\n            g_pmetal_source_steady_hook)"
    )
    blend_arm_at = source_install.find(
        "arm_hook(\n            g_pmetal_source_blend_hook)"
    )
    if (
        enabled_at < 0
        or steady_arm_at < 0
        or blend_arm_at < 0
        or enabled_at < steady_arm_at
        or enabled_at < blend_arm_at
    ):
        fail(
            "source-only activation latch must publish only after both retail "
            "cuts are armed"
        )

    blend_hook_begin = ul_cpp.index(
        "void __fastcall hook_pmetal_source_blend("
    )
    blend_hook_end = ul_cpp.index(
        "\nvoid __fastcall hook_steady_packer(",
        blend_hook_begin,
    )
    if blend_hook_begin < 0 or blend_hook_end <= blend_hook_begin:
        fail("cannot isolate P_Metal blend detour")
    blend_hook = ul_cpp[blend_hook_begin:blend_hook_end]

    require(
        blend_hook,
        "const bool endpoint_a =",
        "retail endpoint-A mode",
    )
    require(
        blend_hook,
        "beta <= 0.0f",
        "retail endpoint-A collapse condition",
    )
    require(
        blend_hook,
        "const bool endpoint_b =",
        "retail endpoint-B mode",
    )
    require(
        blend_hook,
        "beta >= 1.0f",
        "retail endpoint-B collapse condition",
    )
    require(
        blend_hook,
        "if (endpoint_a || endpoint_b) {",
        "endpoint collapse delegated to steady source cut",
    )
    require(
        blend_hook,
        "const bool interior_blend =",
        "interior blend gate",
    )
    require(
        blend_hook,
        "if (interior_blend) {",
        "A/B decode restricted to true interior blend",
    )
    require(
        blend_hook,
        "Advancing the serial before any attempted decode invalidates an older",
        "failed event invalidates stale source by serial advance",
    )

    require(
        ul_cpp,
        "out.serial != latest_source_serial",
        "draw consumer rejects stale source payload",
    )
    require(
        ul_cpp,
        "g_pmetal_source_serial.fetch_add(",
        "source events carry monotonic freshness serial",
    )

    ul_install_begin = ul_cpp.index(
        "bool upper_lower_draw_runtime::install("
    )
    ul_install_end = ul_cpp.index(
        "\nvoid upper_lower_draw_runtime::uninstall()",
        ul_install_begin,
    )
    if ul_install_begin < 0 or ul_install_end <= ul_install_begin:
        fail("cannot isolate visible U/L install block")
    ul_install = ul_cpp[ul_install_begin:ul_install_end]
    require(
        ul_install,
        "if (g_pmetal_source_only_enabled.load(std::memory_order_acquire))\n        return false;",
        "mutual exclusion between isolated EnvSpec carrier and visible U/L runtime",
    )

    require(
        env_cpp,
        "source_.latest(source)",
        "isolated source consumer",
    )

    steady_generic_begin = ul_cpp.index(
        "void __fastcall hook_steady_packer("
    )
    steady_generic_end = ul_cpp.index(
        "\nvoid *__fastcall hook_blend(",
        steady_generic_begin,
    )
    generic_steady = ul_cpp[steady_generic_begin:steady_generic_end]
    if "g_pmetal_source_only_enabled" in generic_steady:
        fail("isolated EnvSpec source path re-entered generic U/L steady detour")

    generic_blend_begin = ul_cpp.index(
        "void __fastcall hook_pmetal_env_blend("
    )
    generic_blend_end = ul_cpp.index(
        "\nbool install_optional_pmetal_env_hook() noexcept",
        generic_blend_begin,
    )
    generic_blend = ul_cpp[generic_blend_begin:generic_blend_end]
    if "g_pmetal_source_only_enabled" in generic_blend:
        fail("isolated EnvSpec source path re-entered generic U/L blend detour")

    for required in (
        "inline_hook g_pmetal_source_steady_hook{};",
        "inline_hook g_pmetal_source_blend_hook{};",
        "g_pmetal_source_steady_orig",
        "g_pmetal_source_blend_orig",
    ):
        require(ul_cpp, required, "dedicated EnvSpec source hook ownership")

    # The active EnvSpec draw runtime must have no dependency on the U/L
    # runtime at all. U/L-off is not merely a feature branch: no b13 carrier,
    # U/L token, U/L cleanup, or U/L constructor dependency may survive.
    for forbidden in (
        "upper_lower_draw_runtime",
        "prepared_upper_lower_draw",
        "upper_lower_receiver_verified",
        "upper_lower_composed",
    ):
        if forbidden in env_h:
            fail(f"P_Metal EnvSpec header still depends on U/L: {forbidden}")

    for forbidden in (
        "lightbank_",
        "prepared.upper_lower",
        "prepare_upper_lower_carrier",
        "operator_id::upper_lower",
        "upper_lower_receiver_verified",
        "use_upper_lower",
        "upper_lower_ready_",
        "upper_lower_fallback_",
        "k_effect_fail_lerp_ul_conflict",
        "k_effect_fail_ul",
    ):
        if forbidden in env_cpp:
            fail(f"P_Metal EnvSpec draw runtime still depends on U/L: {forbidden}")

    for forbidden in (
        "upper_lower_ready",
        "upper_lower_fallback",
    ):
        if forbidden in env_h:
            fail(f"P_Metal EnvSpec telemetry still exposes U/L state: {forbidden}")

    require(
        integrated,
        "g_pmetal_envspec(\n        g_core,\n        g_pmetal_source,\n        g_envspec_resources,\n        g_material_resources);",
        "EnvSpec constructor has no U/L runtime dependency",
    )
    require(
        integrated,
        "const auto env_source =\n        g_pmetal_source.telemetry();",
        "EnvSpec telemetry reads isolated source carrier",
    )
    for forbidden in (
        "ul.pmetal_env_steady",
        "ul.pmetal_env_blend",
        "ul.pmetal_env_miss",
        "ul.pmetal_env_hook_armed",
    ):
        if forbidden in integrated:
            fail(f"EnvSpec telemetry still reads U/L-owned source state: {forbidden}")
    if "g_upper_lower.pmetal_draw_token_state()" in integrated:
        fail("EnvSpec draw routing still consumes an UpperLower token")
    if "g_upper_lower.pmetal_source_diagnostic()" in integrated:
        fail("EnvSpec telemetry still consumes UpperLower source state")
    require(
        integrated,
        "materialize_pmetal_rgba_receiver(\n                        g_core.features(),\n                        source,\n                        pixel_shader->code_size,\n                        false,",
        "stable EnvSpec materialization is permanently U/L-off",
    )

    # The isolated source carrier owns only the two recovered retail source
    # cuts. The legacy U/L selector/reference-token machinery may remain in
    # its own runtime, but EnvSpec must not consume it.
    require(
        ul_cpp,
        "void __fastcall hook_pmetal_source_steady(",
        "dedicated steady source cut implementation",
    )
    require(
        ul_cpp,
        "void __fastcall hook_pmetal_source_blend(",
        "dedicated blend source cut implementation",
    )
    require(
        ul_cpp,
        "publish_pmetal_source_only(",
        "immutable isolated source publication",
    )
    require(
        ul_cpp,
        "out.serial != latest_source_serial",
        "stale isolated source rejection",
    )
    require(
        env_cpp,
        "source_.latest(source)",
        "consumer-local isolated P_Metal source read",
    )
    require(
        env_cpp,
        "k_effect_fail_source = 1u << 4u;",
        "source frontier telemetry",
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

    # HemEnvLerp EnvSpec must remain usable while visible U/L is OFF.
    # Materialization may still understand the historical composed form, but
    # the active runtime accepts only the stock-U/L-preserving payload.
    require(
        lerp_cpp,
        "outcome.upper_lower_preserved_stock =\n        !compose_upper_lower;",
        "stock-U/L Lerp outcome",
    )
    require(
        env_cpp,
        "outcome.upper_lower_composed ||\n        !outcome.upper_lower_preserved_stock",
        "runtime rejects U/L-composed Lerp payload",
    )
    if "prepare_upper_lower_carrier" in env_cpp:
        fail("HemEnvLerp EnvSpec still binds a PTDE U/L b13 carrier")


    print("DSRRL_PMETAL_ENVSPEC_REFERENCE_TRANSPORT_PASS")
    print("  U/L visible operator=OFF")
    print("  shared U/L runtime=OFF when U/L is OFF")
    print("  P_Metal EnvSpec source=isolated V13 carrier")
    print("  isolated hook surface=dedicated objects at 0x563B80 steady + 0x563C30 blend only")
    print("  source-only detours do not enter generic U/L steady/blend detours")
    print("  0x563C30 endpoint collapse=delegated to hooked retail 0x563B80 exactly once")
    print("  0x563C30 interior blend=only path that decodes both A/B endpoints")
    print("  source freshness=monotonic event serial; stale payload replay is rejected")
    print("  source-only active latch=published only after both hooks are armed")
    print("  shared U/L wrapper/blend/cache-builder hooks=forbidden in source-only installer")
    print("  EnvSpec source/draw/materialization/telemetry do not consume U/L runtime state")
    print("  selector row address=retail low byte; full selector retained for identity")
    print("  P_Metal source decode=exact V13 bank signature + PTDE donor at live producer cut")
    print("  draw source=isolated immutable A/B+beta+bank/row carrier")
    print("  HemEnvLerp U/L-off=stock b0 operands preserved; no b13 requirement")


if __name__ == "__main__":
    main()
