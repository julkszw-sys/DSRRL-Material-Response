import argparse
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--source-dir',required=True)
a=p.parse_args(); root=Path(a.source_dir)
def read(path): return (root/path).read_text(encoding='utf-8')
def require(text,needle):
    if needle not in text: raise SystemExit('Missing source invariant: '+needle)
header=read('include/dsrrl/runtime/pointlight_ptde_source_runtime.hpp')
for token in ['0x55bc00u','0x55d0b0u','pair.beta','count != 64u','row_offset != first + 16u * i','bank_structure_signature(','identify_structure(structure_signature)','else return false','raw[3]=1.0f/(result.end-result.begin)','access_cache cache{}','readable_cached(param+first,1024u,cache)']:
    require(header,token)
if 'thread_local access_cache' in header:
    raise SystemExit('PointLight VM cache must not survive beyond one donor capture')
for forbidden in ['VirtualProtect','write_bytes','create_hook','install_hook','fetch_add']:
    if forbidden in header: raise SystemExit('Unexpected source mutation/global hook: '+forbidden)
identity=read('include/dsrrl/runtime/pointlight_ptde_source.hpp')
authority=read('include/dsrrl/runtime/pointlight_bank_structure_authority_v1.hpp')
require(identity,'identify_structure(')
require(identity,'pointlight_bank_structure_authority_v1::k_signatures')
require(authority,'k_known_non_donor_signatures')
fixed=read('src/runtime/fixed_pointlight_draw_runtime.cpp')
clustered=read('src/runtime/clustered_pnts_draw_runtime.cpp')
# Fixed PntSS/PntSSSS uses the separately attested raw-q capture site and its
# own exact shader/range materializer. It no longer routes through the
# clustered Bank/Lerp donor decoder.
require(fixed,'void __fastcall capture_callback(')
require(fixed,'current->raw_q[slot]=value;')
require(fixed,'slot==0u')
require(fixed,'current={};')
require(fixed,'g_gpu_by_context.try_emplace(context)')
require(fixed,'D3D11_MAP_WRITE_DISCARD')
producer=fixed[fixed.index('void __fastcall capture_callback('):fixed.index('bool build_capture_stub(')]
selector=fixed[fixed.index('void fixed_pointlight_draw_runtime::selector_event('):fixed.index('bool fixed_pointlight_draw_runtime::prepare_t19(')]
for hot in (producer,selector):
    for forbidden in ('lock_guard','make_shared','fetch_add','unordered_map','CreateBuffer'):
        if forbidden in hot: raise SystemExit('Global work in fixed producer/selector: '+forbidden)
require(fixed,'view_desc.Buffer.NumElements=8u')
require(clustered,'(void)pointlight_ptde_source::capture(')
require(clustered,'readable_region_cache node_region{}')
require(clustered,'!readable_range_cached(')
if '!readable_range(node, 0x50u)' in clustered:
    raise SystemExit('Clustered selector regressed to per-node VirtualQuery validation')
capture=clustered[clustered.index('bool capture_source('):clustered.index('void release_gpu(')]
require(capture,'target_address != g_base + 0x55BC00u')
require(capture,'target_address != g_base + 0x55D0B0u')
if capture.index('fn(node, raw.data());') < capture.index('target_address != g_base + 0x55BC00u'):
    raise SystemExit('Clustered PointLight calls host source vfunc before exact donor-class prefilter')

# Clustered dynamic t18/t19/b12 must be recording-context local just like the
# fixed PointLight carrier. A process-global DISCARD buffer lets another
# immediate/deferred context replace payload between prepare and bind.
require(clustered,'std::unordered_map<ID3D11DeviceContext *,gpu_resources>')
require(clustered,'g_gpu_by_context{}')
require(clustered,'g_gpu_by_context.try_emplace(')
require(clustered,'gpu_resources *gpu = nullptr;')
require(clustered,'ensure_gpu_locked(')
require(clustered,'release_all_gpu_locked();')
if 'gpu_resources g_gpu{}' in clustered:
    raise SystemExit('Clustered PointLight regressed to one process-global GPU carrier')
if 'bool ensure_gpu(' in clustered:
    raise SystemExit('Clustered PointLight regressed to separate locked ensure_gpu pass')

prepare=clustered[clustered.index('bool clustered_pnts_draw_runtime::prepare_sidecar('):clustered.index('void clustered_pnts_draw_runtime::release_prepared_draw(')]
if prepare.count('std::lock_guard<std::mutex> lock(') != 1:
    raise SystemExit('Clustered PointLight prepare must use one carrier synchronization point')
for token in ['gpu->t18_buffer','gpu->t19_buffer','gpu->b12','gpu->t18_srv->AddRef()','gpu->t19_srv->AddRef()','gpu->b12->AddRef()']:
    require(prepare,token)
shader=read('src/operators/point_light/fixed_local_specular_single_materializer.cpp')
for token in ['range_compare.erase_words=8u','range_load.words[6]=4u+light','SAT((PTDE End-distance)*PTDE invRange)','a.erase_words>b.erase_words']:
    require(shader,token)
print('POINTLIGHT_PTDE_SOURCE_PASS: exact Bank/Lerp classes, structural PTDE donor priority, attested raw-source fallback, operator-local PTDE attenuation; no new hooks')
print('SCOPE: host-selected sources only; foreign source classes still fail open; host membership/culling remains a separate OPEN residual')
