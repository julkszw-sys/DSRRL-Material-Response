#!/usr/bin/env python3
"""Adversarial V440 verifier negative controls, no EXE write."""
import sys
from pathlib import Path
from verify_v440_native_channel import verify,PE
b=Path(sys.argv[1]).read_bytes()
assert verify(b)["static_abi"]=="CONFIRMED"
for name,rva in [("SHA-only",None),("channel_setter",0x1d7820),
 ("typed_packet",0x22024c),("model_vtable",0x12eb148),
 ("type5_emitter_R14",0x1c6939),("type6_emitter_R15",0x1c6c79),
 ("model_virtual_call",0x220ced),("native_packer_call",0x1c69fe)]:
 tampered=bytearray(b)
 pos=len(b)-30 if rva is None else PE(b).offset(rva)
 tampered[pos]^=1
 try:verify(tampered)
 except AssertionError:continue
 raise AssertionError("mutation not rejected: "+name)
print("V440_NATURAL_NEGATIVES_PASS 8")
