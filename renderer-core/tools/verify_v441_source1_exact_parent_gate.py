#!/usr/bin/env python3
"""V441 static original DSR vs addon Source1 policy boundary; read-only."""
import json,re,sys
from pathlib import Path
from verify_v440_native_channel import PE,verify
from verify_v441_nested_vfx_model_actor import check as vfx
def check(original,source,sha=True):
 if sha:verify(original)
 vfx(original,strict_sha=False)
 p=PE(original)
 gate=r'constexpr\s+std::uintptr_t\s+k_parent_return\s*=\s*0x220CF0u\s*;'
 assert len(re.findall(gate,source))==1
 for label,pc in (("k_ret_sel_1",0x20e019),("k_ret_sel_2",0x20eb7f),("k_ret_sel_3",0x20fb9e)):
  x=r'constexpr\s+std::uintptr_t\s+'+label+r'\s*=\s*0x'+format(pc,"x")+r'u\s*;'
  assert len(re.findall(x,source,re.I))==1
 assert "parent_return !=\n            g_source_base +\n                k_parent_return" in source
 assert "material.model_owner_instance !=\n            reinterpret_cast<std::uintptr_t>(owner)" in source
 assert "descriptor_owner != owner" in source
 assert p.read(0x220ced,3)==bytes.fromhex("ff5018")
 assert p.read(0x2172aa,3)==bytes.fromhex("ff5018")
 return {"static_S1_scope":"CONFIRMED_CURRENT_CODE",
 "S1_parent_vcall_return":"0x220cf0",
 "native_ASM_parent_vcall_return":"0x2172ad",
 "ASM_admitted_by_this_parent_gate":False,
 "S1_owner":"FrpgDrawContext, not typed actor generation",
 "VFX_nested_ModelDrawEntity":"CONFIRMED",
 "Source2_R43":"UNCHANGED",
 "patch_authorized":False,
 "reason_for_flicker":"OPEN"}
if __name__=="__main__":
 b=Path(sys.argv[1]).read_bytes()
 s=Path(sys.argv[2]).read_text(encoding="utf-8")
 print(json.dumps(check(b,s),indent=2))
