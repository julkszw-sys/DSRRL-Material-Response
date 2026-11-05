#!/usr/bin/env python3
"""V440-R3 mutation tests for alternate builder, SHA is checked on baseline only."""
from pathlib import Path
import sys
from verify_v440_native_channel import PE,verify
from verify_v440_alternate_builder import check
b=Path(sys.argv[1]).read_bytes();p=PE(b);verify(b);check(b)
for label,rva in [
 ("packet_A",0x21642d),("type5_wrapper",0x2167ba),
 ("virtual_model_call",0x2172aa),("unwind_root_flags",0x174d464)]:
 copy=bytearray(b);copy[p.offset(rva)]^=0x10
 try:check(bytes(copy),sha=False)
 except AssertionError:continue
 raise AssertionError("alternate semantic mutation missed: "+label)
print("V440_ALT_SEMANTIC_NEGATIVES_PASS 4")
