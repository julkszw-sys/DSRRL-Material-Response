#!/usr/bin/env python3
"""Negative source oracle: no changes to shader/resource or established V237."""
import importlib.util
from pathlib import Path
q=Path(__file__).with_name("apply_v239_endpoint_witness.py")
spec=importlib.util.spec_from_file_location("v239",q)
x=importlib.util.module_from_spec(spec)
spec.loader.exec_module(x)
root=Path(__file__).resolve().parents[2]
src=(root/x.PATH).read_text(encoding="utf-8")
assert src.count(x.OLD)==1 and src.count(x.NEW)==0
assert x.patch(src)==src
out=x.patch(src,True)
assert out==src.replace(x.OLD,x.NEW,1)
assert x.patch(out,True)==out and x.patch(out)==out
for bad in (src.replace(x.OLD,"bad"),src+x.OLD,src.replace("build_island_draw_mutation(","broken_mutation(")):
    try: x.patch(bad,True)
    except ValueError: pass
    else: raise AssertionError("V239 source drift was accepted")
assert "decision.receiver_id == 33u" in x.NEW
assert "env_semantics.envspc_slot < 3u" in x.NEW
assert "masks[3]" in x.NEW
assert "REQUEST_READY_NOT_PIXEL" in x.NEW
assert "source.beta * 16.0f" in x.NEW
assert "source.a[0]" in x.NEW and "source.b[0]" in x.NEW
assert "source.envdiffuse_a[0]" in x.NEW and "source.envdiffuse_b[0]" in x.NEW
assert "prepared.env_resources.probe_a" in x.NEW
print("V239_RX33_ENDPOINT_BETA_BUCKET_SOURCE_NEGATIVE_ORACLE_PASS")
