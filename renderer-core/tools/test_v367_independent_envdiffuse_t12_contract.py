#!/usr/bin/env python3
"""V367 isolated EnvDiffuse source/transaction contract (no runtime PASS)."""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
h=(root/'include/dsrrl/runtime/pmetal_envspec_draw_runtime.hpp').read_text('utf8')
cpp=(root/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text('utf8')
resources=(root/'src/runtime/envspec_resource_runtime.cpp').read_text('utf8')
integrated=(root/'integrated/integrated_addon.cpp').read_text('utf8')
cmake=(root/'integrated/CMakeLists.txt').read_text('utf8')
policy=(root/'include/dsrrl/core/draw_transaction_policy.hpp').read_text('utf8')
cleanup=(root/'tools/materialize_v230_release_cleanup.py').read_text('utf8')
island=cpp.split('prepare_classic_envdiffuse_only(',1)[1].split('\n#endif',1)[0]
assert 'DSRRL_V367_INDEPENDENT_CLASSIC_RX34' in cmake
assert 'DSRRL_PHYSICAL_CUT_POINTLIGHT_ALL' in cmake
assert 'register_classic_rx34_from_stock(' in h
assert 'classic_envdiffuse_rx34_v365::materialize(' not in cpp  # namespace imported
assert 'materialize(' in cpp.split('register_classic_rx34_from_stock(',1)[1].split('prepare_classic_envdiffuse_only(',1)[0]
assert 'stock_size, candidate' in cpp
assert 'CreatePixelShader(' in cpp.split('register_classic_rx34_from_stock(',1)[1].split('prepare_classic_envdiffuse_only(',1)[0]
assert 'family != pmetal_envspec_receiver_family::stable_hemenv' in island
assert '!material.owner_tuple_exact' in island
assert '!material.material_slot_valid' in island
assert 'decision.receiver_id != 34u' in island
assert 'core::operator_id::env_diffuse' in island
assert 'prepare_envdiffuse_bound(' in island
assert 'prepare_classic_envdiffuse_bound(' in island
assert 'prepared.request.primary = core::operator_id::env_diffuse;' in island
assert 'prepared.request.srvs[0]' in island and '11u, prepared.envdiffuse_resources.ptde_a' in island
assert 'prepared.request.samplers[0]' in island and '11u, prepared.envdiffuse_resources.sampler' in island
assert 'constant_buffers[0]' not in island and 'env_resources_.prepare_bound(' not in island
assert '12u' not in island and '14u' not in island
assert 'DSRRL V367 ENVSPEC T12 AUTHORITY' in resources
assert 'stock_t12_identified' in resources and 'g_pack_ready' in resources
assert 'prepare_classic_envdiffuse_only(' in integrated
assert integrated.count('prepare_classic_envdiffuse_only(')>=2
assert 'register_classic_rx34_from_stock(' in integrated
assert 'dsrrl::core::operator_id::env_diffuse,' in integrated
assert 'V367 RX34 ENVDIFFUSE' in cleanup
assert 'draw_mutation_srv | draw_mutation_sampler' in policy
assert 'required_mutation_mask' in policy
print('V367_EXACT_RX34_ENVDIFFUSE_T11_STOCK_T12_CONTRACT_PASS')
