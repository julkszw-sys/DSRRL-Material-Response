#!/usr/bin/env python3
"""v2.3.0: remove V357 rejected-BC6H logging counters only.

Unknown source handling remains exact SHA fail-open in the unchanged
operator path. Never disable V358/V359 lifetime/name attestation.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'src/runtime/envspec_resource_runtime.cpp'
s=p.read_text('utf-8')
for name in ('g_v357_missing_bc6h_init','g_v357_unknown_bc6h_census'):
 old='#if defined(DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS)\n        const auto n='+name+'.fetch_add('
 new='#if defined(DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS) && !defined(DSRRL_V230_PRODUCTION)\n        const auto n='+name+'.fetch_add('
 if s.count(old)!=1:raise RuntimeError('negative-only V357 counter source drift: '+name)
 s=s.replace(old,new,1)
p.write_text(s,'utf-8')
print('V230_NEGATIVE_BC6H_CENSUS_COUNTERS_COMPILED_OUT=2')
