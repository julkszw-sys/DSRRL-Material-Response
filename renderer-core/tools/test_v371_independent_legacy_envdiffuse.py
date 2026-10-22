#!/usr/bin/env python3
"""V371 source/ABI guard: this is NOT an in-game GPU or pixel-validation test."""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
src=(root/'src/runtime/envspec_resource_runtime.cpp').read_text(encoding='utf8')
shader=(root/'src/runtime/pmetal_envspec_draw_runtime.cpp').read_text(encoding='utf8')
head=(root/'include/dsrrl/operators/env_spec/legacy_envdiffuse_m18_exact.hpp').read_text(encoding='utf8')
name=(root/'include/dsrrl/runtime/gi_envdif_name_witness.hpp').read_text(encoding='utf8')
cmake=(root/'integrated/CMakeLists.txt').read_text(encoding='utf8')

# Two exact native DSR t11 identities and independently SHA-verified PTDE donors.
assert 'EnvDif_m18_500' in head and 'EnvDif_m18_510' in head
assert '24576u' in head and '1536u' in head
assert 'v364_exact_legacy_diffuse' in head
assert 'texture_identity_transport::snapshot_raw' in name
assert 'gi_envdif_name_witness::capture' in src
assert 'g_v371_diffuse_creation_sha' in src
assert 'g_v371_legacy_by_view' in src
assert 'v371_record_name_locked' in src
assert 'v371_exact_resource_locked' in src
assert 'name.conflicting' in src or 'ni->second.conflicting' in src
assert 'view->GetResource(&actual_backing)' in src
assert 'view->GetDevice(&actual_device)' in src
assert 'backing_key==legacy->second.resource_handle' in src
assert 'g_v371_diffuse_creation_sha.erase(resource_handle.handle)' in src
assert 'g_v371_legacy_by_view.erase(view.handle)' in src

# Content-addressed cold asset loading; only two approved m18 assets get GPU
# replacement; the remaining 37 verified DDS stay unbound.
assert 'v371_load_exact_legacy_asset' in src
assert 'hashing::sha256(bytes.data(),bytes.size())!=entry.ptde_asset_sha' in src
assert 'get_v371_ptde_legacy_envdiffuse_cube' in src
assert 'format::r8g8b8a8_unorm' in src
assert 'v364_legacy_by_selector(probe)' in src
assert 'g_v371_legacy_rgba[index].size()!=entry->bytes' in src

# EnvDiffuse cannot depend on the old coupled FULL HemEnv flag.
guard='#if !defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG) && !defined(DSRRL_V370_ISOLATED_EXACT_ENVDIFFUSE)'
assert src.count(guard)>=5
assert 'DSRRL_V370_ISOLATED_EXACT_ENVDIFFUSE' in cmake
bound=src.split('bool envspec_resource_runtime::prepare_envdiffuse_bound(',1)[1].split('bool envspec_resource_runtime::prepare_envdiffuse(',1)[0]
assert 'g_diffuse_by_view.find(key)' in bound  # PackedGI
assert 'g_v371_legacy_by_view.find(key)' in bound  # separate LegacyGI
assert 'prepare_envdiffuse(a,b,probe_b_required,prepared)' in bound
assert 'prepare_envspec(' not in bound and 'prepare_native_dsr(' not in bound
assert 'g_device == nullptr' in bound
assert 'if(!exact)return false;' in bound

# Mixed representations and unknown sources must fail open.
diffuse=src.split('bool envspec_resource_runtime::prepare_envdiffuse(',1)[1].split('void envspec_resource_runtime::release(',1)[0]
assert 'if(!known(probe_a) || !known(probe_b))return false' in diffuse
assert 'probe_b_required &&' in diffuse
assert 'probe_a>=env::k_legacy_envspec_probe_count' in diffuse
assert 'probe_b>=env::k_legacy_envspec_probe_count' in diffuse
assert 'needs_pack && !g_envdiffuse_pack_ready' in diffuse

# Old exact PTDE stable operator remains authoritative. No V369 fake c86.
assert 'source_.latest(material,source)' in shader
assert 'payload[12]=source.envdiffuse_a[0]' in shader
assert 'payload[13]=source.envdiffuse_a[1]' in shader
assert 'payload[14]=source.envdiffuse_a[2]' in shader
assert 'q.srvs[0]={11u,prepared.envdiffuse_resources.ptde_a};' in shader
assert 'q.constant_buffers[0]={12u,prepared.b12,owner};' in shader
print('V371 source-contract PASS — native GPU/activation/pixels OPEN')
