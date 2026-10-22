#!/usr/bin/env python3
"""V372 source-only post-draw diagnostic contract, not pixel validation."""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
addon=(root/'integrated/integrated_addon.cpp').read_text(encoding='utf-8')
res=(root/'src/runtime/envspec_resource_runtime.cpp').read_text(encoding='utf-8')
shader=(root/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text(encoding='utf-8')
tx=(root/'src/runtime/draw_state_transaction.cpp').read_text(encoding='utf-8')
adapter=(root/'src/runtime/island_draw_adapter.cpp').read_text(encoding='utf-8')
release=(root/'tools/materialize_v230_release_cleanup.py').read_text(encoding='utf-8')

assert addon.count('[DSRRL V372 ED TX]')==2
assert 'kind=draw' in addon and 'kind=indexed' in addon
assert '[DSRRL V372 ED APPEND FAIL]' in addon
assert 'dsrrl::runtime::draw_tx_issued(result)' in addon
assert addon.count('result!=dsrrl::runtime::draw_tx_result::issued_restored')==2
assert addon.count('dsrrl::runtime::draw_tx_result::issued_restore_failed')==0  # V315 materializer inserts two later
assert 'prepared.envspec.envdiffuse_resources.probe_a' in addon
assert 'const char *cls=(probe==500u || probe==510u)' in addon
assert 'probe<342u' in addon
assert 'prepared.batch.mutation.replace_pixel_shader?1u:0u' in addon
assert 'reshade::log::message(reshade::log::level::warning,line);' in addon

# Gate must not loosen: no global material or nonmetal admission.
assert 'exact_pmetal_envspec_candidate(material,decision)' in shader
assert 'source.beta!=0.0f' in shader
assert 'pixel_srv_shadow_snapshot(cmd_list,11u,1u,t11)' in shader
assert 'prepared.ready=true;' in shader
assert 'core_.features().enabled(core::operator_id::env_diffuse)' in shader

# Source identity remains exact and independently split from stock EnvSpec.
assert 'v371_exact_resource_locked' in res
assert 'g_v371_legacy_by_view.find(key)' in res
assert 'g_diffuse_by_view.find(key)' in res
assert 'view->GetResource(&actual_backing);' in res
assert 'g_v371_legacy_by_view.erase(view.handle)' in res
assert 'g_v371_diffuse_creation_sha.erase(resource_handle.handle)' in res
assert 'get_v371_ptde_legacy_envdiffuse_cube' in res
assert '[DSRRL V371 LEGACY T11 RESOURCE READY]' in res
assert '[DSRRL V371 PTDE LEGACY ENVDIFFUSE CUBE]' in res

# Existing D3D11 draw/restore operator stays unchanged.
assert 'transactions.replay_draw(' in adapter
assert 'transactions.replay_draw_indexed(' in adapter
assert 'const bool restored = restore(cmd_list, state);' in tx
assert 'draw_tx_result::issued_restored' in tx
assert 'if (!k_draw_replay_runtime_enabled)' in addon
assert 'constexpr bool k_pmetal_native_draw_runtime_enabled = false;' in addon

# Release cleanup strips INFO, not WARN. Keep the bounded witnesses.
assert 'args.startswith("reshade::log::level::info,")' in release
assert addon.count('static std::atomic<std::uint64_t> v372_ed_tx_count')==2
print('V372 DRAW STAGE SOURCE CONTRACT PASS; runtime draw and PTDE pixel OPEN')
