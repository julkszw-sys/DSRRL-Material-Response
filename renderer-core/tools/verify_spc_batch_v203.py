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
