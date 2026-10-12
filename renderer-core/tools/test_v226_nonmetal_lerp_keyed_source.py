#!/usr/bin/env python3
"""V226 regression: exact keyed Lerp only, stock fail-open and P_Metal unchanged."""
import importlib.util
from pathlib import Path
path=Path(__file__).with_name("apply_v226_nonmetal_lerp_keyed_source.py")
spec=importlib.util.spec_from_file_location("v226",path)
v=importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)

head="class pmetal_env_source_runtime {\n"+v.HEADER_ANCHOR+"\n};\n"
source=(
    "v220_exact_nonmetal_source_key\n"
    "[DSRRL V223 SHARED LIGHTBANK]\n"
    "if (latest_hook_source(out)) {\n"
    "pmetal_producer_state_latest(key, epoch, out);\n"
    +v.SOURCE_ANCHOR+"\n"
)
draw=(
    "prepare_exact_native_resource_join_v224\n"
    +v.DRAW_ANCHOR+"\n"
)
a,b,c=v.patch(head,source,draw)
assert v.patch(a,b,c)==(a,b,c)
assert "latest_keyed_nonmetal_lerp" in a
assert "v220_exact_nonmetal_source_key(material), epoch, out" in b
assert "out.beta < 0.0f || out.beta > 1.0f" in b
assert "out.bank_signature_a == 0u" in b
assert "out.bank_signature_b == 0u" in b
assert "g_selector_epoch.load(std::memory_order_acquire) != epoch" in b
assert "source_.latest(material, source)" in c
assert "family == pmetal_envspec_receiver_family::hemenvlerp" in c
assert "sidecar_20261010::exact_nonmetal_material(material)" in c
assert "V226 NONMETAL LERP SOURCE" in c
assert "stock_dsr_fail_open" in c
assert "if (latest_hook_source(out)) {" in b
assert "env_resource_ready = " not in v.DRAW_NEW
for mutated in (
    (head.replace(v.HEADER_ANCHOR,"none"),source,draw),
    (head,source.replace("v220_exact_nonmetal_source_key","missing"),draw),
    (head,source.replace("[DSRRL V223 SHARED LIGHTBANK]","missing"),draw),
    (head,source,draw.replace("prepare_exact_native_resource_join_v224","missing")),
):
    try: v.patch(*mutated)
    except ValueError: pass
    else: raise AssertionError("drift admitted")
print("V226_NONMETAL_LERP_KEYED_ONLY_SOURCE_ORACLE_PASS")
print("V226_PMETAL_STABLE_SOURCE_AND_SHADER_GUARDS_PASS")
