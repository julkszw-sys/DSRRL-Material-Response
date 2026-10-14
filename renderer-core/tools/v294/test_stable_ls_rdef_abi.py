#!/usr/bin/env python3
"""Synthetic RDEF contract tests: does not ship game shader bytes."""
import struct
from verify_stable_ls_rdef_abi import FIELDS,rdef
def put(b,p,n):struct.pack_into("<I",b,p,n)
def make():
 rows=list(FIELDS.items());strings=bytearray(b"$Globals\0");refs=[]
 for _,name in rows:refs.append(len(strings));strings+=name.encode()+b"\0"
 vc=len(rows);table=56;names=table+40*vc
 payload=bytearray(names+len(strings))
 put(payload,0,1);put(payload,4,32)
 put(payload,32,names);put(payload,36,vc);put(payload,40,table);put(payload,44,21*16)
 for i,((reg,_),off) in enumerate(zip(rows,refs)):
  pos=table+40*i
  put(payload,pos,names+off);put(payload,pos+4,16*reg);put(payload,pos+8,16)
 payload[names:]=strings
 result=bytearray(40)+b"RDEF"+struct.pack("<I",len(payload))+payload+b"ISGN"+struct.pack("<I",4)+bytes(4)
 result[:4]=b"DXBC";put(result,28,2);put(result,32,40);put(result,36,48+len(payload));put(result,24,len(result))
 return result
def rejected(b,reason):
 try:rdef(b)
 except (ValueError,UnicodeError,struct.error):return
 raise AssertionError("accepted "+reason)
def test():
 b=make();fields,size=rdef(b)
 assert fields==FIELDS and size==336
 # Mutation tests exercise envelope, chunk count, chunk tables, CB bounds,
 # Globals identity, duplicates, variable bounds and unknown field names.
 a=bytearray(b);a[:4]=b"FAKE";rejected(a,"magic")
 a=bytearray(b);put(a,24,100);rejected(a,"length")
 a=bytearray(b);put(a,28,99);rejected(a,"chunks")
 a=bytearray(b);put(a,32,999999);rejected(a,"chunk pointer")
 a=bytearray(b);put(a,40+4,999999);rejected(a,"chunk size")
 a=bytearray(b);put(a,48+32+8,999999);rejected(a,"variable table")
 a=bytearray(b);put(a,48+56+4,999999);rejected(a,"variable bounds")
 a=bytearray(b);a[48+56+40*9+9]^=1
 fields,_=rdef(a);assert fields.get(12)!="gFC_FogCol"
 a=bytearray(b);put(a,48+56+40+4,12*16);rejected(a,"duplicate register")
 # Identity gate denies all synthetic shaders regardless of structurally valid RDEF.
 from verify_stable_ls_rdef_abi import audit
 rejected_identity=False
 try:audit(bytes(b),33)
 except ValueError:rejected_identity=True
 assert rejected_identity
 print("V294_RDEF_STRUCTURAL_GUARDS_10_CASES_PASS")
if __name__=="__main__":test()
