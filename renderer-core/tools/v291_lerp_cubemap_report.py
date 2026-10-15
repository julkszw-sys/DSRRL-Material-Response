#!/usr/bin/env python3
"""Bounded actual Lerp cubemap A/B and beta sampling audit from V291 owner log.

The shader's operator is not measured by log: GPU sample and PTDE pixels OPEN.
Only an *issued* event can witness an in-game prepared draw.
"""
import argparse
import csv
import json
import re
from collections import Counter
from pathlib import Path

MARKER = "[DSRRL V291 LERP CUBEMAP]"
KV = re.compile(r"(\w+)=([^\s]+)")
ISSUED = {"indexed_issued", "draw_issued"}
ARMED = {"native_armed"}
COLUMNS = ("log", "stage", "rx", "route", "slot", "mtd", "flver",
           "beta", "beta_band", "serial", "spec_a", "spec_b",
           "diffuse_a", "diffuse_b", "pair_join", "t11", "t13",
           "t12", "t14", "s_envspec", "s_envdiffuse", "b12")
def parse(logs):
    rows = []
    for path in logs:
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            if MARKER not in line:
                continue
            data = dict(KV.findall(line.split(MARKER,1)[1]))
            row = {key:data.get(key,"") for key in COLUMNS}
            row["log"]=path.name
            try:
                beta = float(row["beta"])
            except ValueError:
                beta = float("nan")
            row["is_issued"] = int(row["stage"] in ISSUED)
            row["native_armed"] = int(row["stage"] in ARMED)
            row["nonzero_beta"] = int(0.0 < beta < 1.0)
            row["distinct_probe_pair"] = int(row["spec_a"] != row["spec_b"])
            row["resource_join_complete"] = int(
                row["pair_join"]=="1" and all(
                    row[k] and int(row[k],16)!=0
                    for k in ("t11","t13","t12","t14")))
            rows.append(row)
    return rows
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--log", action="append", type=Path, required=True)
    ap.add_argument("--output-dir", type=Path, required=True)
    args=ap.parse_args()
    rows=parse(args.log)
    args.output_dir.mkdir(parents=True,exist_ok=True)
    with (args.output_dir/"lerp_cube_draws.csv").open("w",newline="",encoding="utf-8") as f:
        writer=csv.DictWriter(f,fieldnames=[*COLUMNS,
            "is_issued","native_armed","nonzero_beta","distinct_probe_pair","resource_join_complete"])
        writer.writeheader()
        writer.writerows(rows)
    issued=[r for r in rows if r["is_issued"]]
    summary={
        "events":len(rows),
        "issued_observations":len(issued),
        "native_armed_observations":sum(r["native_armed"] for r in rows),
        "nonzero_beta_issued":sum(r["nonzero_beta"] for r in issued),
        "distinct_pair_nonzero_beta_issued":sum(
            r["nonzero_beta"] and r["distinct_probe_pair"] for r in issued),
        "resource_join_issued":sum(r["resource_join_complete"] for r in issued),
        "beta_bands_issued":dict(Counter(r["beta_band"] for r in issued)),
        "scope":"BOUNDED_FIRST_TUPLE_BETA_BAND_OBSERVATIONS_ONLY",
        "shader_math_confirmed_by_gameplay_log":False,
        "ptde_pixel_equivalence":"OPEN",
    }
    (args.output_dir/"summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(summary,sort_keys=True))
if __name__=="__main__":main()
