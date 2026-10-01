#!/usr/bin/env python3
from pathlib import Path
import argparse


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise RuntimeError(f"{label}: missing {needle!r}")


def forbid(text: str, needle: str, label: str) -> None:
    if needle in text:
        raise RuntimeError(f"{label}: forbidden {needle!r}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--source-dir", required=True)
    ns = ap.parse_args()
    root = Path(ns.source_dir).resolve()

    integrated = (root / "integrated/integrated_addon.cpp").read_text(encoding="utf-8")
    native = (root / "src/runtime/pmetal_native_draw_bridge.cpp").read_text(encoding="utf-8")
    source = (root / "src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
    upper_lower = (root / "src/runtime/upper_lower_draw_runtime.cpp").read_text(encoding="utf-8")
    flver = (root / "src/runtime/flver_engine_hooks.cpp").read_text(encoding="utf-8")
    texture = (root / "src/runtime/texture_identity_transport.cpp").read_text(encoding="utf-8")
    flver_registry = (root / "src/runtime/flver_identity_registry.cpp").read_text(encoding="utf-8")
    owner_producer = (root / "src/runtime/material_owner_producer.cpp").read_text(encoding="utf-8")
    owner_selection = (root / "src/runtime/material_owner_selection.cpp").read_text(encoding="utf-8")

    require(integrated, "g_pmetal_native_draw.arm_draw(", "non-indexed single-pass arm")
    require(integrated, "g_pmetal_native_draw.arm_draw_indexed(", "indexed single-pass arm")
    require(integrated, "prepared.envspec_in_batch", "exact P_Metal batch gate")
    require(integrated, "Return false so ReShade continues into its single original", "stock wrapper continuation")
    require(integrated, "g_pmetal_native_draw.uninstall();", "explicit native hook teardown")
    require(integrated, "active_integrated_draw_route(", "runtime-active route pruning")
    require(integrated, "publish_active_dynamic_draw_routes();", "one-time active route publication")
    require(integrated, "g_upper_lower_selection_transport_active.load(", "U/L route requires live transport")
    require(integrated, "g_hemdir3_selection_transport_active.load(", "HemDir3 route requires live transport")

    for needle in (
        "k_vtbl_draw_indexed = 12u",
        "k_vtbl_draw = 13u",
        "k_vtbl_draw_indexed_instanced = 20u",
        "k_vtbl_draw_instanced = 21u",
        "capture_state(",
        "apply_mutation(",
        "restore_state(",
        "g_original_draw",
        "g_original_draw_indexed",
    ):
        require(native, needle, "native original-draw bridge")

    forbid(native, "core_.transactions()", "native bridge must not re-enter generic transaction core")
    forbid(native, "raw_replay", "native bridge must not replay a second draw")

    require(source, "pmetal_env_source_runtime::install()", "isolated P_Metal source lifecycle")
    require(source, "pmetal_env_source_selector_event(", "isolated exact selector source event")
    require(source, "pmetal_producer_state_publish(", "isolated producer publication")
    require(source, "k_pmetal_material_route = 345u", "exact P_Metal route gate")
    require(source, "P_Metal[DSB].mtd", "exact P_Metal semantic gate")
    require(flver, "if (identity.route_index == 345u)", "pre-decoder P_Metal route gate")
    require(flver, "k_selector_identity_cache_sets = 128u", "final selector identity cache sets")
    require(flver, "k_selector_identity_cache_ways = 4u", "final selector identity cache associativity")
    require(flver, "selector_identity_cache_lookup(", "final selector identity cache lookup")
    require(flver, "selector_identity_cache_publish(", "final selector identity cache publication")
    require(flver, "publish_exact_selector_identity(", "shared exact selector publication")
    forbid(upper_lower, "pmetal_env_source_runtime::install()", "active P_Metal source must not live in U/L module")
    forbid(upper_lower, "void pmetal_env_source_selector_event(", "active P_Metal selector source must not live in U/L module")

    require(flver, "g_selector_upper_lower_enabled", "U/L selector hot-path gate")
    require(flver, "g_selector_hemdir3_enabled", "HemDir3 selector hot-path gate")
    require(texture, "k_logical_name_capacity", "fixed TLS texture-name buffer")
    forbid(texture, "g_logical_name.push_back", "texture hook heap-growth removal")

    require(flver_registry, "k_lookup_tls_cache_ways = 4u", "4-way FLVER identity cache")
    require(flver_registry, "g_mutex_fallbacks", "FLVER mutex fallback telemetry")
    require(owner_producer, "k_owner_material_cache_ways = 4u", "4-way owner-to-MTD cache")
    require(owner_selection, "k_owner_auth_cache_ways = 4u", "4-way owner-auth cache")

    print("DSRRL_RUNTIME_V2_PMETAL_SINGLE_PASS_AUDIT_PASS")


if __name__ == "__main__":
    main()
