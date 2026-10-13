#!/usr/bin/env python3
"""V272 owner-original PTDE DX9 c84/c85 constant BANK audit.

Read supplied original game corpus only, never distribute original DATA.exe
or fpo bytes. This proves CPU -> staged PS c84/c85 -> D3D9 *transport ABI*,
not the semantic CPU producer/actual runtime c84.w/c85.w values.
"""
import argparse,hashlib,json,struct,zipfile
from pathlib import Path
OWNER_EXE="PTDE/DATA.exe"
OWNER_PS=("PTDE/PTDE_FRPG_PHN_EXTRACTED/dvdbnd1_dcx_0000631D9390/"
          "FRPG_Phn_DifSpcBmp__________HemEnvLerp.fpo")
EXE_SHA="88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d"
PS_SHA="c3c911c0482d52ff91a1ea34cf330c95b15cf265561b87a649443dd8b31bee3e"
PS_FLUSH_VA=0x431760
PS_FLUSH_DIRECT_DISPATCH=(0x4317ec,0x43181a,0x43185e)
PS_VTBL_OFFSET=0x1b4

def sha(b):return hashlib.sha256(b).hexdigest()

def va_to_offset(exe,va):
 pe=struct.unpack_from("<I",exe,0x3c)[0]
 if exe[pe:pe+4]!=b"PE\0\0":raise ValueError("invalid PE")
 num_sections=struct.unpack_from("<H",exe,pe+6)[0]
 option_size=struct.unpack_from("<H",exe,pe+20)[0]
 image_base=struct.unpack_from("<I",exe,pe+52)[0]
 rva=va-image_base
 start=pe+24+option_size
 for i in range(num_sections):
  q=start+i*40
  virtual,rawsize,rawptr=struct.unpack_from("<III",exe,q+12)
  if virtual<=rva<virtual+rawsize:return rawptr+rva-virtual
 raise ValueError(f"PTDE VA {va:08x} absent")

def ctab_constants(ps):
 q=ps.find(b"CTAB")
 if q<0:raise ValueError("missing DX9 CTAB")
 q+=4
 sz,creator,version,n,ci,flags,target=struct.unpack_from("<7I",ps,q)
 if sz!=28 or version!=0xffff0300 or n<2 or n>120:raise ValueError("not original PS3 CTAB")
 found={}
 for i in range(n):
  at=q+ci+i*20
  name_off,regset,regindex,regcount,res,ti,dv=struct.unpack_from("<IHHHHII",ps,at)
  end=ps.find(b"\0",q+name_off)
  if end<0:raise ValueError("bad CTAB name")
  name=ps[q+name_off:end].decode("ascii")
  found[name]={"register_set":regset,"register_index":regindex,"register_count":regcount}
 for register in (84,85,110):
  name=f"DL_FREG_{register:03d}"
  if found.get(name)!={"register_set":2,"register_index":register,"register_count":1}:
   raise ValueError("different PTDE constant binding "+name)
 return found

