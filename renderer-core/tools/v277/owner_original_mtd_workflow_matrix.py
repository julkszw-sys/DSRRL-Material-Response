#!/usr/bin/env python3
"""V277 original owner MTD workflow matrix; do not publish game bytes."""
import hashlib,zipfile,zlib,struct,json,sys
from pathlib import Path
SHA={"DSR/mtd/Mtd.mtdbnd.dcx":"7c5a0952c1016a3efa3c6a7180bc60af81ccf7b2a4685c3c89028eda78c2d8b5",
"DSR/mtd/MtdPatch.mtdbnd.dcx":"45c1227cb8b8cbb6a55ee2b21c188e9ccff68c59c3764ee5d070755576bf8864"}
def sha(b):return hashlib.sha256(b).hexdigest()
def records(raw,source):
 assert raw[:4]==b"DCX\0"
 b=zlib.decompress(raw[76:]);assert b[:10]==b"BND307D7R6"
 count=struct.unpack_from("<I",b,16)[0]
 for i in range(count):
  f,size,off,idx,nptr,_=struct.unpack_from("<6I",b,32+24*i)
  assert f==64 and off+size<=len(b)
  name=b[nptr:b.index(b"\0",nptr)].decode("ascii")
  if not name.startswith(("P_Metal[DSB]","P_Leather[DSB]","P_DullLeather[DSB]")):continue
  data=b[off:off+size];p=data.find(b"g_MaterialWorkflow")
  value=0
  if p>=0:
   assert data[p+24:p+27]==b"int"
   value=struct.unpack_from("<I",data,p+56)[0]
   assert value in (0,1)
  yield {"name":name,"sha256":sha(data),"mt_db":source,"entry":i,
         "workflow_authored_default":value,"has_workflow_property":p>=0}
def run(path):
 with zipfile.ZipFile(path) as z:
  allrows=[]
  for name,digest in SHA.items():
   b=z.read(name);assert sha(b)==digest
   allrows.extend(records(b,name))
 assert len(allrows)>=16
 assert all("MtdPatch" not in r["mt_db"] for r in allrows)
 names={}
 for row in allrows:names.setdefault(row["name"],set()).add(row["sha256"])
 return {"source_sha256":SHA,"materials":allrows,
  "duplicate_names_different_sha":{k:sorted(v) for k,v in names.items() if len(v)>1},
  "caveat":"MTD authored default is not proof of runtime cb0[85] value"}
if __name__=="__main__":
 result=run(Path(sys.argv[1]));Path(sys.argv[2]).write_text(json.dumps(result,indent=2)+"\n")
 print("V277_SHA_PINNED_ORIGINAL_MTD_BRANCH_DEFAULTS_PASS",len(result["materials"]))
