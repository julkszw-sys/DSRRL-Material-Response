#!/usr/bin/env python3
"""Atomic owner-local V286 final DXBC export; no shader assets shipped."""
import hashlib,json,os,shutil,struct,subprocess,tempfile
from pathlib import Path
from dxbc_legacy_checksum import valid as checksum_valid

RX=(33,34,35)
PIN={
 33:("b8a9f32010dd279ffae5dd3c787dceb4b06c696c3fe4007bd87ba8f3303e25ca","1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab"),
 34:("abb86881378803d3389bb42d63c97750865a62e23b1ede205bfb63795855ec30","8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97"),
 35:("023ad52a81399ff81066e55ff223ac9e4a90be7a64e21eb760ac5ba68f38c23b","b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70")
}
def sha(b):return hashlib.sha256(b).hexdigest()
def export(executable,inputs,destination):
 if len(inputs)!=6:raise ValueError("exact six owner-final shaders required")
 inputs=[Path(x) for x in inputs];destination=Path(destination).absolute()
 if destination.exists():raise FileExistsError("cannot overwrite output")
 if not destination.parent.is_dir():raise FileNotFoundError("output parent")
 for k,rx in enumerate(RX):
  for p,want in ((inputs[k],PIN[rx][0]),(inputs[k+3],PIN[rx][1])):
   if sha(p.read_bytes())!=want:raise ValueError("final SHA mismatch RX"+str(rx))
 root=Path(tempfile.mkdtemp(dir=destination.parent,prefix=".v288_"))
 try:
  stage=root/"triplet"
  subprocess.run([str(Path(executable).absolute()),*[str(x) for x in inputs],str(stage)],check=True,capture_output=True,text=True)
  names=[f"v286_R5_R11_R7_LerpRX{rx}.dxbc" for rx in RX]
  if not stage.is_dir() or {p.name for p in stage.iterdir()}!=set(names):
   raise ValueError("partial or unknown RX output")
  rows=[]
  for rx,name in zip(RX,names):
   raw=(stage/name).read_bytes()
   if len(raw)<1000 or raw[:4]!=b"DXBC" or struct.unpack_from("<I",raw,24)[0]!=len(raw) or not checksum_valid(raw):
    raise ValueError("invalid DXBC checksum or envelope RX"+str(rx))
   rows.append(dict(rx=rx,filename=name,size=len(raw),sha256=sha(raw),
                    stable_r17_sha256=PIN[rx][0],lerp_r19_sha256=PIN[rx][1]))
  manifest=dict(schema="DSRRL_V288_OWNER_FINAL_TRIPLET_V1",
                runtime_bind=False,pixel_status="OPEN",receivers=rows)
  (stage/"V288_OWNER_FINAL_MANIFEST.json").write_text(json.dumps(manifest,indent=2)+"\n")
  os.rename(stage,destination)
  return manifest
 finally:shutil.rmtree(root)
if __name__=="__main__":
 import argparse
 ap=argparse.ArgumentParser()
 ap.add_argument("v286_tool");ap.add_argument("inputs",nargs=6)
 ap.add_argument("new_output_dir")
 a=ap.parse_args()
 print(json.dumps(export(a.v286_tool,a.inputs,a.new_output_dir),indent=2))
