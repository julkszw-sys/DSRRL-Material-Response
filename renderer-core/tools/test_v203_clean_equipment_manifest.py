#!/usr/bin/env python3
"""Regression: equipment manifests are exact v203 name+FLVER+slot+MTD tuples."""
import re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
header=(ROOT/"renderer-core/include/dsrrl/runtime/v203_equipment_flver_slot_spec_manifest.hpp").read_text()
corpus=(ROOT/"renderer-core/data/provenance/spec_routes_v12.tsv").read_text()
rows=re.findall(r'\{ "([0-9a-f]{64})", (\d+)u, "([^"]+)", "([^"]+)" \},',header)
allowed={name.lower():int(h,16) for h,name in (
    line.split("\t")[:2] for line in corpus.splitlines()
    if re.match(r"^[0-9a-f]{16}\t",line)
)}
def hash_name(name):
    h=14695981039346656037
    for ch in name.lower():
        h=(h ^ ord(ch))*1099511628211 & 0xffffffffffffffff
    return h
assert len(rows)==36, (len(rows),"expected exact 36 rows")
assert len(set(rows))==36
assert len(set(x[0] for x in rows))==21
assert len(set(x[3].lower() for x in rows))==18
keys=set(rows)
for sha,slot,mtd,spec in rows:
    assert spec.lower() in allowed, spec
    assert hash_name(spec)==allowed[spec.lower()],spec
    assert re.match(r"^(?:bd|am|lg|hd|wp)_",spec.lower())
    assert mtd.endswith("[DSB].mtd")
    assert (sha,str(int(slot)+100),mtd,spec) not in keys
    assert ("f"*64,slot,mtd,spec) not in keys
    assert (sha,slot,mtd,"c5330_s") not in keys
assert "resolve_exact_equipment_native_ps_t1" not in header
assert "match_exact_c5330_native_ps_t1" not in header
print("V203_EQUIPMENT_MANIFEST_36_EXACT_ROWS_PASS owners=21 names=18")
print("V203_EQUIPMENT_NEGATIVE_SLOT_SHA_NPC_PASS")
