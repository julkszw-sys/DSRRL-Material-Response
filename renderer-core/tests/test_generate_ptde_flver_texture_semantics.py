#!/usr/bin/env python3
from __future__ import annotations

import csv
import hashlib
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


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def expect_system_exit(fn, needle: str) -> None:
    try:
        fn()
    except SystemExit as exc:
        assert needle in str(exc), str(exc)
    else:
        raise AssertionError(f"expected SystemExit containing {needle!r}")


def main() -> int:
    tool = load_tool()
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        tsv = root / "flver_material_ownership_input_v1.tsv"

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
        expect_system_exit(
            lambda: tool.load_records(tsv),
            "unknown PTDE texture_semantic",
        )

        # Provenance is fail-open: a generator invocation may only consume the
        # exact canonical TSV/error files named and hashed by one scan summary.
        write_rows(tsv, ["g_Diffuse"])
        errors = root / "flver_scan_errors_v1.jsonl"
        errors.write_text("", encoding="utf-8")
        summary = {
            "outputs": {
                "canonical_tsv": tsv.name,
                "errors_jsonl": errors.name,
            },
            "sha256": {
                tsv.name: digest(tsv),
                errors.name: digest(errors),
            },
        }
        tool.require_scan_member(summary, tsv, digest(tsv), "canonical_tsv")
        tool.require_scan_member(summary, errors, digest(errors), "errors_jsonl")

        bad_sha = dict(summary)
        bad_sha["sha256"] = dict(summary["sha256"])
        bad_sha["sha256"][tsv.name] = "0" * 64
        expect_system_exit(
            lambda: tool.require_scan_member(bad_sha, tsv, digest(tsv), "canonical_tsv"),
            "scan provenance SHA mismatch",
        )

        renamed = root / "other.tsv"
        renamed.write_bytes(tsv.read_bytes())
        expect_system_exit(
            lambda: tool.require_scan_member(summary, renamed, digest(renamed), "canonical_tsv"),
            "scan provenance filename mismatch",
        )

        expect_system_exit(
            lambda: tool.require_scan_member({}, tsv, digest(tsv), "canonical_tsv"),
            "scan summary lacks output provenance",
        )

    print("generate_ptde_flver_texture_semantics: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
