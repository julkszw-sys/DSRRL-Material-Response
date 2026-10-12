#!/usr/bin/env python3
"""V229 post-prepare read-only material and shader cut source oracle."""
import importlib.util
from pathlib import Path

path=Path(__file__).with_name("apply_v229_gloss_material_consumer_witness.py")
spec=importlib.util.spec_from_file_location("v229",path)
v=importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)
sample=(
    "bool exact_pmetal_material(material) {}\n"
    "prepare_exact_native_resource_join_v224\n"
    "if (family == pmetal_envspec_receiver_family::hemenvlerp) {}\n"
    +v.ANCHOR+"\n"
)
out=v.patch(sample)
assert v.patch(out)==out
assert out.count(v.ANCHOR)==1
assert out.count("[DSRRL V229 GLOSS CUT]")==1
assert "class=%s family=%s" in out
assert "class=NONMETAL" not in out  # computed at runtime
assert "v229_pmetal ? 0u : 2u" in out
assert "v229_lerp ? 1u : 0u" in out
assert "decision.ptde_specular_power" in out
assert "decision.ptde_specular_power_verified" in out
assert "env_semantics.envspc_slot" in out
assert "prepared.env_resources.ptde_a" in out
assert "prepared.env_resources.ptde_b" in out
assert "prepared.envdiffuse_resources.ptde_a" in out
assert "prepared.envdiffuse_resources.ptde_b" in out
assert "prepared.material_resources.spec_rgb" in out
assert "prepared.material_resources.diffuse" in out
assert "prepared.material_resources.normal" in out
assert "prepared.shader" in out
assert "prepared.b12" in out
assert "t10_count" in out and "t1_fmt" in out
assert "stage=PREPARED_ONLY pixel=OPEN" in out
assert "context->PSGetShaderResources(" in out
assert "v229_stock_t1->Release()" in out
assert "std::array<std::atomic<std::uint64_t>,24>,4" in out
assert "#if defined(DSRRL_V229_GLOSS_MATERIAL_CONSUMER_WITNESS)" in out
# No shader, CB, pixel, normal, diffuse or specular bind changes in V229.
for forbidden in ("prepared.request.srvs[","prepared.request.constant_buffers[",
                  "source.beta =","decision.c101 =","shader->Release()"):
    assert forbidden not in v.INSERT, forbidden
for src in [
    sample.replace("exact_pmetal_material(material)","foo"),
    sample.replace("prepare_exact_native_resource_join_v224","missing"),
    sample.replace(v.ANCHOR,"BROKEN SOURCE"),
    sample+v.ANCHOR,
]:
    try: v.patch(src)
    except ValueError: pass
    else: raise AssertionError("V229 drift must fail")
print("V229_MATERIAL_GLOSS_PREPARED_CUT_SOURCE_ORACLE_PASS")
print("V229_STABLE_LERP_PMETAL_NONMETAL_UNCHANGED_PIXEL_OPEN")
