#!/usr/bin/env python3
from __future__ import annotations

import csv
import importlib.util
import tempfile
from pathlib import Path


def load_tool():
    repo = Path(__file__).resolve().parents[2]
    path = repo / "renderer-core/tools/generate_ptde_flver_texture_semantics.py"
    spec = importlib.util.spec_from_file_location("ptde_semantics", path)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def write_rows(path: Path, semantics: list[str]) -> None:
    fields = [
        "game", "flver_identity", "material_slot", "mtd_name",
        "mtd_sha256", "texture_semantic",
    ]
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, delimiter="\t", lineterminator="\n")
        w.writeheader()
        for semantic in semantics:
            w.writerow({
                "game": "PTDE",
                "flver_identity": "fixture.flver",
                "material_slot": "0",
                "mtd_name": "P_Test.mtd",
                "mtd_sha256": "1" * 64,
                "texture_semantic": semantic,
            })


def main() -> int:
    tool = load_tool()
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        tsv = root / "ownership.tsv"

        write_rows(tsv, ["g_Diffuse", "g_Specular"])
        records, slots = tool.load_records(tsv)
        assert slots == 1
        assert len(records) == 1
        assert records[0]["positive_mask"] == (
            tool.BITS["g_Diffuse"] | tool.BITS["g_Specular"]
        )
        assert records[0]["semantics"] == ["g_Diffuse", "g_Specular"]

        # Empty semantic is allowed as an observed material slot with no
        # positive texture-semantic evidence; absence remains UNKNOWN.
        write_rows(tsv, [""])
        records, slots = tool.load_records(tsv)
        assert slots == 1
        assert records[0]["positive_mask"] == 0
        assert records[0]["semantics"] == []

        # A non-empty semantic outside the serialized ABI must never be
        # silently discarded from positive_mask.
        write_rows(tsv, ["g_Diffuse", "g_FutureSemantic"])
        try:
            tool.load_records(tsv)
        except SystemExit as exc:
            message = str(exc)
            assert "unknown PTDE texture_semantic" in message
            assert "g_FutureSemantic" in message
        else:
            raise AssertionError("unknown semantic did not fail open")

    print("generate_ptde_flver_texture_semantics: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
