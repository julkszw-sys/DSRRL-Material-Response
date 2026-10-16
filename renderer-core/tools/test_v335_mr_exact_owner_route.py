#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
hdr=(root/"include/dsrrl/runtime/v335_mr_exact_owner_route.hpp").read_text()
addon=(root/"integrated/integrated_addon.cpp").read_text()
build=(root/"tools/build_v335_exact_mr_route_one_addon.ps1").read_text()
assert "dsr_flver_owner_tuple_authenticated" in hdr
assert "matches_hex(" in hdr
assert "route.route_index == 0u" in hdr
assert "material.owner_tuple_exact" in hdr
assert "material.material_slot_valid" in hdr
assert "material.raw_mtd_sha256" in hdr
assert "material.route_index = match->route_index" in hdr
assert "v335_recover_exact_mr_route(" in addon
assert "stage=exact_route_recovered" in addon
assert "V335_MR_EXACT_OWNER_ROUTE" in build
assert "V333_WANDERER_SLOT9_SCHLICK_WITNESS" not in build
print("V335 source guard PASS: exact owner and raw SHA, no Schlick patch")
