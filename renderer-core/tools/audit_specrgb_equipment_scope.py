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
    resources = (root / "src/runtime/material_resource_draw_runtime.cpp").read_text(encoding="utf-8")
    pmetal = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
    subsurface = (root / "src/runtime/subsurface_draw_runtime.cpp").read_text(encoding="utf-8")

    # Equipment ownership is mandatory. Receiver/MTD similarity alone may not
    # authorize PTDE SpecRGB sidecars.
    require(
        resources,
        "PTDE texture sidecars are authorized only on authenticated equipment draws.",
        "equipment-only resource policy",
    )
    require(
        resources,
        "query.material.owner_tuple_exact",
        "exact equipment owner tuple",
    )
    require(
        resources,
        "spec_equipment_name_hash_allowed_v12",
        "exact equipment SpecRGB logical identity",
    )

    # Candidate telemetry must represent only actual equipment consumers.
    require(
        integrated,
        "const bool equipment_owner_exact =",
        "equipment candidate gate",
    )
    require(
        integrated,
        "Do not pre-mark\n    // it as an effect candidate here.",
        "no generic SpecRGB effect candidate",
    )
    require(
        integrated,
        "observe_equipment_specrgb_carrier(",
        "passive equipment carrier probe",
    )
    require(
        integrated,
        "exact_specular_companion_ready(",
        "exact equipment companion liveness probe",
    )

    legacy = """if (material.owner_tuple_exact) {
        mask |= effect_probe_bit(
            effect_probe_id::spec_rgb);"""
    if legacy in integrated:
        fail("legacy owner-only generic SpecRGB candidate gate is present")

    # Generic MR must keep stock t1/PBL semantics and never advertise a t10
    # SpecRGB consumer after the MR operator reset.
    require(
        integrated,
        "generic stable MR is diffuse-v1 only.",
        "generic MR anti-hybrid guard",
    )
    require(
        integrated,
        "generic diffuse MR must never advertise\n            // a SpecRGB consumer.",
        "generic Lerp anti-hybrid guard",
    )

    # Three legal equipment consumers are source-complete.
    require(
        integrated,
        "prepare_fixed_pointlight_material_requests(",
        "fixed local-specular SpecRGB carrier",
    )
    require(
        integrated,
        "prepared.resources.spec_rgb &&",
        "fixed local-specular exact SpecRGB readiness",
    )

    require(
        pmetal,
        "prepare_draw_requests(",
        "P_Metal EnvSpec SpecRGB carrier",
    )
    require(
        pmetal,
        "!prepared.material_resources.spec_rgb",
        "P_Metal EnvSpec exact SpecRGB readiness",
    )

    require(
        subsurface,
        "prepare_subsurface_body_requests(",
        "Subsurface body SpecRGB carrier",
    )
    require(
        subsurface,
        "!prepared.resources.spec_rgb",
        "Subsurface exact SpecRGB readiness",
    )

    print("DSRRL_SPECRGB_EQUIPMENT_SCOPE_PASS")
    print("  carrier=exact equipment owner/resource -> PTDE sidecar -> consumer-local transaction")
    print("  carrier_probe=passive_no_bind")
    print("  legal_consumers=fixed_local_specular,pmetal_envspec,subsurface")
    print("  effect_probe=prepared_resource_only")
    print("  generic_mr=stock_t1_no_specrgb")
    print("  clustered_pointlight=independent_no_inherited_specrgb")


if __name__ == "__main__":
    main()
