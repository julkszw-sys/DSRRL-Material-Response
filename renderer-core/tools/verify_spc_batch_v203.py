#!/usr/bin/env python3
"""Static census audit: all 25 exact SPC material profiles."""
import json
import pathlib
import re
ROOT = pathlib.Path(__file__).resolve().parents[1]
routes = json.loads((ROOT / "data/material_response_routes_v1.json").read_text())["routes"]
envs = json.loads((ROOT / "data/census/ptde_mtd_envspec_router_v1.json").read_text())["records"]
header = (ROOT / "include/dsrrl/runtime/ptde_metal_envspec_authority.hpp").read_text()
pattern = r'\{ptde_metal_envspec_profile::(\w+),\s*(\d+)u,\s*"([^"]+)",\s*"([0-9a-f]{64})",\s*([\d.]+)f,\s*(\d+)u\}'
compiled = re.findall(pattern, header)
assert len(compiled) == 25, len(compiled)

assert len({r[2] for r in compiled}) == len(compiled)
for profile, route, name, sha, gain, slot in compiled:
    matches = [x for x in routes if x["mtd_name"] == name]
    assert len(matches) == 1, name
    assert matches[0]["route_index"] == int(route), name
    assert matches[0]["sha256"] == sha, name
    assert float(matches[0]["c101"]) == float(gain), name
for profile, route, name, sha, gain, slot in compiled:
    assert len([e for e in envs if e["mtd_name"] == name]) == 1, name
    env = next(e for e in envs if e["mtd_name"] == name)
    assert env["dsr_mtd_sha256"] == sha, name
    assert env["envspc_slot"] == int(slot), name
    assert env["state"] == "PRESENT", name
assert all(x["material_family"] == "DifSpcBmp" for x in routes if any(x["mtd_name"] == z[2] for z in compiled))
print('Unique MTD and EnvSpec entries:',len(compiled))
