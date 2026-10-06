import argparse
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--source-dir',required=True)
a=p.parse_args(); root=Path(a.source_dir)

def read(path):
    return (root/path).read_text(encoding='utf-8')

def require(text,needle):
    if needle not in text:
        raise SystemExit('Missing source invariant: '+needle)

header=read('include/dsrrl/runtime/pointlight_ptde_source_runtime.hpp')
identity=read('include/dsrrl/runtime/pointlight_ptde_source.hpp')
authority=read('include/dsrrl/runtime/pointlight_bank_structure_authority_v1.hpp')
fixed=read('src/runtime/fixed_pointlight_draw_runtime.cpp')
clustered=read('src/runtime/clustered_pnts_draw_runtime.cpp')
integrated=read('integrated/integrated_addon.cpp')
ownership=read('data/provenance/clustered_pnts_composed_shader_ownership_v1.json')

# Exact donor path remains available and fail-open.
for token in [
    '0x55bc00u','0x55d0b0u','bank_structure_signature(',
    'identify_structure(structure_signature)','raw[3]=1.0f/(result.end-result.begin)',
    'access_cache cache{}'
]:
    require(header,token)
require(identity,'identify_structure(')
require(authority,'k_signatures')
require(authority,'k_known_non_donor_signatures')
if 'thread_local access_cache' in header:
    raise SystemExit('PointLight VM cache must not survive beyond one donor capture')
for forbidden in ['VirtualProtect','write_bytes','create_hook','install_hook','fetch_add']:
    if forbidden in header:
        raise SystemExit('Unexpected source mutation/global hook: '+forbidden)

# Fixed PntSS/PntSSSS consumers are already linear in the confirmed census.
# Their runtime captures the host-selected packed source directly and must not
# be forced through the clustered donor/attenuation path.
require(fixed,'void __fastcall capture_callback(')
require(fixed,'current->raw_q[slot]=value')
require(fixed,'g_snapshots[owner_key]=current')
require(fixed,'desc.Usage=D3D11_USAGE_IMMUTABLE')
require(fixed,'view_desc.Buffer.NumElements=4u')
require(fixed,'bool fixed_pointlight_draw_runtime::source_ready() const noexcept')
require(fixed,'const auto selected=g_draw_snapshot;')

# R52 source-first clustered mode retires the old draw-side sidecar/replacement
# path. Keep the historical implementation source for provenance, but require
# current activation to happen only at the exact retail source cut.
if 'constexpr bool k_clustered_pointlight_receiver_runtime_enabled = false;' in integrated:
    require(clustered,'k_clustered_source_override_rva = 0xB7E02u;')
    require(clustered,'k_clustered_source_override_preimage')
    require(clustered,'clustered_source_override_callback(')
    require(clustered,'pointlight_ptde_source::capture(')
    require(clustered,'Clustered PntS never performs receiver/material authorization.')
    require(integrated,'g_clustered_pointlight_selection_transport_active.store(\n        false,')
    require(integrated,'!g_fixed_pointlight.source_ready()')
    print('POINTLIGHT_PTDE_SOURCE_PASS: clustered source-only producer cut active; clustered receiver/material path retired; fixed source gate precedes receiver/material work')
    print('SCOPE: construction/static activation only; runtime/pixel equivalence remains OPEN')
    raise SystemExit(0)

# Clustered path: exact retail source class is the authority. R36 removed the
# duplicate Bank/Lerp host pack on donor success: Bank/Lerp read only the
# attested host-owned position lane, reconstruct PTDE source first, and invoke
# the exact stock source vfunc only when donor authority misses. Direct keeps
# its exact homologous stock packer. Donor miss must never discard the draw.
capture=clustered[clustered.index('bool capture_source('):clustered.index('void release_gpu(')]
for token in [
    'target_address == g_base + 0x55BC00u',
    'target_address == g_base + 0x55C570u',
    'target_address == g_base + 0x55D0B0u',
    'if (!bank_source &&',
    'bank_source ? 0x60u : 0x70u',
    'if (bank_source || lerp_bank_source) {',
    'if (!pointlight_ptde_source::capture(',
    'fn(node, raw.data());'
]:
    require(capture,token)
if capture.index('fn(node, raw.data());') < capture.index('if (!pointlight_ptde_source::capture('):
    raise SystemExit('Clustered Bank/Lerp PointLight regressed to stock pack before PTDE donor attempt')
fallback_start=capture.index('if (!pointlight_ptde_source::capture(')
fallback_end=capture.index('    } else {',fallback_start)
if 'fn(node, raw.data());' not in capture[fallback_start:fallback_end]:
    raise SystemExit('Clustered Bank/Lerp donor miss no longer preserves exact stock source fallback')
