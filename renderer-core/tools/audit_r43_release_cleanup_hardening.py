#!/usr/bin/env python3
"""R43 release-prep hardening source regression checks.

Static checks supplement, not replace, native MSVC compilation and runtime tests.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition, description):
    if not condition:
        raise SystemExit("R43_AUDIT_FAIL " + description)
    print("R43_AUDIT_PASS " + description)


transaction = (ROOT / "src/runtime/draw_state_transaction.cpp").read_text(encoding="utf-8")
resources = (ROOT / "src/runtime/material_resource_draw_runtime.cpp").read_text(encoding="utf-8")
source = (ROOT / "src/runtime/pmetal_env_source_runtime.cpp").read_text(encoding="utf-8")
flver = (ROOT / "src/runtime/flver_identity_registry.cpp").read_text(encoding="utf-8")
envspec = (ROOT / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
cmake = (ROOT / "integrated/CMakeLists.txt").read_text(encoding="utf-8")

require(
    transaction.count("release_unadopted_ps_classes(classes);") == 3,
    "three PSGetShader capture rejection paths release unadopted COM refs",
)
require(
    "k_max_sidecar_file_bytes" in resources
    and "end > k_max_sidecar_file_bytes" in resources
    and resources.index("end > k_max_sidecar_file_bytes") < resources.index("std::vector<std::uint8_t> bytes(size)"),
    "DDS bounded before full-file allocation",
)
require(
    resources.count("safe_load_sidecar(\n                native_device,") == 3
    and "catch (...) {\n        return {nullptr, load_status::unsupported};\n    }" in resources,
    "three asset classes use noexcept sidecar-path loading",
)
require(
    source.count("#if !defined(DSRRL_RELEASE_CLEANUP)") >= 9,
    "hot diagnostic source counters compile out in release-prep",
)
require(
    "#if !defined(DSRRL_RELEASE_CLEANUP)" in envspec
    and "pixel_srv_shadow_snapshot" in envspec
    and "prepare_draw_requests_bound" in envspec,
    "EnvSpec exact receiver/resource and live-SRV fallback preserved",
)
require(
    "DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL" in cmake
    and "DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE" in cmake
    and "DSRRL_RELEASE_CLEANUP" in cmake,
    "release-prep physical-cut and diagnostic restrictions retained",
)

require(
    "g_insert_generation" in flver
    and "entry.miss_generation ==" in flver
    and "g_insert_generation.fetch_add(" in flver
    and "g_epoch.fetch_add(" in flver,
    "new FLVER insert invalidates negative TLS verdict without positive owner flush",
)

print("R43_RELEASE_AUDIT_SOURCE_PASS")