def audit(path):
 with zipfile.ZipFile(path) as z:
  exe=z.read(OWNER_EXE);ps=z.read(OWNER_PS)
 if sha(exe)!=EXE_SHA or sha(ps)!=PS_SHA:raise ValueError("not pinned original PTDE evidence")
 regs=ctab_constants(ps)
 expected=b"\x8b\x81\xb4\x01\x00\x00\xff\xd0"
 for va in PS_FLUSH_DIRECT_DISPATCH:
  off=va_to_offset(exe,va)
  if exe[off:off+len(expected)]!=expected:
   raise ValueError("wrong PS constant F D3D9 upload dispatcher")
 # PTDE authored producer inputs are independently compared/copied by
 # two entity setters. Exact call sequence also shows distinct .w source
 # stack slots in the blended update path, before D3D9 upload.
 setter_and_copy_patterns={
  0x424d3d:"660fd68730010000", # c84 setter -> entity+0x130
  0x424e4d:"660fd68740010000", # c85 setter -> entity+0x140
  0xf941f7:"f30f7e8730010000", # c84 emitter reads entity+0x130
  0xf94205:"8d8e80050000",     # c84 emitter writes c84 at +0x580
  0xf941b7:"f30f7e8740010000", # c85 emitter reads entity+0x140
  0xf941c5:"8d8e90050000",     # c85 emitter writes c85 at +0x590
  0xf95165:"f30f1044245c",     # c84.w from independently computed stack+0x5C
  0xf951dc:"f30f1044246c",     # c85.w from independently computed stack+0x6C
 }
 for va,expected_hex in setter_and_copy_patterns.items():
  expected=bytes.fromhex(expected_hex)
  off=va_to_offset(exe,va)
  if exe[off:off+len(expected)]!=expected:
   raise ValueError("PTDE separate c84/c85 producer path changed")
 # Original flush loops across 8 DWORD dirty bitsets, 32 registers each,
 # gathering 16-byte float4 registers from a stage +0x40 cache.
 start=va_to_offset(exe,PS_FLUSH_VA)
 end=va_to_offset(exe,0x43186e)
 code=exe[start:end]
 required=(
  b"\x83\xfd\x20",         # cmp ebp,32
  b"\x83\xf8\x08",         # cmp eax,8
  b"\x83\xc7\x10",         # add edi,16
  b"\x8d\x54\x16\x40",    # lea edx,[esi+edx+0x40]
 )
 if not all(tag in code for tag in required):raise ValueError("PS dirty staging topology changed")
 def slot(c):
  return {"register":c,"stage":"PIXEL","relative_float4_from_bank":hex(0x40+c*16),
          "float4_stride_bytes":16,"component_w_offset":hex(0x40+c*16+12)}
 return {
  "schema":"dsrrl.v272.ptde_psconstant_float_upload.v1",
  "source":"original PTDE owner DATA.exe and original ps_3_0 fpo",
  "exe_sha256":sha(exe),"fpo_sha256":sha(ps),
  "shader_register_names":{k:regs[k] for k in ("DL_FREG_084","DL_FREG_085","DL_FREG_110")},
  "device_method":"IDirect3DDevice9::SetPixelShaderConstantF",
  "device_vtbl_offset":hex(PS_VTBL_OFFSET),
  "flush_va":hex(PS_FLUSH_VA),
  "direct_dispatch_vas":[hex(x) for x in PS_FLUSH_DIRECT_DISPATCH],
  "producer_entity_c84_setter_va":"0x424c60",
  "producer_entity_c85_setter_va":"0x424d70",
  "emitter_entity_c84_va":"0xf941e0",
  "emitter_entity_c85_va":"0xf941a0",
  "entity_c84_float4_offset":"0x130",
  "entity_c85_float4_offset":"0x140",
  "interpolation_source_w_slots":["[esp+0x5c]","[esp+0x6c]"],
  "separate_entity_update_paths_verified":True,
  "separate_runtime_values_verified":False,
  "dirty_mask_words":8,"bits_per_mask":32,
  "staging_float4_stride":16,
  "c84":slot(84),"c85":slot(85),"c110":slot(110),
  "producer_c84w":"OPEN","producer_c85w":"OPEN",
  "ptde_runtime_values":"UNMEASURED",
  "materialized_R19_bridge":"NOT_CHANGED",
  "ptde_pixel":"OPEN"}

if __name__=="__main__":
 ap=argparse.ArgumentParser()
 ap.add_argument("--part02",required=True,type=Path)
 ap.add_argument("--out",required=True,type=Path)
 a=ap.parse_args()
 result=audit(a.part02)
 a.out.parent.mkdir(parents=True,exist_ok=True)
 a.out.write_text(json.dumps(result,indent=2,sort_keys=True)+"\n",encoding="utf8")
 print("V272_PTDE_C84_C85_PIX_SHADER_UPLOAD_TRANSPORT_AUTHENTICATED_PASS")
 print("V272_PTDE_SEMANTIC_PRODUCERS_AND_VISIBLE_PIXELS_OPEN")
