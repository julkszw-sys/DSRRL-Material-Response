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

    require(
        integrated,
        "constexpr bool k_velocity_writer_patch_enabled = false;",
        "shared velocity writer TXAA guard",
    )
    require(
        integrated,
        "const bool velocity_changed =\n        k_velocity_writer_patch_enabled &&\n        on_create_velocity_pipeline(",
        "velocity create-time guard",
    )
    require(
        integrated,
        "if (!k_velocity_writer_patch_enabled)\n        return;",
        "velocity init guard",
    )
    require(
        integrated,
        "if (k_velocity_writer_patch_enabled &&\n            vertex_bound)",
        "velocity bind guard",
    )
    require(
        integrated,
        "on_create_compute_pipeline(",
        "MotionBlurTiles consumer-local materialization",
    )
    require(
        integrated,
        "camera-fallback-disabled MotionBlurTiles compute pipeline",
        "MotionBlurTiles runtime attestation",
    )
    require(
        integrated,
        'velocity_writer=%s pixel=UNVERIFIED',
        "temporal isolation telemetry",
    )
    require(
        integrated,
        '"STOCK_TAA_GUARD"',
        "stock velocity writer runtime state",
    )

    print("DSRRL_MOTION_BLUR_TEMPORAL_ISOLATION_PASS")


if __name__ == "__main__":
    main()
