import hashlib,os,struct,tempfile
from pathlib import Path
from atomic_owner_v286_export import RX,PIN,export

def run():
 with tempfile.TemporaryDirectory() as root:
  root=Path(root)
  files=[root/f"in{i}" for i in range(6)]
  for i,p in enumerate(files):p.write_bytes(bytes([i+1]))
  for i,rx in enumerate(RX):
   PIN[rx]=(hashlib.sha256(files[i].read_bytes()).hexdigest(),
            hashlib.sha256(files[i+3].read_bytes()).hexdigest())
  prog=root/"fake_v286"
  prog.write_text("""#!/usr/bin/env python3
import os,sys,struct
from pathlib import Path
out=Path(sys.argv[7]);out.mkdir()
for rx in (33,34,35):
 if os.getenv('FAIL_AT_RX35') and rx==35:sys.exit(3)
 if os.getenv('MISS_RX35') and rx==35:break
 b=bytearray(1024);b[:4]=b'DXBC'
 struct.pack_into('<I',b,24,len(b))
 (out/f'v286_R5_R11_R7_LerpRX{rx}.dxbc').write_bytes(b)
""")
  prog.chmod(0o755)
  out=root/"result"
  assert len(export(prog,files,out)["receivers"])==3
  assert len(list(out.iterdir()))==4
  try:export(prog,files,out)
  except FileExistsError:pass
  else:raise AssertionError('overwrite allowed')
  out.rename(root/"saved")
  files[4].write_bytes(b"tampered")
  try:export(prog,files,out)
  except ValueError:pass
  else:raise AssertionError('sha tamper accepted')
  files[4].write_bytes(bytes([5]))
  for env in ('FAIL_AT_RX35','MISS_RX35'):
   os.environ[env]='1'
   try:export(prog,files,out)
   except (ValueError,Exception):pass
   else:raise AssertionError('partial publish allowed')
   finally:os.environ.pop(env)
   assert not out.exists()
  print("V288_ATOMIC_OWNER_SOURCE_TEST_PASS")
if __name__=="__main__":run()
