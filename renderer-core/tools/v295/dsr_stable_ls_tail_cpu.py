"""Minimal DXBC SM5 CPU evaluator of an EXACT Stable scattering 15-op tail.
Only ADD/MAD/MUL/RSQ/DIV and strict D3D10 operand decoding are admitted.
Not an instruction-accurate GPU interpreter; no game bytes shipped.
"""
import math,struct
def decode_operand(w,p):
 t=w[p];p+=1
 count=t&3;select=(t>>2)&3;comps=(t>>4)&255
 kind=(t>>12)&255;dims=(t>>20)&3;mod=0
 if t>>31:
  ext=w[p];p+=1
  if (ext&63)!=1:raise ValueError("unrecognized operand extension")
  mod=(ext>>6)&255
  if mod not in (0,1):raise ValueError("unrecognized source modifier")
 idx=[]
 for j in range(dims):
  if ((t>>(22+3*j))&7)!=0:raise ValueError("relative/indexed operand not admitted")
  idx.append(w[p]);p+=1
 if kind==4:
  if dims:raise ValueError("immediate index")
  if count==1:idx=(struct.unpack("<f",struct.pack("<I",w[p]))[0],);p+=1
  elif count==2:idx=struct.unpack("<4f",struct.pack("<4I",*w[p:p+4]));p+=4
  else:raise ValueError("invalid immediate")
 else:idx=tuple(idx)
 return (kind,idx,count,select,comps,mod),p
def decode_instructions(w,start,end):
 p=start;out=[]
 while p<end:
  t=w[p];length=(t>>24)&127;op=t&2047
  n={0:3,0x32:4,0x38:3,0x44:2,0x0e:3}[op]
  k=p+1;args=[]
  for _ in range(n):
   arg,k=decode_operand(w,k);args.append(arg)
  if k!=p+length:raise ValueError("non-exact instruction boundary")
  out.append((p,op,args));p=k
 if p!=end or len(out)!=15:raise ValueError("scattering span")
 return out
def source(operand,state):
 kind,idx,count,mode,field,modifier=operand
 if kind==4:values=list(idx) if count==2 else [idx[0]]*4
 elif kind==0:values=state[("r",idx[0])]
 elif kind==8:values=state[("cb",idx[0],idx[1])]
 else:raise ValueError("non-mapped source")
 if count==1:values=[values[0]]*4
 elif mode==1:values=[values[(field>>(2*i))&3] for i in range(4)]
 elif mode==2:values=[values[field&3]]*4
 elif mode!=0:raise ValueError("unhandled swizzle")
 if modifier==1:values=[-x for x in values]
 return values
def execute(seq,state):
 state={key:list(val) for key,val in state.items()}
 for p,op,args in seq:
  v=[source(item,state) for item in args[1:]]
  if op==0:out=[a+b for a,b in zip(*v)]
  elif op==0x32:out=[a*b+c for a,b,c in zip(*v)]
  elif op==0x38:out=[a*b for a,b in zip(*v)]
  elif op==0x44:out=[1/math.sqrt(a) for a in v[0]]
  elif op==0x0e:out=[a/b for a,b in zip(*v)]
  else:raise ValueError("unexpected scattering op")
  kind,idx,count,mode,mask,modifier=args[0]
  if kind!=0 or count!=2 or mode!=0:raise ValueError("non-temp write")
  key=("r",idx[0]);previous=state.get(key,[0.]*4)
  state[key]=[out[i] if mask&(1<<i) else previous[i] for i in range(4)]
 return state
