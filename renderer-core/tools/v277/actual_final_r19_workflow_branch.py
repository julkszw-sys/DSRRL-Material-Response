#!/usr/bin/env python3
"""V277 exact final R19 shader MaterialWorkflow IF_Z gate crosscheck."""
import hashlib,struct,json,sys
from pathlib import Path
FINAL={
33:("1d9fed8a55689e09bacf55a66a88f2c3474f7c3f9a5604fe5151b0506b06caab",1108,1319,1421,1372),
34:("8f86b1aec73b3b9ec77c10ce6de2308d0b0d4533a1430373ea92b297ee387f97",1017,1228,1330,1281),
35:("b5f94ccec4e01e7d98c4329c2b28aecfd65e103729be969a53f486b182c55b70",677,888,990,941)}
def run(rx,path):
 data=Path(path).read_bytes();sha,cond,els,endif,gamma=FINAL[rx]
 assert hashlib.sha256(data).hexdigest()==sha
 assert data[:4]==b"DXBC"
 n=struct.unpack_from("<I",data,28)[0];w=None
 for i in range(n):
  p=struct.unpack_from("<I",data,32+4*i)[0]
  if data[p:p+4]==b"SHEX":
   size=struct.unpack_from("<I",data,p+4)[0]
   w=struct.unpack_from("<%dI"%(size//4),data,p+8)
 assert w and w[1]==len(w)
 assert w[cond:cond+4]==(0x0400001f,0x0020800a,0,85)
 assert (w[cond]>>18)&1==0
 assert w[els]&2047==18 and w[endif]&2047==21
 assert els<gamma<endif and w[gamma]&2047==25
 return {"rx":rx,"sha256":sha,"material_gate":"IF_Z cb0[85].x",
         "else_second_gamma_if":"cb0[85].x != 0",
         "if_word":cond,"else_word":els,"endif_word":endif,
         "second_gamma_exp_word":gamma}
if __name__=="__main__":
 rows=[run(rx,p) for rx,p in zip(FINAL,sys.argv[1:4])]
 Path(sys.argv[4]).write_text(json.dumps(rows,indent=2)+"\n")
 print("V277_FINAL_R19_C85_IFZ_BRANCH_THREE_RX_PASS")
