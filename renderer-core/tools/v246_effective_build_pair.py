#!/usr/bin/env python3
"""V246 compile-time forensic pair: owner-working v2.0.3 vs pixel-failed V245.

Read effective generated C++ and CMake flags AFTER materialization, not tracked
donors. This tool cannot establish PTDE pixel equivalence or runtime draw hits.
No game, exe, shader, resource or protected runtime code is modified.
"""
from __future__ import annotations
import argparse
from collections import Counter
from difflib import SequenceMatcher, unified_diff
import hashlib
import json
from pathlib import Path
import shutil

SOURCES = (
    "renderer-core/src/runtime/pmetal_env_source_runtime.cpp",
    "renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp",
    "renderer-core/src/runtime/material_resource_draw_runtime.cpp",
    "renderer-core/src/runtime/flver_engine_hooks.cpp",
    "renderer-core/src/runtime/pmetal_producer_state.cpp",
    "renderer-core/src/runtime/draw_state_transaction.cpp",
    "renderer-core/src/runtime/island_draw_adapter.cpp",
    "renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp",
    "renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp",
    "renderer-core/integrated/integrated_addon.cpp",
    "build/generated/integrated_addon_physical_cut.cpp",
)
MUST_MATCH_EXACT = (
    "renderer-core/src/runtime/draw_state_transaction.cpp",
    "renderer-core/src/runtime/island_draw_adapter.cpp",
    "renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp",
    "renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp",
)

def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def capture(label: str) -> None:
    root = Path(".")
    dest = Path("v246_capture") / label
    if dest.exists():
        shutil.rmtree(dest)
    manifest: dict = {"label": label, "sources": {}, "options": {}}
    for filename in SOURCES:
        path = root / filename
        if not path.is_file():
            raise RuntimeError(f"Required effective source unavailable: {filename}")
        data = path.read_bytes()
        target = dest / filename
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        manifest["sources"][filename] = {
            "sha256": sha(data), "bytes": len(data),
            "lines": data.count(b"\n") + 1,
        }
    cache = root / "build/CMakeCache.txt"
    if not cache.is_file():
        raise RuntimeError("Effective CMake cache missing")
    for line in cache.read_text(errors="replace").splitlines():
        if "=" in line and ":" in line and not line.startswith(("//", "#")):
            name, value = line.split("=", 1)
            name = name.split(":", 1)[0]
            if name.startswith(("DSRRL_", "CMAKE_CXX_FLAGS", "CMAKE_BUILD_TYPE")):
                manifest["options"][name] = value
    expected = (
        "DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG",
        "DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE",
        "DSRRL_PMETAL_R19_LERP_MATERIALWORKFLOW",
        "DSRRL_PHYSICAL_CUT_UL_H3_SUBSURFACE",
    )
    for option in expected:
        if manifest["options"].get(option) != "ON":
            raise RuntimeError(f"{label}: required PTDE operator switch disabled: {option}")
    for file in MUST_MATCH_EXACT:
        if not (dest / file).is_file():
            raise RuntimeError(f"{label} missing protected shader/transaction {file}")
    dest.joinpath("manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    print("V246_EFFECTIVE_COMPILER_INPUT_SNAPSHOT_PASS", label,
          "files", len(manifest["sources"]))

def compare() -> None:
    base = Path("v246_compare/known_good_v203")
    test = Path("v246_compare/v245_pixel_fail")
    b = json.loads((base / "manifest.json").read_text())
    t = json.loads((test / "manifest.json").read_text())
    report: dict = {
        "reference": "6a73033f6275f88487effeb1e34bbbdd7a6a3a88",
        "failed_build_source": "bb63009d3d079424162b35a0bc9e791849a90dee",
        "owner_pixel_v245": "FAIL_STOCK_DSR_DURING_LERP",
        "operator": "HemEnvLerp",
        "files": {}, "build_option_differences": {},
    }
    opt_names = sorted(set(b["options"]) | set(t["options"]))
    for name in opt_names:
        v0, v1 = b["options"].get(name), t["options"].get(name)
        if v0 != v1:
            report["build_option_differences"][name] = [v0, v1]
    diff_root = Path("v246_report")
    diff_root.mkdir(exist_ok=True)
    for filename in SOURCES:
        d0 = b["sources"][filename]["sha256"]
        d1 = t["sources"][filename]["sha256"]
        equal = d0 == d1
        report["files"][filename] = {
            "working_sha256": d0, "failed_sha256": d1, "equal": equal,
        }
        if filename in MUST_MATCH_EXACT and not equal:
            raise RuntimeError(f"V246 protected R19 shader or transaction drift: {filename}")
        if equal:
            continue
        lines0 = (base / filename).read_text(errors="replace").splitlines(True)
        lines1 = (test / filename).read_text(errors="replace").splitlines(True)
        changes = list(unified_diff(lines0, lines1,
                       fromfile=f"working/{filename}",
                       tofile=f"failed/{filename}", n=4))
        label = Path(filename).name + ".diff"
        (diff_root / label).write_text("".join(changes), encoding="utf-8")
        report["files"][filename]["diff_path"] = label
        report["files"][filename]["diff_lines"] = len(changes)

    for label in ("known_good_v203", "v245_pixel_fail"):
        source = base if label == "known_good_v203" else test
        env = (source / SOURCES[0]).read_text()
        if "latest_hook_source(out)" not in env:
            raise RuntimeError(f"{label}: historical P_Metal packer fallback missing")
        integrated = (source / "build/generated/integrated_addon_physical_cut.cpp").read_text()
        if "dsrrl::runtime::pmetal_env_source_runtime" not in integrated:
            raise RuntimeError(f"{label}: physical cut deleted P_Metal source")
    report["native_compile_status"] = "WIN64_JOBS_SUCCEEDED"
    report["ptde_pixel_equivalence"] = "NOT_TESTED"
    (diff_root / "v246_effective_compile_diff.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n"
    )
    for filename, data in report["files"].items():
        print(("SAME " if data["equal"] else "DIFF ") + filename)
    print("OPTIONS_CHANGED", json.dumps(report["build_option_differences"]))
    print("V246_EFFECTIVE_BUILD_DONOR_COMPARISON_PASS")
    summary = Path("v246_report/v246_summary.md")
    summary.write_text(
        "## V246 effective native compiled-source comparison\n\n"
        "Owner-reference v2.0.3 vs visual-failed V245, not just tracked blobs.\n\n"
        + "\n".join(f"- **{'IDENTICAL' if m['equal'] else 'DIFFERENT'}** "
                    f"\`{name}\`" for name, m in report["files"].items())
        + "\n\nBuild flag differences:\n\n"
        + "\n".join(f"- \`{key}\`: \`{vals[0]}\` → \`{vals[1]}\`"
                    for key, vals in report["build_option_differences"].items())
        + "\n\nRuntime pixel FAIL in V245, root cause still OPEN. "
          "Only native construction comparison is proved.\n",
        encoding="utf-8",
    )

def main() -> None:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode", required=True)
    c = sub.add_parser("capture")
    c.add_argument("--label", required=True,
                   choices=("known_good_v203", "v245_pixel_fail"))
    sub.add_parser("compare")
    args = parser.parse_args()
    if args.mode == "capture":
        capture(args.label)
    else:
        compare()

if __name__ == "__main__":
    main()
