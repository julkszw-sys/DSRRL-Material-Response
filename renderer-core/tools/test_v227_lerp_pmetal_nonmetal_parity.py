#!/usr/bin/env python3
"""V227 bounded parity observer source contract / negative gates."""
import importlib.util
from pathlib import Path
p=Path(__file__).with_name("apply_v227_lerp_pmetal_nonmetal_parity.py")
spec=importlib.util.spec_from_file_location("v227",p)
v=importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)

sample=(
    "bool exact_pmetal_material(material) {}\n"
    "prepare_exact_native_resource_join_v224\n"
    '"[DSRRL V224 NATIVE RESOURCE JOIN]"\n'
    +v.ANCHOR+"\n"
)
patched=v.patch(sample)
assert v.patch(patched)==patched
assert patched.count("[DSRRL V227 LERP PARITY]")==1
assert patched.count("if (!env_resource_ready) {")==1
assert patched.count("shader->Release();")==1
assert patched.count("telemetry::hot_count(probe_rejects_)")==1
assert "source_unchanged=1 bridge_unchanged=1" in patched
assert "count < 8u" in patched
assert "family == pmetal_envspec_receiver_family::hemenvlerp" in patched
assert "exact_pmetal_material(material)" in patched
assert "sidecar_20261010::exact_nonmetal_draw(material, decision)" in patched
assert "context->PSGetShaderResources(12u, 3u, live)" in patched
assert "probe_b_required ? 1u : 0u" in patched
assert "src_generation" in patched and "bank_b" in patched
assert "ptde_probe_a" in patched and "ptde_probe_b" in patched
assert "GetResource(&res_a)" in patched and "GetResource(&res_b)" in patched
assert "res_a->Release()" in patched and "res_b->Release()" in patched
for broken in [
    sample.replace("prepare_exact_native_resource_join_v224","unknown"),
    sample.replace("exact_pmetal_material(material)","unknown"),
    sample.replace(v.ANCHOR,"unrecognized operator"),
]:
    try: v.patch(broken)
    except RuntimeError: pass
    else: raise AssertionError("Unexpected V227 source mutation allowed")
print("V227_READONLY_BOUNDED_LERP_PARITY_SOURCE_ORACLE_PASS")
print("V227_V223_SHARED_SOURCE_V224_EXACT_PROBE_AND_PMETAL_PRESERVED")
