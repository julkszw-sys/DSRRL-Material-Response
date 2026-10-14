#!/usr/bin/env python3
"""V297 Lerp V286 owner-final: isolated PTDE post-LS domain; never changes Fog.
Source only; game-provided SHA-verified final DXBC and V288 manifest required.
"""
import argparse,hashlib,json,os,shutil,struct,sys,tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"v288"))
from dxbc_legacy_checksum import valid,repair
H=lambda b:hashlib.sha256(b).hexdigest()
FOG="28c23273a5fb0b984b55fde72ee59546af4aa8646c55e1cb1bf8db426e9a4363"
TAIL="d91051cb7545eddfa64feda8cd024e1ecc94f0d383d6dfb79462c99f65233f14"
PIN={33:("b8a9f32010dd279ffae5dd3c787dceb4b06c696c3fe4007bd87ba8f3303e25ca","1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab"),
34:("abb86881378803d3389bb42d63c97750865a62e23b1ede205bfb63795855ec30","8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97"),
35:("023ad52a81399ff81066e55ff223ac9e4a90be7a64e21eb760ac5ba68f38c23b","b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70")}
POST=[0x0600002f,0x00100072,0,0x80100246,0x81,0,0x0a000038,0x00100072,0,0x00100246,0,0x00004002,0x400ccccd,0x400ccccd,0x400ccccd,0,0x05000019,0x00100072,0,0x00100246,0]
AFTER=[0x0400004c,0x0020800a,0,0x66]
def patch(source):
 if not valid(source):raise ValueError("untrusted DXBC checksum")
 u=lambda p:struct.unpack_from("<I",source,p)[0]
 n=u(28)
 if not 1<=n<=32 or 32+4*n>len(source):raise ValueError("chunk count")
 offs=[u(32+4*j) for j in range(n)]
 shex=[p for p in offs if p+8<=len(source) and source[p:p+4]==b"SHEX"]
 if len(shex)!=1:raise ValueError("missing/duplicate SHEX")
 pos=shex[0];size=u(pos+4)
 if size%4 or pos+8+size>len(source):raise ValueError("SHEX length")
 words=list(struct.unpack_from("<"+"I"*(size//4),source,pos+8))
 if words[1]!=len(words):raise ValueError("SHEX words invalid")
 hits=[]
 for at in range(204,len(words)-25):
  if words[at:at+21]!=POST or words[at+21:at+25]!=AFTER:continue
  if H(struct.pack("<204I",*words[at-204:at]))!=FOG:continue
  if H(struct.pack("<129I",*words[at-129:at]))!=TAIL:continue
  hits.append(at)
 if len(hits)!=1:raise ValueError("post-scattering cut absent or ambiguous")
 at=hits[0];off=pos+8+at*4
 out=bytearray(source)
 for j in range(21):struct.pack_into("<I",out,off+j*4,0x0100003a)
 out=repair(bytes(out))
 if not valid(out) or out[20:off]!=source[20:off] or out[off+84:]!=source[off+84:]:
  raise ValueError("checksum or nonlocal byte mutation")
 return out,at
def export(manifest_file,source_dir,output_dir):
 manifest=json.loads(Path(manifest_file).read_text())
 if manifest.get("schema")!="DSRRL_V288_OWNER_FINAL_TRIPLET_V1" or manifest.get("runtime_bind") is not False:
  raise ValueError("wrong owner manifest")
 records=manifest.get("receivers",[])
 if len(records)!=3 or [x.get("rx") for x in records]!=[33,34,35]:
  raise ValueError("incomplete owner RX triplet")
 output=Path(output_dir).absolute()
 if output.exists() or not output.parent.is_dir():raise ValueError("output exists or parent missing")
 validated=[]
 for x in records:
  rx=x["rx"];name=x.get("filename")
  if (x.get("stable_r17_sha256"),x.get("lerp_r19_sha256"))!=PIN[rx] or name!=f"v286_R5_R11_R7_LerpRX{rx}.dxbc":
   raise ValueError("unauthorized Stable/Lerp lineage")
  original=(Path(source_dir)/name).read_bytes()
  if H(original)!=x.get("sha256") or len(original)!=x.get("size"):
   raise ValueError("candidate size/full SHA mismatch")
  changed,at=patch(original)
  validated.append((rx,name,changed,at))
 tmp=Path(tempfile.mkdtemp(prefix=".v297_",dir=output.parent))
 try:
  stage=tmp/"triplet";stage.mkdir()
  rows=[]
  for rx,name,changed,at in validated:
   (stage/name).write_bytes(changed)
   rows.append({"rx":rx,"file":name,"bytes":len(changed),"sha256":H(changed),"post_ls_word":at})
  result={"schema":"DSRRL_V297_LERP_PTDE_POSTLS_V1",
          "carrier":"offline candidate only","fog":"stock DSR preserved",
          "runtime_bind":False,"pixel":"OPEN","receivers":rows}
  (stage/"V297_OWNER_POST_LS_MANIFEST.json").write_text(json.dumps(result,indent=2)+"\n")
  os.rename(stage,output)
  return result
 finally:shutil.rmtree(tmp)
if __name__=="__main__":
 p=argparse.ArgumentParser();p.add_argument("v288_owner_manifest");p.add_argument("v286_owner_dxbc_dir");p.add_argument("new_output_dir")
 a=p.parse_args();print(json.dumps(export(a.v288_owner_manifest,a.v286_owner_dxbc_dir,a.new_output_dir),indent=2))
