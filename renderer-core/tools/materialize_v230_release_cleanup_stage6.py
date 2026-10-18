#!/usr/bin/env python3
"""Remove first-hit INFO-only atomics in the generated renderer translation unit.
No renderer authority, D3D11 state, shader, resource or material gate is edited.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1].parent/'build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp'
s=p.read_text('utf8')
counts={
'g_mr_once_receiver_hit':1,
'g_mr_once_owner_join':1,
'g_mr_once_decision_active':1,
'g_mr_once_identity':1,
'g_mr_once_batch_ready':1,
'g_mr_once_draw_issued':2,
'g_lerp_once_draw_issued':2,
'g_pointlight_once_shader_ready':1,
'g_pointlight_once_sidecar_ready':1,
'g_pointlight_once_batch_ready':1,
'g_pointlight_once_applied':2,
'g_pointlight_active_logged':3
}
changes=0
for name,expected in counts.items():
 token=name+'.exchange('
 hits=[];i=0
 while True:
  pos=s.find(token,i)
  if pos<0:break
  hits.append(pos);i=pos+len(token)
 if len(hits)!=expected:raise RuntimeError('observer count drift '+name+': '+str(len(hits)))
 for pos in reversed(hits):
  start=s.rfind('if (',0,pos)
  if start<0 or pos-start>240:raise RuntimeError('not bounded if '+name)
  op=s.find('{',pos)
  if op<0 or op-pos>135:raise RuntimeError('no observer body '+name)
  depth=0
  for end in range(op,len(s)):
   if s[end]=='{':depth+=1
   elif s[end]=='}':
    depth-=1
    if depth==0:break
  else:raise RuntimeError('unbalanced observer '+name)
  body=s[op+1:end]
  if not any(x in body for x in ('((void)0)','log_pointlight_gate_once')):
   raise RuntimeError('not known INFO observer '+name)
  if any(x in body for x in ('return false;','return true;','release_prepared_island_batch(','prepared.ready =','dispatch.transaction =')):
   raise RuntimeError('operator code near observer '+name)
  following=s[end+1:].lstrip()
  if following.startswith('else'):raise RuntimeError('if-else potential semantic mutation '+name)
  s=s[:start]+'if constexpr (false) '+s[start:]
  changes+=1
p.write_text(s,'utf8')
print('V230_FIRST_HIT_INFO_ATOMICS_COMPILED_OUT',changes)
