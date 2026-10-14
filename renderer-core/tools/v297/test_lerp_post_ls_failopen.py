"""V297 strict fail-open: mismatched final input and malformed DXBC never publish."""
import json,struct,tempfile
from pathlib import Path
from patch_lerp_owner_post_ls_domain import patch,export
def rejected(call):
 try:call()
 except (ValueError,FileNotFoundError,OSError):return
 raise AssertionError("invalid owner shader or manifest accepted")
def test():
 for invalid in [b"",b"DXBC",b"DXBC"+bytes(1010),
                 b"FAKE"+bytes(1010)]:
  rejected(lambda b=invalid:patch(b))
 with tempfile.TemporaryDirectory() as d:
  d=Path(d)
  m=d/"manifest.json";out=d/"new_output"
  m.write_text(json.dumps({"schema":"DSRRL_V288_OWNER_FINAL_TRIPLET_V1",
                           "runtime_bind":True,"receivers":[]}))
  rejected(lambda:export(m,d,out));assert not out.exists()
  m.write_text(json.dumps({"schema":"DSRRL_V288_OWNER_FINAL_TRIPLET_V1",
                           "runtime_bind":False,"receivers":[]}))
  rejected(lambda:export(m,d,out));assert not out.exists()
  records=[{"rx":rx,"filename":f"v286_R5_R11_R7_LerpRX{rx}.dxbc",
            "stable_r17_sha256":"not_real","lerp_r19_sha256":"not_real",
            "size":1,"sha256":"not_real"} for rx in (33,34,35)]
  m.write_text(json.dumps({"schema":"DSRRL_V288_OWNER_FINAL_TRIPLET_V1",
                           "runtime_bind":False,"receivers":records}))
  rejected(lambda:export(m,d,out));assert not out.exists()
 print("V297_FAILOPEN_INVALID_INPUT_AND_MANIFEST_PASS")
if __name__=="__main__":test()
