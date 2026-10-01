#!/usr/bin/env python3
import argparse
from pathlib import Path
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--cmake", required=True)
    args = parser.parse_args()

    source_dir = Path(args.source_dir).resolve()
    cmake_path = Path(args.cmake).resolve()
    pointlight_dir = source_dir / "src" / "operators" / "point_light"

    cmake_text = cmake_path.read_text(encoding="utf-8")
    sources = sorted(pointlight_dir.glob("*.cpp"))

    missing = []
    for path in sources:
        rel = path.relative_to(source_dir).as_posix()
        if rel not in cmake_text:
            missing.append(rel)

    if missing:
        print("PointLight core source audit: missing CMake entries:", file=sys.stderr)
        for rel in missing:
            print(f"  {rel}", file=sys.stderr)
        return 1

    # Runtime architecture invariant: PointLight owns its selector callback
    # directly from the FLVER semantic cut. It must not depend on the
    # Upper/Lower runtime merely to receive owner identity.
    flver_hooks = (
        source_dir / "src" / "runtime" / "flver_engine_hooks.cpp"
    ).read_text(encoding="utf-8")
    upper_lower = (
        source_dir / "src" / "runtime" / "upper_lower_draw_runtime.cpp"
    ).read_text(encoding="utf-8")
    integrated = (
        source_dir / "integrated" / "integrated_addon.cpp"
    ).read_text(encoding="utf-8")

    required_flver = (
        '#include "dsrrl/runtime/fixed_pointlight_draw_runtime.hpp"',
        "fixed_pointlight_selector_event_bridge(owner);",
        "clustered_pnts_selector_event_bridge(",
    )
    for token in required_flver:
        if token not in flver_hooks:
            print(
                f"PointLight runtime audit: FLVER direct dispatch missing: {token}",
                file=sys.stderr,
            )
            return 1

    bridge_begin = upper_lower.find(
        "void upper_lower_selector_event_bridge("
    )
    bridge_end = upper_lower.find(
        "\nvoid upper_lower_pmetal_material_event_bridge(",
        bridge_begin,
    )
    if bridge_begin < 0 or bridge_end <= bridge_begin:
        print(
            "PointLight runtime audit: cannot isolate U/L selector bridge",
            file=sys.stderr,
        )
        return 1
    if "fixed_pointlight_selector_event_bridge" in upper_lower[
        bridge_begin:bridge_end
    ]:
        print(
            "PointLight runtime audit: Fixed PL still routed through U/L selector bridge",
            file=sys.stderr,
        )
        return 1

    fast_begin = integrated.find(
        "bool observe_pointlight_draw_identity("
    )
    fast_end = integrated.find(
        "\nbool observe_draw_identity(",
        fast_begin,
    )
    if fast_begin < 0 or fast_end <= fast_begin:
        print(
            "PointLight runtime audit: exact-route draw fast path missing",
            file=sys.stderr,
        )
        return 1

    fast_path = integrated[fast_begin:fast_end]
    forbidden_generic_receivers = (
        "stable_receiver_bound(",
        "hemenvlerp_receiver_bound(",
        "subsurface_receiver_bound(",
        "hemdir3_receiver_bound(",
        "upper_lower_receiver_bound(",
    )
    for token in forbidden_generic_receivers:
        if token in fast_path:
            print(
                f"PointLight runtime audit: fast path regressed into generic receiver census: {token}",
                file=sys.stderr,
            )
            return 1

    if integrated.count("observe_pointlight_draw_identity(") < 3:
        print(
            "PointLight runtime audit: both draw dispatch sites must use the fast path",
            file=sys.stderr,
        )
        return 1

    print(
        f"PointLight core source audit: PASS ({len(sources)} implementation units; "
        "dedicated selector + exact-route draw fast path)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
