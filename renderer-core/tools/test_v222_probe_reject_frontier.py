#!/usr/bin/env python3
"""V222 generated-source contract, anchored V221 replay, regression failure tests."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parent

def load(name):
    spec = importlib.util.spec_from_file_location(
        name, ROOT / (name + ".py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

v221 = load("apply_v221_exact_nonmetal_probe_context_retry")
v222 = load("apply_v222_probe_reject_frontier")

fixture = (
    '#include "dsrrl/runtime/pixel_srv_shadow.hpp"\n'
    'constexpr int k_effect_fail_probe = 1;\n'
    'void prepare() {\n'
    '    const bool env_resource_ready = true;\n'
    '    const bool k_native_probe_feed = env_resources_.prepare_native_dsr(context,0,false,prepared);\n'
    + v221.OLD
    + '\n effect_fail(k_effect_fail_probe);\n}\n'
)
materialized_v221 = v221.patch(fixture)
v222_result = v222.patch(materialized_v221)
assert v222.patch(v222_result) == v222_result
assert v222_result.count('[DSRRL V222 PROBE REJECT FRONTIER]') == 1
assert v222_result.count('[DSRRL V221 PROBE JOIN]') == 1
assert 'route_index == 160u' in v222_result
assert 'material_slot == 3u' in v222_result
assert 'route_index == 88u' in v222_result
assert 'material_slot == 2u' in v222_result
assert 'decision.receiver_id == 34u' in v222_result
assert 'sidecar_20261010::exact_nonmetal_draw(material, decision)' in v222_result
assert 'shadow_ready=%u' in v222_result
assert 'native_probe_known=%u' in v222_result
assert 'registered_native=%llu' in v222_result
assert 'pack_ready=%u sampler_ready=%u' in v222_result
assert 'env_resources_.release(native_probe)' in v222_result
assert 'original_probe_reject=1 swap=0 pixel=OPEN' in v222_result
assert v222_result.index('[DSRRL V222 PROBE REJECT FRONTIER]') < (
    v222_result.index('    if (!env_resource_ready) {'))
assert v222_result.count('    if (!env_resource_ready) {') == 1
assert v222_result.count('telemetry::hot_count(probe_rejects_);') == 1
assert v222_result.count('if (shader != nullptr)') == 1
assert 'latest_hook_source(' not in v222.REPLACE
try:
    v222.patch(fixture)
except RuntimeError:
    pass
else:
    raise AssertionError("V222 must reject source lacking V221")
try:
    v222.patch(materialized_v221.replace(
        '    if (!env_resource_ready) {\n        if (shader != nullptr)',
        '    if (unrecognized_probe_state) {\n        if (shader != nullptr)'))
except RuntimeError:
    pass
else:
    raise AssertionError("V222 must reject drifted fail-open")
print("V222_EXACT_PROBE_FRONTIER_ORACLE_PASS")
print("V222_NO_NEW_BRIDGE_AND_PMETAL_FALLBACK_PRESERVED")
