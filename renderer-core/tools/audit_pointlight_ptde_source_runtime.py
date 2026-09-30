import argparse
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--source-dir',required=True)
a=p.parse_args(); root=Path(a.source_dir)
def read(path): return (root/path).read_text(encoding='utf-8')
def require(text,needle):
    if needle not in text: raise SystemExit('Missing source invariant: '+needle)
header=read('include/dsrrl/runtime/pointlight_ptde_source_runtime.hpp')
for token in ['0x55bc00u','0x55d0b0u','pair.beta','count!=64u','id!=i||off!=first+16u*i','identify(p+first)','else return false','raw[3]=1.0f/(result.end-result.begin)']:
    require(header,token)
for forbidden in ['VirtualProtect','write_bytes','create_hook','install_hook','fetch_add']:
    if forbidden in header: raise SystemExit('Unexpected source mutation/global hook: '+forbidden)
fixed=read('src/runtime/fixed_pointlight_draw_runtime.cpp')
clustered=read('src/runtime/clustered_pnts_draw_runtime.cpp')
require(fixed,'pointlight_ptde_source::capture(source,g_base,donor_raw)')
require(fixed,'current={};')
require(fixed,'g_gpu_by_context.try_emplace(context)')
require(fixed,'D3D11_MAP_WRITE_DISCARD')
producer=fixed[fixed.index('void __fastcall capture_callback('):fixed.index('bool build_capture_stub(')]
selector=fixed[fixed.index('void fixed_pointlight_draw_runtime::selector_event('):fixed.index('bool fixed_pointlight_draw_runtime::prepare_t19(')]
for hot in (producer,selector):
    for forbidden in ('lock_guard','make_shared','fetch_add','unordered_map','CreateBuffer'):
        if forbidden in hot: raise SystemExit('Global work in fixed producer/selector: '+forbidden)
require(fixed,'view_desc.Buffer.NumElements=8u')
require(clustered,'pointlight_ptde_source::capture(node, g_base, raw)')
capture=clustered[clustered.index('bool capture_source('):clustered.index('void release_gpu_locked()')]
require(capture,'target_address != g_base + 0x55BC00u')
require(capture,'target_address != g_base + 0x55D0B0u')
if capture.index('fn(node, raw.data());') < capture.index('target_address != g_base + 0x55BC00u'):
    raise SystemExit('Clustered PointLight calls host source vfunc before exact donor-class prefilter')
shader=read('src/operators/point_light/fixed_local_specular_single_materializer.cpp')
for token in ['range_compare.erase_words=8u','range_load.words[6]=4u+light','SAT((PTDE End-distance)*PTDE invRange)','a.erase_words>b.erase_words']:
    require(shader,token)
print('POINTLIGHT_PTDE_SOURCE_PASS: exact original banks, paired PTDE donors, operator-local range; no new hooks')
print('SCOPE: host-selected sources only; host membership/culling remains a separate OPEN residual')
