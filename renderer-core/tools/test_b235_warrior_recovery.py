#!/usr/bin/env python3
"""B235 based strictly on user's uploaded SHA d55cc478...; source contract only.
PTDE pixel and runtime are separate; gate removal only after exact preimage.
"""
from pathlib import Path
import json,re
root=Path("renderer-core")
runtime=(root/"src/runtime/material_resource_draw_runtime.cpp").read_text("utf8")
hdr=(root/"include/dsrrl/runtime/generated_armor_v13_base_exact.hpp").read_text("utf8")
report=json.loads((root/"data/census/armor_v13_20261023_report.json").read_text("utf8"))
mtd=json.loads((root/"data/census/dsr_mtd_identity_certified_supplement_v1.json").read_text("utf8"))
owner=(root/"src/runtime/material_owner_producer.cpp").read_text("utf8")
cache=(root/"src/runtime/material_response_draw_transaction.cpp").read_text("utf8")
cacheh=(root/"include/dsrrl/runtime/material_response_draw_transaction.hpp").read_text("utf8")
draw=Path("build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp").read_text("utf8")
assert report["v12_pairs"] == 528 and report["v12_triples"] == 557
assert report["missing_pairs_added"] == 34 and report["missing_triples_added"] == 1
assert "hd_m_9360_s" in report["missing_pair_names"] and "hd_m_9360_s" in report["missing_triple_names"]
assert len(mtd["records"]) == 57
cm=[x for x in mtd["records"] if x["semantic_name"]=="C_Metal[DSB].mtd"]
assert len(cm)==1 and cm[0]["raw_mtd_sha256"]=="ae2e8df867fe2859eef37104c13c939e7e7fe4f703b7fe49c408d0230fc71d85"
for token in ("armor_v13::pair_ok(h1,h0)","armor_v13::triple_ok(h0,h1,h2)","armor_v13::diffuse_name(logical_hash)","armor_v13::normal_name(logical_hash)"):
 assert token in runtime,token
for token in ("source_unique_bnd_override","Ps_Body[DSB].mtd","P_DullLeather[DSB]_Edge.mtd"):
 assert token in owner,token
for token in ("replacement_key_published_[key_index]","bank_index * k_replacement_key_receivers","key.store(false, std::memory_order_release)"):
 assert token in cache,token
assert "5u * k_replacement_key_receivers" in cacheh
assert "v296_partial_material" not in draw, "unproven cross-operator veto survived"
assert "latest_hook_source" in (root/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf8")
assert "exact_proven_owner_slot(query)" not in (root/"include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp").read_text("utf8")
print("B235 Warrior crossmaterial SOURCE PASS: 34+1 V13, 57 MTD, C_Metal, exact shader TLS, V296 veto removed, R43 retained")
print("Pixel=OPEN, runtime=OPEN; source/CI is not in-game proof.")
