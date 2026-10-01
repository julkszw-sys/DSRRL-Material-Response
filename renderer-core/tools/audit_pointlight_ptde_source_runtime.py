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

# Fixed path still requires exact donor capture; current activation fix is
# specifically for ordinary clustered Bank/Lerp sources.
require(fixed,'pointlight_ptde_source::capture(source,g_base,donor_raw)')
require(fixed,'g_gpu_by_context.try_emplace(context)')
require(fixed,'D3D11_MAP_WRITE_DISCARD')

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

# Dynamic t18/t19/b12 remains recording-context local.
for token in [
    'std::unordered_map<ID3D11DeviceContext *,gpu_resources>',
    'g_gpu_by_context.try_emplace(',
    'ensure_gpu_locked(',
    'gpu->t18_buffer','gpu->t19_buffer','gpu->b12',
    'gpu->t18_srv->AddRef()','gpu->t19_srv->AddRef()','gpu->b12->AddRef()'
]:
    require(clustered,token)
if 'gpu_resources g_gpu{}' in clustered:
    raise SystemExit('Clustered PointLight regressed to one process-global GPU carrier')

prepare=clustered[clustered.index('bool clustered_pnts_draw_runtime::prepare_sidecar('):clustered.index('void clustered_pnts_draw_runtime::release_prepared_draw(')]
if prepare.count('std::lock_guard<std::mutex> lock(') != 1:
    raise SystemExit('Clustered PointLight prepare must use one carrier synchronization point')

shader=read('src/operators/point_light/fixed_local_specular_single_materializer.cpp')
for token in [
    'range_compare.erase_words=8u',
    'range_load.words[6]=4u+light',
    'SAT((PTDE End-distance)*PTDE invRange)',
    'a.erase_words>b.erase_words'
]:
    require(shader,token)

print('POINTLIGHT_PTDE_SOURCE_PASS: exact source classes; PTDE donor preferred; homologous clustered host source survives donor miss; PTDE attenuation island retained')
print('SCOPE: runtime activation candidate only; pixel equivalence remains OPEN where numeric host source differs from PTDE donor')
