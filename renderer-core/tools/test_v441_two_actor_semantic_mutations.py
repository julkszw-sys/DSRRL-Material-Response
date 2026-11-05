#!/usr/bin/env python3
"""V441 semantic negative controls; mutate ephemeral in-memory PE, never EXE on disk."""
import sys
from pathlib import Path
from verify_v440_native_channel import PE,verify
from verify_v441_two_actor_source_census import check
b=Path(sys.argv[1]).read_bytes()
pe=PE(b);verify(pe);check(pe)
mutations={
 "asm_type5":0x2167ba,"model_type5":0x220639,"model_type6":0x220632,
 "single_packer":0x1c69fe,"blend_packer":0x1c6f4a,
 "asm_actor_channel":0x219d79,"model_actor_channel":0x222d49,
 "other_object_0x1b0":0x2d39ab,"other_object_0x58":0x55d046,
 "specialized_branch":0x221412,
 "rtti_asm_method":0x12ebfe8+0x50,"rtti_model_method":0x12eca10+0x50,
 "asm_bit_branch":0x216232,"model_bit_branch":0x220054
}
for name,rva in mutations.items():
 data=bytearray(b)
 data[pe.offset(rva)]^=1
 try:check(PE(bytes(data)),strict_sha=False)
 except AssertionError:continue
 raise AssertionError("V441 mutation escaped: "+name)
print("V441_SEMANTIC_MUTATION_NEGATIVES_PASS",len(mutations))
