#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
def remove_observer(path,anchor,marker):
 s=path.read_text('utf8')
 if s.count(anchor)!=1:raise RuntimeError('Observer drift '+anchor)
 a=s.index(anchor); op=s.index('{',a+len(anchor)); depth=0
 for i in range(op,len(s)):
  if s[i]=='{':depth+=1
  elif s[i]=='}':
   depth-=1
   if depth==0:break
 else:raise RuntimeError('Unbalanced '+anchor)
 body=s[op+1:i]
 if marker not in body or 'return false;' in body or 'prepared.ready =' in body:
  raise RuntimeError('Not observation-only '+anchor)
 path.write_text(s[:a]+'if constexpr (false) '+s[a:],'utf8')
p=r/'src/runtime/pmetal_envspec_draw_runtime.cpp'
for anchor,marker in [
 ('if (!g_source_frontier_logged.exchange(','[DSRRL ENVSPEC SOURCE]'),
 ('if (!g_resource_mode_logged.exchange(','[DSRRL PMETAL ENVSPEC RESOURCE]'),
 ('if (!g_native_envdiffuse_logged.exchange(','[DSRRL PMETAL R19 LERP]'),
 ('if (stable_envdiffuse_consumer_diag &&','[DSRRL PMETAL FULL PTDE HEMENV]'),
]:
 remove_observer(p,anchor,marker)
print('V230_STANDALONE_OBSERVERS_COMPILED_OUT=4')