for token in ['raw[3] > 0.0f','raw[7] > 0.0f','out.position_inv_range[i] = raw[i]','out.raw_q_end[i] = raw[4u + i]']:
    require(capture,token)

# Dynamic t18/t19/b12 remains recording-context local. The global carrier
# mutex may protect lookup/create only; actual DISCARD uploads must not stay
# serialized across independent recording contexts.
for token in [
    'std::unordered_map<ID3D11DeviceContext *,gpu_resources>',
    'g_gpu_by_context.try_emplace(',
    'ensure_gpu_locked(',
    'gpu->t18_buffer','gpu->t19_buffer','gpu->b12'
]:
    require(clustered,token)
if 'gpu_resources g_gpu{}' in clustered:
    raise SystemExit('Clustered PointLight regressed to one process-global GPU carrier')

prepare=clustered[clustered.index('bool clustered_pnts_draw_runtime::prepare_sidecar('):clustered.index('void clustered_pnts_draw_runtime::release_prepared_draw(')]
if prepare.count('std::lock_guard<std::mutex> lock(') != 1:
    raise SystemExit('Clustered PointLight prepare must use one lookup/create synchronization point')

for token in [
    'ID3D11Buffer *t18_buffer = nullptr;',
    'ID3D11ShaderResourceView *t18_srv = nullptr;',
    'ID3D11Buffer *t19_buffer = nullptr;',
    'ID3D11ShaderResourceView *t19_srv = nullptr;',
    'ID3D11Buffer *b12 = nullptr;',
    'lookup_gpu_fast_cache(',
    'store_gpu_fast_cache(',
    't18_buffer = fast->t18_buffer;',
    't18_srv = fast->t18_srv;',
    't19_buffer = fast->t19_buffer;',
    't19_srv = fast->t19_srv;',
    'b12 = fast->b12;',
    'const bool uploaded =',
    'prepared.t18 = t18_srv;',
    'prepared.t19 = t19_srv;',
    'prepared.b12 = b12;',
    'prepared.carrier_borrowed_tls = true;'
]:
    require(prepare,token)

# R29 moved stable resource ownership into the TLS fast-cache fill path. No
# per-draw COM churn is allowed after a cache hit; the fast-cache entry retains
# resources until epoch invalidation/teardown.
if '->AddRef();' in prepare:
    raise SystemExit('Clustered PointLight prepare regressed to per-draw COM AddRef churn')
for token in [
    'entry.t18_buffer->AddRef();',
    'entry.t18_srv->AddRef();',
    'entry.t19_buffer->AddRef();',
    'entry.t19_srv->AddRef();',
    'entry.b12->AddRef();',
    'g_resource_epoch'
]:
    require(clustered,token)

if 'update_buffer(\n            context,\n            gpu->' in prepare:
    raise SystemExit('Clustered PointLight DISCARD upload is still performed through mutex-owned gpu pointer')

lock_pos=prepare.index('std::lock_guard<std::mutex> lock(')
upload_pos=prepare.index('const bool uploaded =')
device_release_pos=prepare.index('device->Release();',lock_pos)
if not (lock_pos < device_release_pos < upload_pos):
    raise SystemExit('Clustered PointLight upload must occur after lookup/create lock scope and retained-resource handoff')

# Clustered Spc is production-authorized only after the exact historical
# direct-PTDE replacement and its current Runtime-v2 b12 migration both
# attest successfully. Never accept a partial hybrid with stale material
# operands or incomplete legacy-specular ownership.
for token in [
    'clustered_spc_failopen_legacy_specular_attestation',
    '!prepared.clustered_shader.current_b12_abi',
    '!prepared.clustered_shader.legacy_specular_complete',
    'g_clustered_pnts_pipeline.release_prepared_shader('
]:
    require(integrated,token)
require(ownership,'Spc=local_specular_legacy only after current_b12_abi + legacy_specular_complete attestation')
require(ownership,'Spc replay is permitted only when both current_b12_abi and legacy_specular_complete survive candidate registration')

shader=read('src/operators/point_light/fixed_local_specular_single_materializer.cpp')
for token in [
    'range_compare.erase_words=8u',
    'range_load.words[6]=4u+light',
    'SAT((PTDE End-distance)*PTDE invRange)',
    'a.erase_words>b.erase_words'
]:
    require(shader,token)

print('POINTLIGHT_PTDE_SOURCE_PASS: fixed linear path preserved; clustered NoSpc exact source classes retain PTDE attenuation; incomplete clustered Spc legacy-specular island fails open to stock DSR')
print('SCOPE: runtime activation candidate only; pixel equivalence remains OPEN where numeric host source differs from PTDE donor')
