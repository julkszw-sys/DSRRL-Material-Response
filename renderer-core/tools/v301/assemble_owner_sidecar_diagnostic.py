#!/usr/bin/env python3
"""Owner-only V301 diagnostic admission manifest/sidecar pack.
Does NOT claim PTDE pixel equivalence or install to a running game.
"""
import argparse,hashlib,json,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools/v288"))
from dxbc_legacy_checksum import valid
NAMES={rx:f"v286_R5_R11_R7_LerpRX{rx}.dxbc" for rx in (33,34,35)}
def sha(b):return hashlib.sha256(b).hexdigest()
def run(owner_final,new_output):
 owner=Path(owner_final)
 out=Path(new_output)
 if out.exists() or not out.parent.is_dir():raise ValueError("new output dir required")
 receipt=json.loads((owner/"V300_OWNER_LOCAL_RECEIPT.json").read_text())
 if receipt.get("schema")!="V300_OWNER_LOCAL_SHADER_TRIPLET_VERIFIED" or receipt.get("ready_for_runtime_approval") is not False:
  raise ValueError("unrecognized owner local receipt")
 manifest=json.loads((owner/"v297_postls/V297_OWNER_POST_LS_MANIFEST.json").read_text())
 if manifest.get("schema")!="DSRRL_V297_LERP_PTDE_POSTLS_V1" or manifest.get("runtime_bind") is not False:
  raise ValueError("unrecognized V297 candidate manifest")
 rows=manifest.get("receivers",[])
 if len(rows)!=3 or [r.get("rx") for r in rows]!=[33,34,35]:
  raise ValueError("not three exact RX candidates")
 checks={r.get("path"):r for r in receipt.get("files",[])}
 authorized=[]
 for row in rows:
  rx=row["rx"];name=NAMES[rx]
  if row.get("file")!=name:raise ValueError("invalid V297 filename")
  path=owner/"v297_postls"/name
  raw=path.read_bytes()
  key=("v297_postls/"+name)
  item=checks.get(key) or checks.get(key.replace("/", "\\"))
  if not item or sha(raw)!=row.get("sha256") or sha(raw)!=item.get("sha256") or len(raw)!=row.get("bytes") or not valid(raw):
   raise ValueError("owner candidate SHA/checksum receipt mismatch")
  authorized.append((rx,len(raw),sha(raw),raw))
 out.mkdir()
 try:
  include=out/"hemenvlerp_v301_owner_approval.hpp"
  lines=[
   "#pragma once",
   "// Generated for local V301 diagnostic only. PTDE pixel status OPEN.",
   "#include <array>",
   "#include <cstddef>",
   "#include <cstdint>",
   "namespace dsrrl::runtime::hemenvlerp_v301 {",
   "struct owner_rx { std::uint32_t receiver_id; std::size_t size; const char* sha256; };",
   "inline constexpr std::array<owner_rx,3> approved{{"
  ]
  for rx,size,hashval,_ in authorized:lines.append(f'  {{{rx}u,{size}u,"{hashval}"}},')
  lines.extend(["}};", "} // namespace", ""])
  include.write_text("\n".join(lines))
  sidecars=out/"HemEnvLerp";sidecars.mkdir()
  for rx,_,_,raw in authorized:
   (sidecars/f"v297_RX{rx}.dxbc").write_bytes(raw)
  (out/"V301_DIAGNOSTIC_ONLY.json").write_text(json.dumps({
     "phase":"diagnostic_not_pixel_certified","latest_hook_source":"PROTECTED",
     "fog":"stock_DSR","pixel":"OPEN",
     "sidecar_install_relative":"DSR/DSRRL/EnvSpec/HemEnvLerp",
     "approved":[{"rx":rx,"size":size,"sha256":h} for rx,size,h,_ in authorized]
  },indent=2)+"\n")
 except Exception:
  shutil.rmtree(out)
  raise
 return len(authorized)
if __name__=="__main__":
 p=argparse.ArgumentParser();p.add_argument("completed_v300_owner_dir");p.add_argument("new_v301_diagnostic_dir")
 args=p.parse_args()
 print("V301_OWNER_DIAGNOSTIC_APPROVAL_STAGED",run(args.completed_v300_owner_dir,args.new_v301_diagnostic_dir))
