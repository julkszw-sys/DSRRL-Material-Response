#!/usr/bin/env python3
"""Read-only, exact PTDE DATA.exe c135 upload audit."""
import hashlib,json,struct,sys
from pathlib import Path
SHA="88d1eec18eba2542c9c7e5e6903e89f9c075b20be5cd045ce6e568e7b22bea9d"
# PE32 static VA -> raw for this exact SHA-locked 2012 binary
OFF=lambda addr: addr-0x401000+0x400 if 0x401000<=addr<0x10fb1d6 else addr-0x10fc000+0xcfa600
UPLOAD=0x42b0e0
CTOR=0xf8fcd0
UP=bytes.fromhex("56 57 8b f9 8b 77 10 6a 01 81 c6 50 10 00 00 56 b8 87 00 00 00 e8 46 7c ff ff f3 0f 7e 47 60 8d 47 60 8d 8e b0 08 00 00 66 0f d6 01 f3 0f 7e 40 08 5f 66 0f d6 41 08 5e c3")
CT=bytes.fromhex("55 8b ec 83 e4 f0 83 ec 10 50 8b c6 e8 7f 72 03 00 c7 06 90 77 1c 01 66 0f 6f 05 20 5a 10 01 66 0f 7f 04 24 f3 0f 7e 04 24 66 0f d6 46 60 f3 0f 7e 44 24 08 6a 01 8d 4e 40 51 b8 87 00 00 00 66 0f d6 46 68 e8 27 30 49 ff")
def verify(blob,sha=True):
 if sha and hashlib.sha256(blob).hexdigest()!=SHA:raise ValueError("PTDE exe identity")
 if blob[OFF(UPLOAD):OFF(UPLOAD)+len(UP)]!=UP:raise ValueError("c135 uploader changed")
 if blob[OFF(CTOR):OFF(CTOR)+len(CT)]!=CT:raise ValueError("c135 ctor changed")
 if struct.unpack_from("<I",blob,OFF(0x11c7790)+32)[0]!=UPLOAD:raise ValueError("vtable 8")
 if struct.unpack_from("<4f",blob,OFF(0x1105a20))!=(0.,0.,0.,1.):raise ValueError("c135 seed")
 return True
if __name__=="__main__":
 b=Path(sys.argv[1]).read_bytes()
 assert verify(b)
 for address in (UPLOAD,CTOR):
  altered=bytearray(b)
  altered[OFF(address)+16]^=1
  try:verify(altered,False)
  except ValueError:pass
  else:raise AssertionError("must fail closed")
 print(json.dumps({"SHA256":SHA,"PTDE_upload":"0x42b0e0","register":"c135","producer_object_offset":"0x60","lanes":4,"ctor":"0xf8fcd0","initial_seed":[0,0,0,1],"checks":"PASS","dynamic_source":"OPEN","pixel":"UNVERIFIED"},indent=2))
