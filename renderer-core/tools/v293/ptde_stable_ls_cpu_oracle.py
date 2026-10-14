"""CPU reference for the source-authenticated PTDE ps_3_0 Stable scattering cut.
Not a hardware-exact shader interpreter and NOT a PTDE pixel PASS."""
import hashlib,math,struct,sys,zipfile
NAMES={33:"FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo",34:"FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo",35:"FRPG_Phn_DifSpcBmp__________HemEnv.fpo"}
SHA={33:"0c6f2982bc36d56151791acd03caffb63358324d746f3dd408cfeda976fb2daf",34:"9e931f72ca7ebe7ae5bf68f23ff8a7ff3a818f314fd2578f47b1315e9331d9a6",35:"77b40333a33f5543149a7b9ecd371da23af38febbd149b151e429d38a4c2b836"}
OP=(4,4,7,6,5,5,5,4,14,14,14,2,5,5,5,5,5,4,18)
CUT="ec37b28cfa68b451fb4e8b7bca471a2c31e466ea43150eb053f37164ea9a585d"
def digest(b):return hashlib.sha256(b).hexdigest()
def f32(v):return struct.unpack("<f",struct.pack("<f",float(v)))[0]
def reg(t):return (((t>>28)&7)|((t>>8)&24),t&2047)
def instructions(data):
 if data[:4]!=bytes((0,3,255,255)) or len(data)%4:raise ValueError("PS3 header")
 w=struct.unpack("<"+"I"*(len(data)//4),data);i=1
 while i<len(w):
  t=w[i];op=t&65535
  if op==65534:i+=1+((t>>16)&32767);continue
  if op==65535:return
  n=(t>>24)&15
  if not n or i+n>=len(w):raise ValueError("invalid PS3 instruction")
  yield i,op,w[i+1:i+n+1],w[i:i+n+1]
  i+=n+1
def cut(data,rx):
 if digest(data)!=SHA[rx]:raise ValueError("untrusted PTDE FPO")
 ins=list(instructions(data))
 def use(ins,k):return any(reg(t)==(2,k) for t in ins[2])
 first=next(j for j,e in enumerate(ins) if use(e,107))
 last=next(j for j in range(first,len(ins)) if ins[j][1]==18 and use(ins[j],110))
 seq=ins[first:last+1]
 words=[x for e in seq for x in e[3]]
 b=struct.pack("<"+"I"*len(words),*words)
 if tuple(e[1] for e in seq)!=OP or len(words)!=76 or digest(b)!=CUT:
  raise ValueError("PTDE LS bytecode changed")
 if any(e[0]>=seq[0][0] for e in ins if use(e,139)):
  raise ValueError("stock Fog overlaps LS")
 return seq
def execute(seq,values):
 state={k:[f32(x) for x in v] for k,v in values.items()}
 def src(t):
  key=reg(t);w=(t>>16)&255
  v=[state[key][(w>>(2*i))&3] for i in range(4)]
  mode=(t>>24)&15
  if mode==1:v=[-x for x in v]
  elif mode:raise ValueError("unexpected PS3 modifier")
  return v
 for at,op,args,tokens in seq:
  dest=args[0];s=[src(x) for x in args[1:]]
  if op==4:v=[f32(s[0][i]*s[1][i]+s[2][i]) for i in range(4)]
  elif op==5:v=[f32(s[0][i]*s[1][i]) for i in range(4)]
  elif op==2:v=[f32(s[0][i]+s[1][i]) for i in range(4)]
  elif op==7:v=[f32(1/math.sqrt(x)) for x in s[0]]
  elif op==6:v=[f32(1/x) for x in s[0]]
  elif op==14:v=[f32(2**x) for x in s[0]]
  elif op==18:v=[f32(s[0][i]*s[1][i]+(1-s[0][i])*s[2][i]) for i in range(4)]
  else:raise ValueError("unexpected PS3 opcode")
  key=reg(dest);mask=(dest>>16)&15
  state.setdefault(key,[0.,0.,0.,0.])
  for i in range(4):
   if mask&(1<<i):state[key][i]=v[i]
 return tuple(state[(0,2)][:3])
def main(owner_zip):
 state={(0,0):(.2,.5,.7,.8),(0,1):(.15,.25,.35,.45),(0,2):(.05,.10,.15,.2),
        (0,3):(-.3,-.5,-.8,.2),(2,0):(2.,-1.,0.,1.),
        (2,105):(.6,.7,.8,.8),(2,106):(1.,1.,1.,1.),
        (2,107):(.3,1.5,.8,.6),(2,108):(.1,.2,.3,.4),
        (2,109):(.5,.6,.7,.8),(2,110):(.9,.8,.7,.35)}
 result=[]
 with zipfile.ZipFile(owner_zip) as z:
  for rx in (33,34,35):
   name=next(n for n in z.namelist() if n.endswith(NAMES[rx]))
   result.append(execute(cut(z.read(name),rx),state))
 assert result[0]==result[1]==result[2]
 assert all(math.isfinite(x) for x in result[0])
 print("V293_PTDE_STABLE_SCATTERING_CPU_REFERENCE_PASS",result[0])
if __name__=="__main__":main(sys.argv[1])
