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

    # EnvSpec must NEVER install a global LightBank source hook, even when
    # the visible Upper/Lower feature is disabled.
    for forbidden in ("hook_pmetal_source_steady", "hook_pmetal_source_blend",
                      "install_pmetal_source_only_carrier", "g_pmetal_source_payload",
                      "g_pmetal_source_serial", "consume_pmetal_source_only"):
        if forbidden in ul_cpp:
            fail(f"global EnvSpec source carrier returned: {forbidden}")
    install = ul_cpp.split("bool pmetal_env_source_runtime::install() noexcept",1)[1].split(
        "void pmetal_env_source_runtime::uninstall()",1)[0]
    for forbidden in ("prepare_hook", "arm_hook", "install_producer_hooks", "g_upper_lower"):
        if forbidden in install: fail(f"EnvSpec installs global hook: {forbidden}")
    require(integrated, "const bool lightbank_reference_transport_required =\n        upper_lower_enabled;", "U/L-only global hook ownership")
    require(flver_cpp, "pmetal_env_source_selector_clear();", "invalidate on every selector")
    require(flver_cpp, "pmetal_env_source_selector_event(owner, ret, r14, r15, selector_stack, identity);", "exact material callback")
    capture = ul_cpp.split("void pmetal_env_source_selector_event(",1)[1].split(
        "bool pmetal_env_source_runtime::latest(",1)[0]
    require(capture, "!exact_pmetal_material_selection(material)", "exact material gate")
    if capture.index("!exact_pmetal_material_selection(material)") > capture.index("readable_range"):
        fail("source memory work precedes exact P_Metal material gate")
    for needle in ("parent_return != g_base + 0x220CF0u", "descriptor_owner != owner", "descriptor + 0x4Cu", "descriptor + 0x4Eu",
                   "descriptor + 0x50u", "descriptor[0x150u]", "+ 0x2318u", "wrapper + 0x40u",
                   "pmetal_selector_policy::source", "read_exact_pmetal_env_source",
                   "g_pmetal_selected_material = material"):
        require(capture, needle, "exact selector source proof")
    for forbidden in ("arm_hook", "fetch_add", "mutex", "spin", "publish_pmetal_source_only"):
        if forbidden in capture: fail(f"global synchronization/hook in selector source: {forbidden}")
    consumer = ul_cpp.split("bool pmetal_env_source_runtime::latest(",1)[1].split(
        "pmetal_env_source_runtime_telemetry pmetal_env_source_runtime::telemetry",1)[0]
    for needle in ("!g_pmetal_selected_valid", "material.flver_sha256 !=", "material.material_slot !=",
                   "material.raw_mtd_sha256 !=", "g_pmetal_selector_epoch.load"):
        require(consumer, needle, "source lifetime/material mismatch fail-open")
    require(ul_cpp, "thread_local pmetal_envspec_source g_pmetal_selected_source", "no process-global latest donor")
    require(env_cpp, "source_.latest(material, source)", "material-scoped source consumer")

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

    # Bank identity must retain the exact historical V13 FNV equation without
    # turning the globally hooked LightBank producer into a VirtualQuery
    # syscall hot path. Validate the fixed header/table once, read its immutable
    # row fields directly, and reuse a validated VM window across name bytes.
    require(
        ul_cpp,
        "table_bytes - 8u",
        "bounded V13 fixed-table validation",
    )
    require(
        ul_cpp,
        "readable_window name_window{};",
        "cached V13 bank-name VM window",
    )
    require(
        ul_cpp,
        "ensure_readable_window(\n                    cursor,\n                    name_window)",
        "region-safe V13 bank name window",
    )
    require(
        ul_cpp,
        "std::memcpy(\n            &row_id,\n            entry,",
        "direct immutable V13 row identity read",
    )
    if "safe_read(entry, row_id)" in ul_cpp:
        fail("P_Metal bank scan regressed to per-row VirtualQuery")
    if "safe_read(name_byte, ch)" in ul_cpp:
        fail("P_Metal bank scan regressed to per-byte VirtualQuery")

    require(
        ul_cpp,
        "std::size_t pmetal_bank_cache_set(",
        "dedicated P_Metal bank-cache set mixer",
    )
    require(
        ul_cpp,
        "(value >> 13u)",
        "P_Metal bank-cache hash uses higher address bits",
    )
    require(
        ul_cpp,
        "(value >> 23u)",
        "P_Metal bank-cache hash avoids low-bit alignment collapse",
    )
    require(
        ul_cpp,
        "pmetal_bank_cache_set(base_ptr)",
        "P_Metal bank cache uses dedicated mixed set index",
    )
    pmetal_cache_begin = ul_cpp.index(
        "pmetal_bank_cache_entry &pmetal_bank_cache_for("
    )
    pmetal_cache_end = ul_cpp.index(
        "\nstd::uint64_t pmetal_fnv_byte(",
        pmetal_cache_begin,
    )
    pmetal_cache = ul_cpp[pmetal_cache_begin:pmetal_cache_end]
    if "d123_cache_set(base_ptr)" in pmetal_cache:
        fail("P_Metal bank cache regressed to low-bit d123 set indexing")

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
    print("  EnvSpec global LightBank hooks=0; existing exact FLVER selector only")
    print("  source=embedded PTDE donors, selected by exact material+bank+row+A/B")
    print("  TLS lifetime=selector interval; material/epoch mismatch fails open")
    print("  PTDE PackedGI + stock U/L continuation preserved; pixel status OPEN")


if __name__ == "__main__":
    main()
