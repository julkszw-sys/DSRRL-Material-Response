#!/usr/bin/env python3
"""Fix the exact PTDE t11 RGB/A output gather, with strict DXBC postconditions.

Run after materialize_v230_source1_envdiffuse_split.py, before CMake build.
The t11 resource must be .wxyz (sampled register = A,R,G,B); the numerator
r12.yyzw and denominator r12.xxxx feed r12.yzw correctly. Only the final
r2.xyz gather must change from yyzw -> yzww. Retain Build131 / V13.
"""
from pathlib import Path

p=Path("renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp")
s=p.read_text("utf-8")
def swap_in_scope(start,end,old,new,reason):
    global s
    a=s.index(start)
    b=s.index(end,a)
    sub=s[a:b]
    if sub.count(old)!=1:
        raise RuntimeError(reason)
    s=s[:a]+sub.replace(old,new,1)+s[b:]

swap_in_scope(
    "bool apply_exact_ptde_envdiffuse_consumer(","\n#endif",
    "            0x00100e56u,12u,\n            0x00208246u,12u,3u",
    "            0x00100f96u,12u,\n            0x00208246u,12u,3u",
    "ENVDIFFUSE_OUTPUT_SWIZZLE_DRIFT")
swap_in_scope(
    "bool apply_exact_ptde_envdiffuse_consumer(","\n#endif",
    "            words[sample.offset + 8u] != 11u ||\n            words[sample.offset + 10u] != 11u)",
    "            words[sample.offset + 7u] != 0x00107936u ||\n            words[sample.offset + 8u] != 11u ||\n            words[sample.offset + 10u] != 11u)",
    "ENVDIFFUSE_T11_RESOURCE_SWIZZLE_DRIFT")

# A shader that has the expected opcodes but incorrect channel mapping must
# fail-open. Validate the complete introduced DIV and MUL encoded instructions.
old="""           words[a.offset+8u]!=11u || words[a.offset+10u]!=11u)"""
new="""           words[a.offset+7u]!=0x00107936u ||
           words[a.offset+8u]!=11u || words[a.offset+10u]!=11u)"""
swap_in_scope(
    "bool has_exact_ptde_envdiffuse_consumer(",
    "bool apply_exact_ptde_envdiffuse_consumer(",
    old,new,"ENVDIFFUSE_POST_SAMPLE_DRIFT")
old="""           words[c.offset+6u]!=12u || words[c.offset+7u]!=3u)"""
new="""           words[b.offset+1u]!=0x001000e2u ||
           words[b.offset+2u]!=12u ||
           words[b.offset+3u]!=0x00100e56u ||
           words[b.offset+4u]!=12u ||
           words[b.offset+5u]!=0x00100006u ||
           words[b.offset+6u]!=12u ||
           words[c.offset+1u]!=0x00100072u ||
           words[c.offset+2u]!=2u ||
           words[c.offset+3u]!=0x00100f96u ||
           words[c.offset+4u]!=12u ||
           words[c.offset+5u]!=0x00208246u ||
           words[c.offset+6u]!=12u || words[c.offset+7u]!=3u)"""
swap_in_scope(
    "bool has_exact_ptde_envdiffuse_consumer(",
    "bool apply_exact_ptde_envdiffuse_consumer(",
    old,new,"ENVDIFFUSE_POST_OPERANDS_DRIFT")

p.write_text(s,"utf-8")
print("V230_PTDE_ENVDIFFUSE_WXYZ_TO_RGB_SWIZZLE_PASS")
