#!/usr/bin/env python3
"""V296 six original source PTDE/DSR Stable+Lerp LightScattering parity.
Owner source archives only; no copyrighted shader bytes shipped."""
import argparse,hashlib,json,struct,sys,zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"v293"))
from ptde_stable_ls_cpu_oracle import instructions,reg
P=lambda b:hashlib.sha256(b).hexdigest()
FOG="28c23273a5fb0b984b55fde72ee59546af4aa8646c55e1cb1bf8db426e9a4363"
TAIL="d91051cb7545eddfa64feda8cd024e1ecc94f0d383d6dfb79462c99f65233f14"
PTDE="ec37b28cfa68b451fb4e8b7bca471a2c31e466ea43150eb053f37164ea9a585d"
DSR={
("stable",33):("35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24",2682),
("stable",34):("d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837",2601),
("stable",35):("7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701",2254),
("lerp",33):("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",2764),
("lerp",34):("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",2683),
("lerp",35):("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",2336)}
FPO={
("stable",33):("FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo","0c6f2982bc36d56151791acd03caffb63358324d746f3dd408cfeda976fb2daf"),
("stable",34):("FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo","9e931f72ca7ebe7ae5bf68f23ff8a7ff3a818f314fd2578f47b1315e9331d9a6"),
("stable",35):("FRPG_Phn_DifSpcBmp__________HemEnv.fpo","77b40333a33f5543149a7b9ecd371da23af38febbd149b151e429d38a4c2b836"),
("lerp",33):("FRPG_Phn_DifSpcBmp______Csd_HemEnvLerp.fpo","ab3debbe2f90ccbcc7b70173dc87b4c98c6b6e126b1bd6d93b59685fd38ba72d"),
("lerp",34):("FRPG_Phn_DifSpcBmp______Sdw_HemEnvLerp.fpo","51451c962c42574a1335a5ebe0eadbc1bc7e3929bcf4af7684cc8229e81d875f"),
("lerp",35):("FRPG_Phn_DifSpcBmp__________HemEnvLerp.fpo","c3c911c0482d52ff91a1ea34cf330c95b15cf265561b87a649443dd8b31bee3e")}
POST=[0x0600002f,0x00100072,0,0x80100246,0x81,0,0x0a000038,0x00100072,0,0x00100246,0,0x00004002,0x400ccccd,0x400ccccd,0x400ccccd,0,0x05000019,0x00100072,0,0x00100246,0]
AFTER=[0x0400004c,0x0020800a,0,0x66]
def dsr_words(blob):
 if blob[:4]!=b"DXBC" or struct.unpack_from("<I",blob,24)[0]!=len(blob):raise ValueError("DXBC envelope")
 n=struct.unpack_from("<I",blob,28)[0]
 chunks=[struct.unpack_from("<I",blob,32+4*i)[0] for i in range(n)]
 shex=[i for i in chunks if blob[i:i+4]==b"SHEX"]
 if len(shex)!=1:raise ValueError("SHEX count")
 off=shex[0];sz=struct.unpack_from("<I",blob,off+4)[0]
 if sz%4 or off+8+sz>len(blob):raise ValueError("SHEX size")
 w=struct.unpack_from("<"+"I"*(sz//4),blob,off+8)
 if w[1]!=len(w):raise ValueError("SHEX size mismatch")
 return w
def dsr_check(blob,family,rx):
 want,pos=DSR[family,rx]
 if P(blob)!=want:raise ValueError("DSR original SHA mismatch")
 w=dsr_words(blob)
 part=lambda off,n:struct.pack("<"+"I"*n,*w[off:off+n])
 if w[pos:pos+21]!=POST or w[pos+21:pos+25]!=AFTER:raise ValueError("post-LS gamma shape")
 if P(part(pos-204,204))!=FOG or P(part(pos-129,129))!=TAIL:raise ValueError("Fog or LS changed")
 return {"dsr_sha256":want,"postgamma_word":pos}
def ptde_check(blob,family,rx):
 _,want=FPO[family,rx]
 if P(blob)!=want:raise ValueError("PTDE original SHA mismatch")
 s=list(instructions(blob))
 def uses(item,k):return any(reg(t)==(2,k) for t in item[2])
 first=next(i for i,x in enumerate(s) if uses(x,107))
 last=next(i for i in range(first,len(s)) if s[i][1]==18 and uses(s[i],110))
 fragment=s[first:last+1];flat=[w for x in fragment for w in x[3]]
 if len(fragment)!=19 or len(flat)!=76 or P(struct.pack("<76I",*flat))!=PTDE:raise ValueError("PTDE LS mismatch")
 if not all(x[0]<fragment[0][0] for x in s if uses(x,139)):raise ValueError("PTDE Fog overlaps scattering")
 return {"ptde_sha256":want,"ptde_scattering_word":fragment[0][0]}
def run(archive,original_dir):
 rows=[]
 with zipfile.ZipFile(archive) as z:
  for family in ("stable","lerp"):
   for rx in (33,34,35):
    d=Path(original_dir)/(f"RX{rx}"+("_lerp" if family=="lerp" else "")+"_stock.dxbc")
    source=dsr_check(d.read_bytes(),family,rx)
    suffix=FPO[family,rx][0]
    names=[n for n in z.namelist() if n.endswith(suffix)]
    if len(names)!=1:raise ValueError("PTDE source missing/ambiguous")
    rows.append({"family":family,"rx":rx,**source,**ptde_check(z.read(names[0]),family,rx)})
 return {"status":"SOURCE_EQUALITY_ONLY_PIXEL_OPEN","fog":"STOCK_DSR",
         "ptde_ls_sha256":PTDE,"dsr_ls_tail_sha256":TAIL,"dsr_fog_ls_sha256":FOG,"receivers":rows}
if __name__=="__main__":
 p=argparse.ArgumentParser();p.add_argument("owner_ptde_archive");p.add_argument("owner_stock_dxbc_directory");p.add_argument("--report")
 args=p.parse_args();result=run(args.owner_ptde_archive,args.owner_stock_dxbc_directory)
 if args.report:Path(args.report).write_text(json.dumps(result,indent=2)+"\n")
 print("V296_SIX_RX_STABLE_LERP_SHARED_LIGHTSCATTERING_SOURCE_PASS")
