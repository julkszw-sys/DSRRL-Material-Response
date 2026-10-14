#!/usr/bin/env python3
"""Extract all six SHA-pinned ORIGINAL owner DSR RX33-35 Stable and Lerp DXBC.
NO copyrighted binary data is committed or uploaded to GitHub Actions.
"""
import argparse,hashlib,json,struct,zipfile,zlib
from pathlib import Path
BINDER="DSR/shader/FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx"
OWNERS={
"stable33":("35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24",19856),
"stable34":("d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837",19556),
"stable35":("7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701",18060),
"lerp33":("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",20380),
"lerp34":("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",20080),
"lerp35":("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",18580)
}
def extract(archive,output):
 output=Path(output)
 if output.exists():raise FileExistsError("exact six original directory must be new")
 if not output.parent.is_dir():raise FileNotFoundError("output parent missing")
 with zipfile.ZipFile(archive) as src:
  dcx=src.read(BINDER)
 if not dcx.startswith(b"DCX\0"):raise ValueError("wrong DSR DCX")
 binder=zlib.decompress(dcx[76:])
 if not binder.startswith(b"BND3"):raise ValueError("wrong DSR binder")
 target={row[0]:name for name,row in OWNERS.items()}
 result={};i=0
 while (i:=binder.find(b"DXBC",i))!=-1:
  if i+32>len(binder):break
  size=struct.unpack_from("<I",binder,i+24)[0]
  if 128<size<256000 and i+size<=len(binder):
   shader=binder[i:i+size]
   h=hashlib.sha256(shader).hexdigest()
   if h in target:
    name=target[h]
    if name in result or size!=OWNERS[name][1]:
     raise ValueError(f"ambiguous or incorrect original {name}")
    result[name]=shader
  i+=4
 if set(result)!=set(OWNERS):raise ValueError("not all 6 owner original shaders found")
 output.mkdir()
 rows=[]
 for name,b in sorted(result.items()):
  fname=f"{name}.dxbc"
  (output/fname).write_bytes(b)
  rows.append(dict(name=name,file=fname,size=len(b),sha256=hashlib.sha256(b).hexdigest()))
 (output/"ORIGINAL_OWNER_SHA_MANIFEST.json").write_text(json.dumps(rows,indent=2)+"\n")
 return rows
if __name__=="__main__":
 p=argparse.ArgumentParser()
 p.add_argument("part02_owner_zip")
 p.add_argument("new_original_output_dir")
 a=p.parse_args()
 print(json.dumps(extract(a.part02_owner_zip,a.new_original_output_dir),indent=2))
