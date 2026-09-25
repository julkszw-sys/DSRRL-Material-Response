#!/usr/bin/env python3
"""Verify DSRRL PTDE/DSR MTD semantic census invariants.

Static/construction verifier only. It intentionally does not claim runtime
activation or pixel equivalence.
"""
from __future__ import annotations
import json, pathlib, re, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
C=ROOT/'data/census/ptde_mtd_semantic_census_v1.json'
B=ROOT/'data/census/ptde_mtd_semantic_exact_bindings_v1.json'
S=ROOT/'data/census/ptde_subsurface_usage_census_v1.json'
VALID={'USE','NO_USE','UNKNOWN'}
SHA=re.compile(r'^[0-9a-f]{64}$')

def load(p): return json.loads(p.read_text(encoding='utf-8'))
def die(s): raise SystemExit('FAIL: '+s)

def check_operator_map(m, where):
    for op,v in m.items():
        state=v.get('state') if isinstance(v,dict) else v
        if state not in VALID: die(f'{where}.{op}: invalid state {state!r}')
        if state=='NO_USE':
            basis=v.get('basis','') if isinstance(v,dict) else ''
            if not basis: die(f'{where}.{op}: NO_USE requires explicit evidence basis')

def main():
    c,b,s=map(load,(C,B,S))
    if c.get('states')!=['USE','NO_USE','UNKNOWN']: die('canonical state ordering changed')
    if c.get('runtime_activation')!='OPEN' or c.get('pixel_equivalence')!='OPEN': die('static census must not promote runtime/pixel status')
    cols=set(c['operator_columns'])
    required={'material_response','spec_rgb','env_spec','subsurface','diffuse','normal_bump','upper_lower','hemenv','hemenv_lerp','pointlight','alpha_blend','parallax','emissive_lightmap','texture_resource_consumers'}
    if cols!=required: die('operator column set mismatch')
    for i,x in enumerate(c.get('exact_overrides',[])):
        if not SHA.fullmatch(x.get('ptde_mtd_sha256','')): die(f'override {i}: invalid raw SHA')
        check_operator_map(x.get('operators',{}),f'override[{i}]')
    seen=set()
    for i,r in enumerate(b.get('records',[])):
        ident=(r.get('mtd_name'),r.get('raw_sha256'))
        if not ident[0] or not SHA.fullmatch(ident[1] or ''): die(f'binding {i}: incomplete exact identity')
        if ident in seen: die(f'binding {i}: duplicate exact identity')
        seen.add(ident)
        # legacy compact strings are allowed, but NO_USE must be route-scoped by file semantics.
        for op,state in r.get('operators',{}).items():
            if state not in VALID: die(f'binding {i}.{op}: invalid state')
        if r.get('operators',{}).get('pointlight')=='NO_USE' and r.get('receiver_scope')!=[33,34,35]:
            die(f'binding {i}: PointLight NO_USE escaped certified no-PointLight receiver scope')
    if s.get('runtime_activation') not in ('OPEN','NOT_TESTED',None): die('subsurface census overclaims runtime')
    print(json.dumps({'status':'PASS','exact_binding_records':len(seen),'exact_overrides':len(c.get('exact_overrides',[])),'operator_columns':len(cols),'runtime_activation':'OPEN','pixel_equivalence':'OPEN'},sort_keys=True))
if __name__=='__main__': main()
