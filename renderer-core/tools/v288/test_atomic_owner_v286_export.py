"""V290 six independent atomic publication guards for owner-local V286 outputs."""
import hashlib,os,struct,subprocess,tempfile
from pathlib import Path
from atomic_owner_v286_export import RX,PIN,export
from dxbc_legacy_checksum import repair,valid

FAKE='''#!/usr/bin/env python3
import os,sys
from pathlib import Path
out=Path(sys.argv[7]);out.mkdir()
mode=os.environ.get("V288_FAKE_MODE","good")
for rx in (33,34,35):
 if mode=="partial" and rx==35:sys.exit(4)
 if mode=="missing" and rx==35:break
 b=bytearray(Path(os.environ["V288_FAKE_FIXTURE"]).read_bytes())
 if mode=="bad_checksum" and rx==35:b[40]^=1
 if mode=="bad_envelope" and rx==35:b[:4]=b"FAKE"
 (out/f"v286_R5_R11_R7_LerpRX{rx}.dxbc").write_bytes(b)
'''

def rejected(call,types):
    try:call()
    except types:return
    raise AssertionError("untrusted output accepted")

def run():
 with tempfile.TemporaryDirectory() as td:
  root=Path(td)
  files=[root/f"in{i}" for i in range(6)]
  for i,p in enumerate(files):p.write_bytes(bytes([i+1]))
  for i,rx in enumerate(RX):
   PIN[rx]=(hashlib.sha256(files[i].read_bytes()).hexdigest(),
            hashlib.sha256(files[i+3].read_bytes()).hexdigest())
  raw=bytearray(1024);raw[:4]=b"DXBC"
  struct.pack_into("<I",raw,24,len(raw))
  fixture=repair(bytes(raw))
  assert valid(fixture)
  source=root/"valid.dxbc";source.write_bytes(fixture)
  tool=root/"fake_v286"
  tool.write_text(FAKE);tool.chmod(0o755)
  out=root/"triplet"
  os.environ["V288_FAKE_FIXTURE"]=str(source)
  manifest=export(tool,files,out)
  assert len(manifest["receivers"])==3
  assert len(list(out.iterdir()))==4
  for row in manifest["receivers"]:
   assert valid((out/row["filename"]).read_bytes())
  rejected(lambda:export(tool,files,out),(FileExistsError,))
  out.rename(root/"saved")
  files[4].write_bytes(b"tampered")
  rejected(lambda:export(tool,files,out),(ValueError,))
  files[4].write_bytes(bytes([5]))
  assert not out.exists()
  for mode,exception in [("partial",subprocess.CalledProcessError),
                          ("missing",ValueError),
                          ("bad_checksum",ValueError),
                          ("bad_envelope",ValueError)]:
   os.environ["V288_FAKE_MODE"]=mode
   try:rejected(lambda:export(tool,files,out),(exception,))
   finally:os.environ.pop("V288_FAKE_MODE",None)
   assert not out.exists()
   assert not any(root.glob(".v288_*"))
  print("V290_ATOMIC_TRIPLET_CHECKSUM_TEST_PASS six guards + exact digest verification")

if __name__=="__main__":run()
