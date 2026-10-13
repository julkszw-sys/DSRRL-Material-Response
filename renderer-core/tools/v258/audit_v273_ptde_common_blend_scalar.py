#!/usr/bin/env python3
"""V273: scoped PTDE HemEnvLerp producer t -> c84.w,c85.w equality oracle.

Read-only, owner SHA-pinned original PTDE DATA.exe x86, never redistribute it.
Result concerns ONLY the selector-mismatch branch in 0xF94C50. The broader
PTDE shader family and PTDE-versus-DSR t semantic mapping remain OPEN.
"""
import argparse,hashlib,json,struct
from pathlib import Path

EXE_SHA="88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d"
# Explicit original VA -> complete original instruction bytes, not guessed
# offset patterns and never patched into any user game executable.
PINS={
 "selector_mismatch":(0xF94C8C,"3bf20f8589050000"),
 "source_t_first_saved":(0xF95223,"f30f104608"),
 "source_t_stack_saved":(0xF95228,"f30f1144242c"),
 "blend_t_load":(0xF95515,"d944242c"),
 "blend_t_push":(0xF95529,"d91c24"),
 "blend_decode_call":(0xF95534,"e817c30500"),
 "blend_a0_zero":(0xFF1923,"d9ee"),
 "blend_a0_push":(0xFF1929,"d91c24"),
 "blend_a1_zero":(0xFF1935,"d9ee"),
 "blend_a1_push":(0xFF193B,"d91c24"),
 "blend_b0_t":(0xFF1958,"d94510"),
 "blend_b0_push":(0xFF195F,"d91c24"),
 "blend_b1_t":(0xFF197D,"d94510"),
 "blend_b1_push":(0xFF1984,"d91c24"),
 "decode_w_reads_argument":(0x4BBFE7,"f30f104508"),
 "decode_w_writes_output":(0x4BBFEC,"f30f11400c"),
 "blend_b0_xyz":(0xFF1987,"660fd64620"),
 "blend_b0_zw":(0xFF198C,"f30f7e4008"),
 "blend_b1_xyz":(0xFF19A6,"660fd64630"),
 "blend_b1_zw":(0xFF19AB,"f30f7e4008"),
 "c84w_from_blend_b0":(0xF9567D,"f30f1044245c"),
 "c85w_from_blend_b1":(0xF956F4,"f30f1044246c"),
 "c84w_copy_without_gain":(0xF95683,"f30f1144241c"),
 "c85w_copy_without_gain":(0xF956FE,"f30f1144241c"),
 "c84_setter_call":(0xF9569C,"e8bff548ff"),
 "c85_setter_call":(0xF95713,"e858f648ff"),
 "rgb_gain_c84":(0xF95633,"f30f104514"),
 "rgb_gain_c85":(0xF956AA,"f30f104514"),
 "steady_c84w":(0xF95165,"f30f1044245c"),
 "steady_c85w":(0xF951DC,"f30f1044246c"),
 "steady_decode":(0xF94D9D,"e85ec80500"),
 "steady_zero_w":(0xFF171B,"f30f115c241c"),
 "fallback_zero_w":(0xFF177B,"f30f1144241c"),
}
def off(b,va):
 pe=struct.unpack_from("<I",b,0x3c)[0]
 if b[pe:pe+4]!=b"PE\0\0":raise ValueError("not an original PTDE PE file")
 count=struct.unpack_from("<H",b,pe+6)[0]
 opt=struct.unpack_from("<H",b,pe+20)[0]
 base=struct.unpack_from("<I",b,pe+52)[0]
 rva=va-base
 for k in range(count):
  t=pe+24+opt+40*k
  virtual_size,virtual_addr,raw_size,raw_ptr=struct.unpack_from("<IIII",b,t+8)
  if virtual_addr<=rva<virtual_addr+raw_size:
   return raw_ptr+rva-virtual_addr
 raise ValueError(f"no original text VA {va:#x}")
def verify(b,full_sha=True):
 if full_sha and hashlib.sha256(b).hexdigest()!=EXE_SHA:raise ValueError("owner SHA mismatch")
 for name,(va,ophex) in PINS.items():
  op=bytes.fromhex(ophex);p=off(b,va)
  if b[p:p+len(op)]!=op:raise ValueError(f"wrong original PTDE instruction {name} {va:#x}")
 for at,to in ((0xF95534,0xFF1850),(0xF9569C,0x424C60),
               (0xF95713,0x424D70),(0xF94D9D,0xFF1600)):
  p=off(b,at)
  if b[p]!=0xE8 or at+5+struct.unpack_from("<i",b,p+1)[0]!=to:
   raise ValueError("PTDE call target mismatch")
 return {
  "version":"V273","status":"CONFIRMED_ORIGINAL_BINARY_STATIC_BRANCH",
  "exe_sha256":EXE_SHA,"scope":"0xF94C50 selector mismatch interior blend",
  "source_t":"selector descriptor [+0x8] -> [esp+0x2c] -> 0xFF1850 parameter",
  "decoder":"0xFF1850 calls 0x4BBF70 twice with 0 and twice with t",
  "decoder_alpha":"0x4BBF70 writes the passed float directly into output.w",
  "c84_w":"third output.w of 0xFF1850 via 0xF9567D -> 0x424C60",
  "c85_w":"fourth output.w of 0xFF1850 via 0xF956F4 -> 0x424D70",
  "equation":"PTDE c84.w == c85.w == t for authenticated interior blend branch",
  "rgb_separate":"PTDE c84/c85 RGB scaled by [ebp+0x14]; .w untouched",
  "stable":"0xFF1600 zeroes both associated .w values",
  "still_open":["DSR source.beta semantic equivalence to PTDE selector t",
       "actual PTDE GPU values on matched draw","c110.w and k135",
       "material/texture/probe operator and PTDE pixels"],
  "binary_pin_count":len(PINS)}
if __name__=="__main__":
 ap=argparse.ArgumentParser()
 ap.add_argument("--exe",type=Path,required=True)
 ap.add_argument("--out",type=Path,required=True)
 ap.add_argument("--falsify",action="store_true")
 a=ap.parse_args()
 b=a.exe.read_bytes();r=verify(b)
 if a.falsify:
  for name,(va,_) in PINS.items():
   edited=bytearray(b);edited[off(b,va)]^=1
   try:verify(edited,False)
   except ValueError:pass
   else:raise RuntimeError("mutant passed: "+name)
  r["mutations_rejected"]=len(PINS)
  print(f"V273_PTDE_C84_C85_COMMON_BETA_RE_PASS pins={len(PINS)} mutants_rejected={len(PINS)}")
 a.out.write_text(json.dumps(r,indent=2,sort_keys=True)+"\n",encoding="utf8")
