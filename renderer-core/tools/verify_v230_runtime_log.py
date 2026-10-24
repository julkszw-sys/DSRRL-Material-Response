#!/usr/bin/env python3
"""Read an actual ReShade log from an owner-run DSR session.

Separates construction, receiver-hit, resource activation, native Draw +
restore, fail-open, and PTDE pixels. Never marks pixel equivalence PASS.
Examples:
    python renderer-core/tools/verify_v230_runtime_log.py ReShade.log
    python renderer-core/tools/verify_v230_runtime_log.py --selftest
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

TAG = "[DSRRL V230 RUNTIME]"
NATIVE = "[DSRRL V230 NATIVE DRAW]"
SHADER = "[DSRRL PS PAYLOAD AUTHORITY]"
RX_RE = re.compile(r"\brx=(33|34|35)\b")
FAM_RE = re.compile(r"\bfamily=(STABLE|LERP)\b")
STAGE_RE = re.compile(r"\bstage=([a-z_]+)\b")
VALUE_RE = re.compile(r"\b([a-z][a-z0-9_]*)=([^\s]+)")



def get_rx34_sidecar_rows(log):
    rows = {}
    for line in log.splitlines():
        if "[DSRRL RX34 RESOURCE ID WITNESS]" not in line:
            continue
        v = dict(VALUE_RE.findall(line.split("WITNESS]", 1)[1]))
        slot, key = v.get("slot"), v.get("name")
        if slot in ("t0", "t1", "t2") and key:
            rows.setdefault((slot, key), v)
    return sorted(rows.values(), key=lambda r: (r["slot"], r["name"]))

def evaluate(log: str) -> dict:
    rows = []
    native_rows = []
    shader_rows = []
    for line in log.splitlines():
        if TAG in line:
            tail = line.split(TAG, 1)[1]
            v = dict(VALUE_RE.findall(tail))
            if v.get("rx") in ("33", "34", "35") and v.get("family") in ("STABLE", "LERP"):
                rows.append(v)
        if NATIVE in line:
            native_rows.append(dict(VALUE_RE.findall(line.split(NATIVE, 1)[1])))
        if SHADER in line:
            shader_rows.append(dict(VALUE_RE.findall(line.split(SHADER, 1)[1])))

    def slot(mask: str, index: int) -> bool:
        try:
            return bool(int(mask, 16) & (1 << index))
        except (ValueError, TypeError):
            return False

    detail = []
    for rx, fam in sorted({(v["rx"], v["family"]) for v in rows}):
        subset = [v for v in rows if v["rx"] == rx and v["family"] == fam]
        ready = [v for v in subset if v.get("stage") == "batch_ready"]
        valid = [
            v for v in ready
            if v.get("shader") == "1"
            and v.get("cb12") == "1"
            and v.get("envspec_ready") == "1"
            and v.get("envdiffuse_ready") == "1"
            and all(slot(v.get("srv_mask", ""), i) for i in ((11, 12, 13, 14) if fam == "LERP" else (11, 12, 14)))
            and all(slot(v.get("sampler_mask", ""), i) for i in ((11, 12, 13, 14) if fam == "LERP" else (11, 12, 14)))
        ]
        detail.append({
            "rx": int(rx),
            "family": fam,
            "stages": sorted({v.get("stage", "") for v in subset}),
            "carrier_and_transaction_request": "PASS" if valid else "OPEN",
            "draw_dispatch_observed": any(
                v.get("stage") in ("draw_issued", "indexed_issued") for v in subset
            ),
            "prepare_failopen_seen": any(v.get("stage") == "prepare_reject" for v in subset),
        })

    native_applied = any(
        v.get("stage") == "draw_applied_restored"
        and v.get("original_draw_reached") == "1"
        and v.get("restored") == "1"
        and v.get("envspec_owner") == "1"
        for v in native_rows
    )
    restore_fail = any(
        v.get("stage") in ("restore_failed_quarantine", "restore_failed")
        for v in native_rows + rows
    )
    def status(success: bool) -> str:
        return "OBSERVED" if success else "NOT_OBSERVED"

    return {
        "addon_boot_identity": status(
            "[DSRRL BUILD_ID]" in log or "v230_production" in log
        ),
        "shader_create_time": status(any(
            v.get("created") == "1" and v.get("rx") in ("33", "34", "35")
            for v in shader_rows
        )),
        "receiver_hit": status(bool(rows)),
        "complete_resource_request": status(any(
            v["carrier_and_transaction_request"] == "PASS" for v in detail
        )),
        "native_hook_draw_applied_and_restored": status(native_applied),
        "any_draw_dispatch_issued": status(
            any(v["draw_dispatch_observed"] for v in detail)
        ),
        "restore_failure": "OBSERVED_WARNING" if restore_fail else "NOT_OBSERVED",
        "runtime_result": (
            "RESTORE_FAULT" if restore_fail else
            "BRIDGE_ACTIVATION_OBSERVED" if native_applied
                or any(v["draw_dispatch_observed"] for v in detail)
            else "NO_BRIDGE_DRAW_PROOF"
        ),
        "ptde_pixel_behavior": "UNVERIFIED_REQUIRES_MATCHED_PTDE_AB",
        "scope_note": "Native post-draw EnvSpec owner and RX-level arm may not refer to the same draw; no RX-level correlation claimed.",
        "receivers": detail,
    }


def selftest() -> None:
    sample = """
[DSRRL BUILD_ID] v230_production
[DSRRL PS PAYLOAD AUTHORITY] family=stable rx=34 created=1
[DSRRL V230 RUNTIME] stage=batch_ready rx=34 route=345 family=STABLE shader=1 cb12=1 envspec_ready=1 envdiffuse_ready=1 spec_rgb=1 probe_a=12 probe_b=12 srv_mask=0x00005800 sampler_mask=0x00005800 pixel_pass=0
[DSRRL V230 RUNTIME] stage=native_armed rx=34 route=345 family=STABLE shader=1 cb12=1 envspec_ready=1 envdiffuse_ready=1 spec_rgb=1 probe_a=12 probe_b=12 srv_mask=0x00005800 sampler_mask=0x00005800 pixel_pass=0
[DSRRL V230 NATIVE DRAW] kind=draw_indexed stage=draw_applied_restored envspec_owner=1 ps_replace=1 srv_mask=0x00005800 original_draw_reached=1 restored=1 pixel_pass=0
"""
    result = evaluate(sample)
    assert result["runtime_result"] == "BRIDGE_ACTIVATION_OBSERVED"
    assert result["complete_resource_request"] == "OBSERVED"
    assert result["restore_failure"] == "NOT_OBSERVED"
    assert result["ptde_pixel_behavior"].startswith("UNVERIFIED")
    result = evaluate(sample + "\n[DSRRL V230 NATIVE DRAW] stage=restore_failed_quarantine")
    assert result["runtime_result"] == "RESTORE_FAULT"
    assert evaluate("")["runtime_result"] == "NO_BRIDGE_DRAW_PROOF"
    print("V230_RUNTIME_PARSER_SELFTEST_PASS synthetic_only_not_gameplay_evidence")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", nargs="?", type=Path)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    if args.selftest:
        selftest()
        return
    if args.log is None:
        parser.error("Provide a real ReShade.log or --selftest")
    print(json.dumps(
        evaluate(args.log.read_text("utf-8", errors="replace")),
        indent=2, sort_keys=True,
    ))


if __name__ == "__main__":
    main()
