#!/usr/bin/env python3
import json
import pathlib

root = pathlib.Path(__file__).resolve().parents[1]
p = root / "data/census/flver_mtd_semantic_ownership_v1.json"
d = json.loads(p.read_text(encoding="utf-8"))
assert d["schema"] == 1
assert d["runtime_activation"] == "OPEN"
assert d["pixel_equivalence"] == "OPEN"
assert d["identity_model"]["primary_key"] == ["game/version", "exact FLVER identity", "material_slot", "semantic MTD identity", "raw MTD SHA256"]
a = d["artifact943_static_coverage"]
assert a["partsbnd"] == 1245
assert a["flver"] == 1277
assert a["declared_material_texture_slots"] == 19003
assert a["primary_g_specular_bindings"] == 4705
assert a["ordinary_sidecar_ownership"] == "552/552"
assert a["bridge_mtd_names"] == 27
assert a["confirmed_mtd_routes"] == 17
assert a["open_mtd_routes"] == 10
assert a["parser_errors"] == 0
assert d["row_schema"]["default_operator_state"] == "UNKNOWN"
assert d["current_materialization"]["row_payload"] == "PENDING_REPRODUCIBLE_EXPORT_OF_ARTIFACT_943"
for required in ("cross-slot propagation inside one FLVER", "NO_USE from corpus absence"):
    assert required in d["forbidden"]
print("flver_mtd_semantic_ownership_v1: PASS")
