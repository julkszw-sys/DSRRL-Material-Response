#!/usr/bin/env python3
"""V279 transient read-only paired Material-to-CB/SRV/sampler source trace.
Apply only after V275 + V277 source overlays. Validate exact anchors.
"""
from pathlib import Path
import re,argparse
P=Path("renderer-core/integrated/integrated_addon.cpp")
INC='#include "../tools/v277/v277_exact_stable_lerp_dxbc_capture.hpp"'
NEW=INC+'\n#include "../tools/v279/v279_material_carrier_trace.hpp"'
PAT=re.compile(r'(    dsrrl::runtime::v275::observe\(\n'
               r'        cmd_list,material,decision,receiver_id,hemenvlerp_bound,\n'
               r'        "BATCH_PREPARED",3u,&prepared\.batch\.mutation,'
               r'(?:vertex_count|index_count),(?:false|true)\);)')
def transform(s):
    assert s.count(INC)==1 and NEW not in s
    s=s.replace(INC,NEW,1)
    matches=PAT.findall(s)
    assert len(matches)==2,("expected both indexed and ordinary draws",len(matches))
    def inject(m):
        return m.group(1)+'''
    dsrrl::runtime::v279::observe(
        cmd_list,material,decision,receiver_id,hemenvlerp_bound,
        prepared.batch.mutation);'''
    s=PAT.sub(inject,s)
    assert s.count('dsrrl::runtime::v279::observe(')==2
    return s
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--apply',action='store_true')
    a=parser.parse_args()
    s=P.read_text(encoding='utf8')
    if a.apply:P.write_text(transform(s),encoding='utf8',newline='\n')
    else:
        assert NEW in s and s.count('dsrrl::runtime::v279::observe(')==2
    print('V279_EXACT_MATERIAL_TRANSPORT_HOOK_ORACLE_PASS')
    print('V279_READ_ONLY_GPU_CB_PIXEL_UNOBSERVED')
if __name__=='__main__':main()
