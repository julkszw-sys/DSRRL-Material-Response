#!/usr/bin/env python3
from __future__ import annotations

import csv
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from test_dsr_flver_zip_ownership_census import build_fixture


def run_export(
    root:Path,
    out:Path,
    unresolved:Path,
    summary:Path,
)->subprocess.CompletedProcess[str]:
    repo=Path(__file__).resolve().parents[2]
    tool=repo/"renderer-core/tools/dsr_full_flver_ownership_export.py"
    return subprocess.run(
        [
            sys.executable,
            str(tool),
            str(root),
            "--out",str(out),
            "--unresolved",str(unresolved),
            "--summary",str(summary),
        ],
        check=False,
        text=True,
        capture_output=True,
    )


def main()->int:
    with tempfile.TemporaryDirectory() as td:
        root=Path(td)
        (root/"parts").mkdir()
        (root/"mtd/a").mkdir(parents=True)
        (root/"mtd/b").mkdir(parents=True)

        (root/"parts/HD_A_0000.flver").write_bytes(build_fixture())
        mtd_bytes=b"exact P_Metal fixture bytes"
        # Duplicate paths with identical content are one exact MTD identity,
        # not an ambiguity.
        (root/"mtd/a/P_Metal[DSB].mtd").write_bytes(mtd_bytes)
        (root/"mtd/b/P_Metal[DSB].mtd").write_bytes(mtd_bytes)

        out=root/"ownership.tsv"
        unresolved=root/"unresolved.tsv"
        summary=root/"summary.json"
        proc=run_export(root,out,unresolved,summary)
        assert proc.returncode==0, (proc.stdout,proc.stderr)

        with out.open(encoding="utf-8",newline="") as f:
            rows=list(csv.DictReader(f,delimiter="\t"))
        assert len(rows)==2
        assert [r["texture_semantic"] for r in rows]==[
            "g_Diffuse",
            "g_Specular",
        ]
        expected_mtd=hashlib.sha256(mtd_bytes).hexdigest()
        assert all(r["mtd_sha256"]==expected_mtd for r in rows)
        assert all(r["mtd_name"]=="P_Metal[DSB].mtd" for r in rows)

        state=json.loads(summary.read_text(encoding="utf-8"))
        assert state["source_complete"] is True
        assert state["counts"]["flver_files"]==1
        assert state["counts"]["flver_parse_ok"]==1
        assert state["counts"]["material_instances"]==1
        assert state["counts"]["texture_bindings"]==2
        assert state["counts"]["unresolved_rows"]==0

        # A same-basename MTD with different bytes is genuinely ambiguous.
        # The exporter must fail open instead of choosing one by path order.
        (root/"mtd/b/P_Metal[DSB].mtd").write_bytes(b"different MTD bytes")
        proc=run_export(root,out,unresolved,summary)
        assert proc.returncode==2
        state=json.loads(summary.read_text(encoding="utf-8"))
        assert state["source_complete"] is False
        assert state["counts"]["unresolved_rows"]>=1
        text=unresolved.read_text(encoding="utf-8")
        assert "distinct_hashes=2" in text
        assert "matches=2" in text

    print("dsr_full_flver_ownership_export: PASS")
    return 0


if __name__=="__main__":
    raise SystemExit(main())
