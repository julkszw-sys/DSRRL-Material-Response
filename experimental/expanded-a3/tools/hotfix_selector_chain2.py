from pathlib import Path
import struct,hashlib,json
SRC=Path('/mnt/data/DSRRL_Material_Response_1.45_A3_UL_MONOLITH_RAX_HOTFIX1.addon64')
OUT=Path('/mnt/data/DSRRL_Material_Response_1.45_A3_UL_MONOLITH_SELECTOR_CHAIN_HOTFIX2.addon64')
AUD=Path('/mnt/data/DSRRL_Material_Response_1.45_A3_UL_MONOLITH_SELECTOR_CHAIN_HOTFIX2_AUDIT.json')
BASE_SHA='9009b75e28fdd427b7e3329176178f696a2af7ca675138abcd715deb576ca8d0'
CALLSITE=0x9697
INIT_SKIP=0x1C0F2E
CHAIN=0x1C1D40
LEGACY_OBSERVER=0x9380
A3_OBSERVER=0x1BFC50
EXPECTED_CALL=bytes.fromhex('e8e4fcffff')
EXPECTED_INIT=bytes.fromhex('4881c138980000')

def sha(b):return hashlib.sha256(b).hexdigest()
b=bytearray(SRC.read_bytes())
if sha(b)!=BASE_SHA: raise SystemExit('base sha mismatch')
lf=struct.unpack_from('<I',b,0x3c)[0]; n=struct.unpack_from('<H',b,lf+6)[0]; osz=struct.unpack_from('<H',b,lf+20)[0]; opt=lf+24; sec=opt+osz
secs=[]
for i in range(n):
 o=sec+i*40; name=bytes(b[o:o+8]).split(b'\0')[0].decode(); vs,va,rs,rp=struct.unpack_from('<IIII',b,o+8); secs.append((name,va,vs,rs,rp))
def off(rva):
 for _,va,vs,rs,rp in secs:
  if va<=rva<va+max(vs,rs): return rp+(rva-va)
 raise KeyError(hex(rva))
def relcall(src,target): return b'\xE8'+struct.pack('<i',target-(src+5))
def reljmp(src,target): return b'\xE9'+struct.pack('<i',target-(src+5))
if bytes(b[off(CALLSITE):off(CALLSITE)+5])!=EXPECTED_CALL: raise SystemExit('selector call preimage mismatch')
if bytes(b[off(INIT_SKIP):off(INIT_SKIP)+7])!=EXPECTED_INIT: raise SystemExit('selector install block preimage mismatch')
co=off(CHAIN)
if any(x!=0xCC for x in b[co:co+0x80]): raise SystemExit('selector chain cave not pristine')
stub=bytearray()
stub += bytes.fromhex('4883ec58')
stub += bytes.fromhex('4889542430')
stub += bytes.fromhex('4c89442438')
stub += bytes.fromhex('4c894c2440')
stub += bytes.fromhex('488b842480000000')
stub += bytes.fromhex('4889442448')
stub += bytes.fromhex('4889442420')
stub += bytes.fromhex('488b842488000000')
stub += bytes.fromhex('4889442428')
stub += relcall(CHAIN+len(stub),LEGACY_OBSERVER)
stub += bytes.fromhex('488b4c2430')
stub += bytes.fromhex('488b542438')
stub += bytes.fromhex('4c8b442440')
stub += bytes.fromhex('4c8b4c2448')
stub += relcall(CHAIN+len(stub),A3_OBSERVER)
stub += bytes.fromhex('4883c458c3')
if len(stub)>0x80: raise SystemExit(len(stub))
b[co:co+len(stub)]=stub
b[off(CALLSITE):off(CALLSITE)+5]=relcall(CALLSITE,CHAIN)
b[off(INIT_SKIP):off(INIT_SKIP)+7]=reljmp(INIT_SKIP,0x1C0F71)+b'\x90\x90'
checksum_off=opt+64
struct.pack_into('<I',b,checksum_off,0)
total=0;i=0
while i+1<len(b):
 word=0 if checksum_off<=i<checksum_off+4 else b[i]|(b[i+1]<<8)
 total=(total+word)&0xffffffff; total=(total&0xffff)+(total>>16); i+=2
if i<len(b): total=(total+b[i])&0xffffffff; total=(total&0xffff)+(total>>16)
total=(total&0xffff)+(total>>16); total=total+(total>>16)
cs=(total&0xffff)+len(b)
struct.pack_into('<I',b,checksum_off,cs)
OUT.write_bytes(b)
audit={
 'schema':'dsrrl.a3.selector_chain_hotfix2.v1','status':'CONSTRUCTION_PASS_DIAGNOSTIC',
 'base_sha256':BASE_SHA,'output_sha256':sha(b),'output_size':len(b),'pe_checksum':f'0x{cs:08X}',
 'mechanism':'reuse shipping 1.45 selector hook at EXE RVA 0x22BA20; chain its observer call to legacy resolver then A3 freshness observer; A3 no longer installs a second selector hook',
 'patches':[
  {'rva':hex(CALLSITE),'old':EXPECTED_CALL.hex(),'new':relcall(CALLSITE,CHAIN).hex(),'target_rva':hex(CHAIN)},
  {'rva':hex(INIT_SKIP),'old':EXPECTED_INIT.hex(),'new':(reljmp(INIT_SKIP,0x1C0F71)+b'\x90\x90').hex(),'purpose':'skip conflicting A3 selector install while preserving enabled path'},
  {'rva':hex(CHAIN),'old':'cc'*len(stub),'new':stub.hex(),'size':len(stub),'legacy_observer_rva':hex(LEGACY_OBSERVER),'a3_observer_rva':hex(A3_OBSERVER)}
 ],
 'invariants':['build141 RAX hotfix retained','shipping 1.45 selector resolver still executes first','no second hook at DarkSoulsRemastered.exe+0x22BA20','four LightBank producer hooks unchanged','draw callsites unchanged','EnvDiffuse OFF']
}
AUD.write_text(json.dumps(audit,indent=2)+'\n')
print(json.dumps(audit,indent=2))
