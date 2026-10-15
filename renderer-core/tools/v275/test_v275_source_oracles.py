#!/usr/bin/env python3
"""V275 offline negative/positive source injection tests, no D3D or game."""
from pathlib import Path
from apply_v275_material_draw_trace import transform,ANCHOR,REPL
ROOT=Path("renderer-core")
p=ROOT/"integrated/integrated_addon.cpp"
source=p.read_text(encoding="utf8").replace("\r\n","\n")
original='#include "dsrrl/runtime/pixel_srv_shadow.hpp"\n'
assert source.count(original)==1
pre=source.replace(original,original+ANCHOR,1)
patched=transform(pre)
assert patched.count("dsrrl::runtime::v275::observe(")==8
assert patched.count("BATCH_PREPARED")==2
assert patched.count("PREPARE_FAIL")==2
assert patched.count("DISPATCH_FAIL_OPEN")==2
assert REPL in patched
assert patched.count("if (k_pmetal_native_draw_runtime_enabled &&")==2
try:
    transform(pre.replace(ANCHOR,"",1))
    raise AssertionError("Expected stale anchor fail closed")
except AssertionError:
    pass
try:
    transform(pre.replace("bool on_draw_indexed(\n","bool on_draw_indexed_X(\n"))
    raise AssertionError("Expected missing indexed draw anchor fail closed")
except ValueError:
    pass
assert source==p.read_text(encoding="utf8").replace("\r\n","\n")
print("V275_TRANSIENT_EXACT_DRAW_HOOK_POSITIVE_AND_NEGATIVE_PREIMAGES_PASS")
print("V275_PRODUCTION_SOURCE_UNCHANGED")
