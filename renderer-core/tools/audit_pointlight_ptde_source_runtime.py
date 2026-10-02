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

# Clustered path: exact retail source class is the authority. The host vfunc
# supplies the homologous CPU-packed signal; exact PTDE donor may override it
# when logical-bank authority resolves, but donor miss must not discard the
# entire ordinary PointLight draw.
capture=clustered[clustered.index('bool capture_source('):clustered.index('void release_gpu(')]
require(capture,'target_address != g_base + 0x55BC00u')
require(capture,'target_address != g_base + 0x55D0B0u')
require(capture,'fn(node, raw.data());')
require(capture,'(void)pointlight_ptde_source::capture(')
if capture.index('fn(node, raw.data());') < capture.index('target_address != g_base + 0x55BC00u'):
    raise SystemExit('Clustered PointLight calls host source vfunc before exact source-class prefilter')
if 'if (!pointlight_ptde_source::capture(' in capture:
    raise SystemExit('Clustered ordinary PointLight donor miss still rejects the whole draw')
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
    't18_buffer->AddRef();',
    't19_buffer->AddRef();',
    't18_srv->AddRef();',
    't19_srv->AddRef();',
    'b12->AddRef();',
    'const bool uploaded =',
    'prepared.t18 = t18_srv;',
    'prepared.t19 = t19_srv;',
    'prepared.b12 = b12;'
]:
    require(prepare,token)

if 'update_buffer(\n            context,\n            gpu->' in prepare:
    raise SystemExit('Clustered PointLight DISCARD upload is still performed through mutex-owned gpu pointer')

lock_pos=prepare.index('std::lock_guard<std::mutex> lock(')
upload_pos=prepare.index('const bool uploaded =')
device_release_pos=prepare.index('device->Release();',lock_pos)
if not (lock_pos < device_release_pos < upload_pos):
    raise SystemExit('Clustered PointLight upload must occur after lookup/create lock scope and retained-resource handoff')

# Clustered Spc is not production-complete until its DSR microfacet window is
# actually rewritten to the PTDE legacy local-specular kernel. The current
# journal does not own that rewrite. Runtime must fail open rather than claim
# local_specular_legacy ownership over a DSR-only GGX tail.
for token in [
    'clustered_spc_failopen_unmaterialized_legacy_specular',
    'if (prepared.clustered_shader.spc)',
    'g_clustered_pnts_pipeline.release_prepared_shader('
]:
    require(integrated,token)
require(ownership,'Spc=FAIL_OPEN until local_specular_legacy microfacet window is exactly materialized')
require(ownership,'Spc still contains the stock DSR microfacet local-specular window')

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
