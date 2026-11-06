#!/usr/bin/env python3
"""V443 exact preimage/ABI contract for the existing packer hook observer.
All reads are from an unmodified on-disk PE, no injection/writes.
"""
import hashlib,struct,json,sys,pathlib
EXPECTED_SHA='a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b'
CALLS={
 0x1c69fe:(0x563b80,5),0x1c6a97:(0x563b80,5),0x1c6bc5:(0x563c30,5),
 0x1c6d55:(0x563b80,6),0x1c6dee:(0x563b80,6),0x1c6f4a:(0x563c30,6),
 0x220632:(0x1c0c10,0),0x220639:(0x1c0be0,0),0x2167ba:(0x1c0be0,0),
}
# Exact original program bytes, not generic numeric-offset matching.
PREIMAGES={
 0x1c6939:bytes.fromhex('4d 8b f1'), # mov r14,r9
 0x1c6c79:bytes.fromhex('4d 8b f9'), # mov r15,r9
}

def pe_rva_to_file(b,rva):
 pe=struct.unpack_from('<I',b,0x3c)[0]
 assert b[pe:pe+4]==b'PE\x00\x00'
 section_count=struct.unpack_from('<H',b,pe+6)[0]
 opt_size=struct.unpack_from('<H',b,pe+20)[0]
 s0=pe+24+opt_size
 for i in range(section_count):
  o=s0+40*i
  virtual_size,va,raw_size,raw_off=struct.unpack_from('<IIII',b,o+8)
  if va<=rva<va+max(raw_size,virtual_size):
   off=raw_off+(rva-va)
   if off>=len(b):raise ValueError('RVA extends past file')
   return off
 raise ValueError(f'RVA not mapped: {rva:x}')

def at(b,rva,n):
 off=pe_rva_to_file(b,rva)
 return b[off:off+n]

def target(b,rva):
 s=at(b,rva,5)
 assert s[0]==0xe8, f'not rel32 CALL at {rva:x}'
 return rva+5+struct.unpack('<i',s[1:5])[0]

def verify(b):
 for a,(want,kind) in CALLS.items():
  assert target(b,a)==want,(hex(a),hex(target(b,a)),hex(want))
 for a,want in PREIMAGES.items():
  assert at(b,a,len(want))==want,(hex(a),at(b,a,len(want)).hex(),want.hex())
 # Both builder returns MUST continue to the original emitters.
 assert target(b,0x220639)==0x1c0be0
 assert target(b,0x220632)==0x1c0c10
 # Original DP vtable in constructor and actor vtable in protected image.
 assert at(b,0x1d74a6,7).startswith(bytes.fromhex('48 8d 05'))
 for vt in [0x12E5C08,0x12ECA10,0x12EBFE8]:
  assert len(at(b,vt,8))==8
 return {'status':'PASS','calls':len(CALLS),'native_packer_calls':6,
  'default_builder_calls':2,'alt_builder_calls':1,
  'emitter_register_preimages':len(PREIMAGES),
  'observed_native_producers':'0x563B80/0x563C30',
  'actor_identity':'NOT_RUNTIME_CERTIFIED',
  'builder_packet':'STATIC_CAPABILITY_ONLY','pixel':'OPEN'}

def main():
 d=pathlib.Path(sys.argv[1]).read_bytes()
 sha=hashlib.sha256(d).hexdigest()
 assert sha==EXPECTED_SHA,sha
 result=verify(d)
 # Memory-only mutation falsifiers, never modify original EXE.
 for a in CALLS:
  m=bytearray(d);m[pe_rva_to_file(m,a)]=0x90
  try:verify(m)
  except AssertionError:pass
  else:raise AssertionError(f'mutation undetected {a:x}')
 for a in PREIMAGES:
  m=bytearray(d);m[pe_rva_to_file(m,a)]^=1
  try:verify(m)
  except AssertionError:pass
  else:raise AssertionError(f'mutation undetected {a:x}')
 result.update({'sha256':sha,'mutation_negative_tests':len(CALLS)+len(PREIMAGES),
   'no_exe_file_writes':True})
 print(json.dumps(result,indent=2))
 if len(sys.argv)>2:pathlib.Path(sys.argv[2]).write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
