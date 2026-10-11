#!/usr/bin/env python3
"""V221 generator oracle: only exact nonmetal, SHA-authenticated probe lookup."""
import importlib.util
from pathlib import Path

p = Path(__file__).with_name(
    "apply_v221_exact_nonmetal_probe_context_retry.py")
spec = importlib.util.spec_from_file_location("v221", p)
overlay = importlib.util.module_from_spec(spec)
spec.loader.exec_module(overlay)

fixture = (
    '#include "dsrrl/runtime/pixel_srv_shadow.hpp"\n'
    'constexpr int k_effect_fail_probe = 1;\n'
    'void prepare() {\n'
    + '    const bool env_resource_ready = true;\n'
    + overlay.OLD
    + '\n effect_fail(k_effect_fail_probe);\n }\n'
)
patched = overlay.patch(fixture)
assert overlay.patch(patched) == patched
assert patched.count('sidecar_20261010::exact_nonmetal_draw(material,decision)') == 1
assert '!k_native_dsr_cubemap_feed' in patched
assert '    bool env_resource_ready = true;' in patched
assert 'shadow_env_ready' in patched
assert 'PSGetShaderResources(' in patched
assert 'env_resources_.prepare_bound(' in patched
assert 'if (!env_resource_ready)' in patched
assert '[DSRRL V221 PROBE JOIN]' in patched
assert 'if (shader != nullptr)' in patched
assert 'effect_fail(k_effect_fail_probe);' in patched
assert patched.index('if (!env_resource_ready) {') > patched.index(
    '[DSRRL V221 PROBE JOIN]')
assert 'route_index == 345u' not in patched, (
    "P_Metal exclusion is via the exact_nonmetal_material gate")
try:
    overlay.patch(fixture.replace(
        '    const bool env_resource_ready = true;',
        '    auto probe_ready = true;'))
except RuntimeError:
    pass
else:
    raise AssertionError("V221 must fail closed on drift")
print("V221_PROBE_CONTEXT_ORACLE_PASS exact_owner_only=1")
print("V221_PMETAL_ROUTE_UNTOUCHED_AND_PROBE_SHA_AUTHORITY_RETAINED")
