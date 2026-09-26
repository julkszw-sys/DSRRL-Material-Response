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

    print(f"PointLight core source audit: PASS ({len(sources)} implementation units)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
