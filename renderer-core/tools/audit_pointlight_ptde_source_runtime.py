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
require(fixed,'g_snapshots.erase(owner_key)')
require(fixed,'view_desc.Buffer.NumElements=8u')
require(clustered,'pointlight_ptde_source::capture(node, g_base, raw)')
shader=read('src/operators/point_light/fixed_local_specular_single_materializer.cpp')
for token in ['range_compare.erase_words=8u','range_load.words[6]=4u+light','SAT((PTDE End-distance)*PTDE invRange)','a.erase_words>b.erase_words']:
    require(shader,token)
print('POINTLIGHT_PTDE_SOURCE_PASS: exact original banks, paired PTDE donors, operator-local range; no new hooks')
print('SCOPE: host-selected sources only; host membership/culling remains a separate OPEN residual')
