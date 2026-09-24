#!/usr/bin/env python3
import json
import pathlib

root = pathlib.Path(__file__).resolve().parents[1]
p = root / "data/census/ptde_mtd_resource_ownership_join_contract_v1.json"
d = json.loads(p.read_text(encoding="utf-8"))
assert d["schema"] == 1
assert d["runtime_activation"] == "OPEN"
assert d["pixel_equivalence"] == "OPEN"
assert d["current_state"]["diffuse_resource_routes"]["rows"] == 528
assert d["current_state"]["normal_bump_resource_routes"]["rows"] == 557
assert d["current_state"]["diffuse_resource_routes"]["mtd_owned_rows"] == "PENDING_JOIN"
assert d["current_state"]["normal_bump_resource_routes"]["mtd_owned_rows"] == "PENDING_JOIN"
assert "never implies NO_USE" in d["absence_rule"]
assert "raw MTD SHA256" in d["required_join_key"]
assert "exact material-slot index or equivalent stable slot identity" in d["required_join_key"]
assert d["fail_open"].endswith("ownership is proven.")
print("ptde_mtd_resource_ownership_join_contract_v1: PASS")
