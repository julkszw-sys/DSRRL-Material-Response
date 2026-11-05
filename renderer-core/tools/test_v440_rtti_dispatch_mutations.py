#!/usr/bin/env python3
"""V440-R4 original RTTI and per-class dispatch mutation adversarial controls."""
from pathlib import Path
import sys
from verify_v440_native_channel import PE,verify
from verify_v440_entity_rtti_dispatch import check as rtti
from verify_v440_dispatch_modes import check as dispatcher
b=Path(sys.argv[1]).read_bytes();p=PE(b);verify(b);rtti(b);dispatcher(b)
cases=[
 ("asm_vtable_mode",0x12ec038),("model_vtable_mode",0x12eca60),
 ("asm_type_col",0x163d304),("shared_base_array",0x163d33c),
 ("asm_rtti_name",0x1af9920+16),("asm_flags_mask",0x216248),
 ("model_flags_mask",0x220054)]
for label,rva in cases:
 copy=bytearray(b);copy[p.offset(rva)]^=1
 try:
  rtti(bytes(copy),sha=False);dispatcher(bytes(copy),sha=False)
 except AssertionError:continue
 raise AssertionError("missed RTTI/dispatch corruption "+label)
print("V440_RTTI_DISPATCH_SEMANTIC_NEGATIVES_PASS",len(cases))
