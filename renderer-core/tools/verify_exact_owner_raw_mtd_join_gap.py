#!/usr/bin/env python3
import json
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
assert f['current_raw_mtd_enrichment_source']=='generated_envspec_router_v1'
assert f['envspec_router_records']==325
assert 'UNKNOWN/preserve-host' in d['fail_open']
assert any('basename hash alone' in x for x in d['forbidden'])
producer=(root/'src/runtime/material_owner_producer.cpp').read_text(encoding='utf-8')
assert '#include "dsrrl/operators/material_response/generated_envspec_router_v1.hpp"' in producer
assert 'for (const auto &candidate : mr::generated::k_envspec_router_v1)' in producer
print('exact_owner_raw_mtd_join_gap_v1: PASS boundary documented; generic identity table still required')
