#!/usr/bin/env python3
"""V281 only after V279 transient injection. No change to game code on disk.
Exact two source anchors in indexed/nonindexed paths; restore inherited.
"""
import argparse,re
from pathlib import Path
P=Path("renderer-core/integrated/integrated_addon.cpp")
OLD='#include "../tools/v279/v279_material_carrier_trace.hpp"'
NEW=OLD+'\n#include "../tools/v281/v281_flver_ia_vs_trace.hpp"'
PAT=re.compile(r'(    dsrrl::runtime::v279::observe\(\n'
               r'        cmd_list,material,decision,receiver_id,hemenvlerp_bound,\n'
               r'        prepared\.batch\.mutation\);)')
def transform(s):
    assert s.count(OLD)==1 and NEW not in s
    s=s.replace(OLD,NEW,1)
    matches=PAT.findall(s)
    assert len(matches)==2,('V279 anchors drift',len(matches))
    def add(m):
        return m.group(1)+'''
    dsrrl::runtime::v281::observe(
        cmd_list,material,decision,receiver_id,hemenvlerp_bound);'''
    s=PAT.sub(add,s)
    assert s.count('dsrrl::runtime::v281::observe(')==2
    return s
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--apply',action='store_true')
    a=ap.parse_args();s=P.read_text(encoding='utf8')
    if a.apply:P.write_text(transform(s),encoding='utf8',newline='\n')
    else:
        assert NEW in s and s.count('dsrrl::runtime::v281::observe(')==2
    print('V281_EXACT_HOST_IA_VS_WITNESS_SOURCE_PASS')
    print('V281_NO_IA_MUTATION_NO_GPU_VERTEX_READBACK_NO_PIXELS')
if __name__=='__main__':main()
