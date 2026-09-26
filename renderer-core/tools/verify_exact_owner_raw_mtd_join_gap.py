#!/usr/bin/env python3
import json
import re
from pathlib import Path

root=Path(__file__).resolve().parents[1]
d=json.loads((root/'data/census/exact_owner_raw_mtd_join_gap_v1.json').read_text(encoding='utf-8'))
assert d['schema']==1
assert d['claim_scope']=='CONSTRUCTION_ONLY'
assert d['runtime_activation']=='NOT_TESTED'
assert d['pixel_equivalence']=='OPEN'
f=d['facts']
assert f['owner_tuple_source_complete'] is True
assert f['owner_flver_groups']==3944
assert f['owner_material_tuples']==19985
assert f['current_raw_mtd_enrichment_source']=='generated_dsr_mtd_identity_v1'
assert f['raw_mtd_identity_source_complete'] is False
assert f['envspec_router_records']==325
assert 'UNKNOWN/preserve-host' in d['fail_open']
assert any('basename hash alone' in x for x in d['forbidden'])

registry=(root/'include/dsrrl/operators/material_response/generated_dsr_mtd_identity_v1.hpp').read_text(encoding='utf-8')
m=re.search(r'k_dsr_mtd_identity_source_complete\s*=\s*(true|false)', registry)
assert m and m.group(1)=='false'
m=re.search(r'array<dsr_mtd_identity_record,(\d+)u>\s+k_dsr_mtd_identity_registry', registry)
assert m
assert int(m.group(1))==f['raw_mtd_identity_registry_records']

producer=(root/'src/runtime/material_owner_producer.cpp').read_text(encoding='utf-8')
assert '#include "dsrrl/operators/material_response/generated_dsr_mtd_identity_v1.hpp"' in producer
assert 'dsr_mtd_identity_resolve(semantic_hash, raw_mtd_sha)' in producer
assert 'generated_envspec_router_v1.hpp' not in producer
assert 'k_envspec_router_v1' not in producer
print(f"exact_owner_raw_mtd_join_gap_v1: PASS; dedicated registry partial ({f['raw_mtd_identity_registry_records']} exact records), misses fail open")
