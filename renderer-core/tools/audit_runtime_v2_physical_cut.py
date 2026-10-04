#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

MACRO = "DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE"
FORBIDDEN_SOURCES = (
    "upper_lower_draw_runtime.cpp",
    "upper_lower_hemenv_draw_runtime.cpp",
    "hemdir3_draw_runtime.cpp",
    "subsurface_draw_runtime.cpp",
)
REQUIRED_SOURCE = "pmetal_env_source_runtime.cpp"
GENERATED_SOURCE = "integrated_addon_physical_cut.cpp"


def fail(msg: str) -> None:
    raise SystemExit(f"PHYSICAL_CUT_AUDIT_FAIL: {msg}")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        fail(f"{label}: missing {needle!r}")


def audit_source(root: Path) -> None:
    cmake = (root / "integrated" / "CMakeLists.txt").read_text(encoding="utf-8")
    flver = (root / "src" / "runtime" / "flver_engine_hooks.cpp").read_text(encoding="utf-8")
    generator = (root / "tools" / "generate_runtime_v2_physical_cut.py").read_text(encoding="utf-8")

    require(cmake, MACRO, "CMake option")
    require(cmake, "if(NOT " + MACRO + ")", "conditional runtime source list")
    require(cmake, "generate_runtime_v2_physical_cut.py", "generated integrated source")
    require(cmake, REQUIRED_SOURCE, "P_Metal source retention")
    for source in FORBIDDEN_SOURCES:
        require(cmake, source, "forbidden-source provenance")

    require(flver, "#ifndef " + MACRO, "selector callback compile gate")
    require(flver, "pmetal_env_source_selector_event(", "independent P_Metal selector")
    require(generator, "physical_cut_upper_lower_runtime_stub", "U/L no-op shim")
    require(generator, "physical_cut_hemdir3_runtime_stub", "HemDir3 no-op shim")
    require(generator, "physical_cut_subsurface_runtime_stub", "Subsurface no-op shim")
    require(generator, "g_pmetal_source;", "P_Metal runtime preservation")


def audit_generated(build_dir: Path) -> None:
    generated = list(build_dir.rglob(GENERATED_SOURCE))
    if len(generated) != 1:
        fail(f"expected one {GENERATED_SOURCE}, found {len(generated)}")

    source_text = generated[0].read_text(encoding="utf-8")
    require(source_text, "DSRRL PHYSICAL_CUT_UL_H3_SUBSURFACE generated source", "generated marker")
    require(source_text, "physical_cut_upper_lower_runtime_stub\n    g_upper_lower;", "generated U/L shim global")
    require(source_text, "physical_cut_hemdir3_runtime_stub\n    g_hemdir3;", "generated H3 shim global")
    require(source_text, "physical_cut_subsurface_runtime_stub\n    g_subsurface;", "generated Subsurface shim global")
    require(source_text, "dsrrl::runtime::pmetal_env_source_runtime\n    g_pmetal_source;", "generated P_Metal global")

    for forbidden_global in (
        "dsrrl::runtime::upper_lower_draw_runtime\n    g_upper_lower",
        "dsrrl::runtime::upper_lower_hemenv_draw_runtime\n    g_upper_lower_hemenv",
        "dsrrl::runtime::hemdir3_draw_runtime\n    g_hemdir3",
        "dsrrl::runtime::subsurface_draw_runtime\n    g_subsurface",
    ):
        if forbidden_global in source_text:
            fail(f"real runtime global survived: {forbidden_global}")

    projects = list(build_dir.rglob("dsrrl_renderer_core_integrated.vcxproj"))
    if len(projects) != 1:
        fail(f"expected one integrated vcxproj, found {len(projects)}")
    project_text = projects[0].read_text(encoding="utf-8", errors="ignore").lower()

    if GENERATED_SOURCE.lower() not in project_text:
        fail("generated physical-cut integrated source is not the compiled target source")
    if MACRO.lower() not in project_text:
        fail("physical-cut compile definition is missing from generated target")
    if REQUIRED_SOURCE.lower() not in project_text:
        fail("P_Metal source runtime is missing from generated target")
    for source in FORBIDDEN_SOURCES:
        if source.lower() in project_text:
            fail(f"forbidden runtime source remains in generated target: {source}")


def audit_objects(build_dir: Path) -> None:
    objects = {p.name.lower() for p in build_dir.rglob("*.obj")}
    if REQUIRED_SOURCE[:-4].lower() + ".obj" not in objects:
        fail("P_Metal source runtime object is missing after build")
    for source in FORBIDDEN_SOURCES:
        obj = source[:-4].lower() + ".obj"
        if obj in objects:
            fail(f"forbidden runtime object was compiled: {obj}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", type=Path, default=Path("renderer-core"))
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--post-build", action="store_true")
    args = parser.parse_args()

    audit_source(args.source_dir)
    if args.build_dir is not None:
        audit_generated(args.build_dir)
        if args.post_build:
            audit_objects(args.build_dir)

    print("PHYSICAL_CUT_AUDIT_PASS")


if __name__ == "__main__":
    main()
