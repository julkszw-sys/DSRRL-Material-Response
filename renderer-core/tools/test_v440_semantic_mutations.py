#!/usr/bin/env python3
"""V440 semantic mutation controls on private in-memory PE copies only."""
from pathlib import Path
import sys
from verify_v440_native_channel import PE,verify
from verify_v440_builder_unwind import check
b=Path(sys.argv[1]).read_bytes();p=PE(b)
assert verify(b)["static_abi"]=="CONFIRMED"
assert check(b)["original_SHA_unwind_static"]=="CONFIRMED"
opcodes=[
 ("LightBank_channel",0x22024c),("type5_R14",0x1c6939),
 ("type6_R15",0x1c6c79),("model_packet_R15",0x20dde4),
 ("single_packer_call",0x1c69fe),("model_vtable",0x12eb148)]
for label,rva in opcodes:
 copy=bytearray(b);copy[p.offset(rva)]^=1
 try:verify(bytes(copy),require_sha=False)
 except AssertionError:continue
 raise AssertionError("missed opcode corruption "+label)
unwind=[
 ("builder_root_flags",0x174f4e8),
 ("builder_child_chain",0x174f560),
 ("model_child_chain",0x174b548)]
for label,rva in unwind:
 copy=bytearray(b);copy[p.offset(rva)]^=0x10
 try:check(bytes(copy),verify_source=False)
 except AssertionError:continue
 raise AssertionError("missed unwind corruption "+label)
print("V440_SEMANTIC_NEGATIVES_PASS",len(opcodes)+len(unwind))
