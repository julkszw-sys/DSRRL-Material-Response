#!/usr/bin/env python3
"""Verify original PTDE PS3 Stable scattering without shipping game assets."""
import hashlib,json,struct,sys,zipfile
from pathlib import Path
PARTS={
 33:("FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo","0c6f2982bc36d56151791acd03caffb63358324d746f3dd408cfeda976fb2daf"),
 34:("FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo","9e931f72ca7ebe7ae5bf68f23ff8a7ff3a818f314fd2578f47b1315e9331d9a6"),
 35:("FRPG_Phn_DifSpcBmp__________HemEnv.fpo","77b40333a33f5543149a7b9ecd371da23af38febbd149b151e429d38a4c2b836")
}
DIGEST="ec37b28cfa68b451fb4e8b7bca471a2c31e466ea43150eb053f37164ea9a585d"
OPCODES=[4,4,7,6,5,5,5,4,14,14,14,2,5,5,5,5,5,4,18]
def sha(v):return hashlib.sha256(v).hexdigest()
def reg(word):return ((((word>>28)&7)|((word>>8)&24)),word&2047)
def scan(data):
 if data[:4]!=bytes((0,3,255,255)) or len(data)%4:raise ValueError("not PTDE PS3")
 w=struct.unpack("<"+"I"*(len(data)//4),data)
 i=1
 while i<len(w):
  t=w[i];op=t&65535
  if op==0xfffe:
   i+=1+((t>>16)&32767);continue
  if op==0xffff:return
  n=(t>>24)&15
  if not n or i+n>=len(w):raise ValueError("bad PS3 token length")
  yield i,op,w[i:i+n+1]
  i+=n+1
def verify(data,rx):
 if sha(data)!=PARTS[rx][1]:raise ValueError("untrusted owner PTDE shader")
 instructions=list(scan(data))
 def uses(idx,k):return any(reg(t)==(2,k) for t in idx[2][1:])
 fog=[i[0] for i in instructions if uses(i,139)]
 lead=next((j for j,i in enumerate(instructions) if uses(i,107)),None)
 if lead is None:raise ValueError("no PTDE source c107")
 end=next((j for j in range(lead,len(instructions))
           if instructions[j][1]==18 and uses(instructions[j],110)),None)
 if end is None:raise ValueError("no PTDE c110 blend")
 seq=instructions[lead:end+1]
 if [i[1] for i in seq]!=OPCODES:raise ValueError("PTDE LS topology differs")
 words=[v for i in seq for v in i[2]]
 blob=struct.pack("<"+"I"*len(words),*words)
 if len(words)!=76 or sha(blob)!=DIGEST:raise ValueError("PTDE LS exact tokens differ")
 if not fog or max(fog)>=seq[0][0]:raise ValueError("Fog and LS wrongly grouped")
 return {"rx":rx,"ptde_sha":sha(data),"ls_76_sha":sha(blob),
         "ls_start":seq[0][0],"ls_end":seq[-1][0]+len(seq[-1][2]),
         "fog_c139_before_ls":True,"ops":len(seq)}
def main():
 with zipfile.ZipFile(sys.argv[1]) as z:
  rows=[]
  for rx,(suffix,_) in PARTS.items():
   paths=[n for n in z.namelist() if n.endswith(suffix)]
   if len(paths)!=1:raise ValueError(f"ambiguous PTDE RX{rx}")
   rows.append(verify(z.read(paths[0]),rx))
 print(json.dumps(rows,indent=2))
 print("V291_PTDE_LS_EXACT_SOURCE_3RX_PASS; PTDE PIXEL NOT TESTED")
if __name__=="__main__":main()
