#!/usr/bin/env python3
from __future__ import annotations
import ast,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; GEN=ROOT/"tools"/"generate_ptde_flver_texture_semantics.py"; SRC=ROOT/"data"/"census"/"ptde_flver_texture_semantics_v1_source.json"
REQUIRED={"g_Diffuse","g_Bumpmap","g_DetailBumpmap","g_Specular","g_Lightmap","g_Diffuse_2","g_Bumpmap_2","g_Specular_2"}
def generator_bits():
 tree=ast.parse(GEN.read_text(encoding="utf-8"),filename=str(GEN))
 for node in tree.body:
  value=None
  if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=="BITS" for t in node.targets): value=node.value
  elif isinstance(node,ast.AnnAssign) and isinstance(node.target,ast.Name) and node.target.id=="BITS": value=node.value
  if value is not None:
   if not isinstance(value,ast.Dict): raise SystemExit("BITS is not a dict literal")
   keys=set()
   for key in value.keys:
    if not isinstance(key,ast.Constant) or not isinstance(key.value,str): raise SystemExit("BITS contains a non-string key")
    keys.add(key.value)
   return keys
 raise SystemExit("BITS assignment not found")
def main():
 bits=generator_bits(); missing=REQUIRED-bits
 if missing: raise SystemExit(f"missing texture semantic consumers: {sorted(missing)}")
 src=json.loads(SRC.read_text(encoding="utf-8"))
 if src.get("coverage_state")=="PARTIAL_SOURCE_COVERAGE" and (src.get("positive_use_only") is not True or src.get("absence_is_unknown") is not True): raise SystemExit("partial FLVER coverage must remain positive-only with absence UNKNOWN")
 if int(src.get("source",{}).get("scan_error_count",0))>0 and src.get("absence_is_unknown") is not True: raise SystemExit("scan errors forbid negative authority")
 observed=src.get("observed",{}).get("positive_semantic_identity_counts",{}); unknown=set(observed)-bits
 if unknown: raise SystemExit(f"source manifest reports semantics outside generator surface: {sorted(unknown)}")
 print(f"PASS texture semantic surface: supported={len(bits)} reported={len(observed)} coverage={src.get('coverage_state')} absence=UNKNOWN")
 return 0
if __name__=="__main__": raise SystemExit(main())
