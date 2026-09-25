#!/usr/bin/env python3
import json
import pathlib

root = pathlib.Path(__file__).resolve().parents[1]
p = root / "data/census/ptde_mtd_resource_ownership_join_contract_v1.json"
d = json.loads(p.read_text(encoding="utf-8"))
assert d["schema"] == 2
assert d["runtime_activation"] == "OPEN"
assert d["pixel_equivalence"] == "OPEN"
assert d["current_state"]["diffuse_resource_routes"]["rows"] == 528
assert d["current_state"]["normal_bump_resource_routes"]["rows"] == 557
assert d["current_state"]["diffuse_resource_routes"]["mtd_owned_rows"] == "PENDING_EXACT_ROW_JOIN"
assert d["current_state"]["normal_bump_resource_routes"]["mtd_owned_rows"] == "PENDING_EXACT_ROW_JOIN"
assert "never implies NO_USE" in d["absence_rule"]
assert "raw MTD SHA256" in d["required_join_key"]
assert "exact material-slot index or equivalent stable slot identity" in d["required_join_key"]
a=d["artifact943_recovered_facts"]
assert a["ordinary_sidecar_ownership"] == "552/552"
assert a["bridge_mtd_names"] == 27
assert a["confirmed_mtd_routes"] == 17
assert a["open_mtd_routes"] == 10
assert a["parser_errors"] == 0
n=d["normal_independent_static_evidence"]
assert n["safe_tuple_classes"] == 599
assert n["homologous_material_slot_rows"] == 3213
assert n["homologous_material_slot_rows_total"] == 4449
assert n["mtd_promotion"].startswith("BLOCKED")
assert "aggregate artifact943 counts" in " ".join(d["forbidden"])
print("ptde_mtd_resource_ownership_join_contract_v1: PASS")
