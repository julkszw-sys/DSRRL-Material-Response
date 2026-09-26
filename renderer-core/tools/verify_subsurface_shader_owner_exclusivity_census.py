#!/usr/bin/env python3
import json
from pathlib import Path

p = Path(__file__).resolve().parents[1] / "data" / "census" / "subsurface_shader_owner_exclusivity_census_v1.json"
d = json.loads(p.read_text(encoding="utf-8"))

assert d["schema"] >= 2
assert d["island"] == "bridge.subsurface"
assert d["runtime_activation"] == "OPEN"
assert d["pixel_equivalence"] == "OPEN"
assert len(d["source_shader_sha256"]) == 3
assert len(set(d["source_shader_sha256"])) == 3
assert d["binder_sha_alias_uniqueness"]["status"] == "CLOSED"
assert d["ownership_result"] == "NOT_SOURCE_COMPLETE"
assert d["authorized_material"]["name"] == "Ps_Body[DSBT].mtd"
assert d["authorized_material"]["owner_tuple_corpus_rows"] == 10
assert d["authorized_material"]["observed_material_slots"] == [0]
assert d["carrier_policy"]["global_create_time_substitution"] == "FAIL_OPEN_UNLESS_SHADER_OWNER_EXCLUSIVITY_IS_PROVEN"
assert "all-DSR-MTD" in d["remaining_missing_edge"]
assert "SPX" in d["remaining_missing_edge"]
assert "feature-key/receiver selection" in d["remaining_missing_edge"]
assert "all FLVER material-slot owners" in d["remaining_missing_edge"]

receiver_shas = {r["sha256"] for r in d["source_receivers"]}
assert receiver_shas == set(d["source_shader_sha256"])

print("subsurface shader-owner exclusivity census boundary: PASS (fail-open; reverse selector join still required)")
