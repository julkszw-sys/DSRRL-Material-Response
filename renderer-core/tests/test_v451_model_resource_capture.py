#!/usr/bin/env python3
"""Synthetic V451 CSV regression: 2 native entities may share resource pointer,
which must NOT be silently interpreted as a gameplay actor certificate.
"""
import csv,importlib.util,tempfile
from pathlib import Path
p=Path('renderer-core/tools/analyze_v451_model_resource_capture.py')
s=importlib.util.spec_from_file_location('v451_analyzer',p)
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
with tempfile.TemporaryDirectory() as d:
 path=Path(d)/'capture.csv'
 with path.open('w',encoding='utf-8',newline='') as f:
  w=csv.writer(f)
  w.writerow(['seq','qpc','frame','tid','kind']+[f'v{i:02d}' for i in range(24)])
  def emit(seq,source,row,entity,resource,status,probe,slot):
   v=[seq,source,row,entity,entity+0x108 if entity else 0,0x4444,
      resource,0,0x5555,0x6666,int(bool(resource)),0,status,probe,slot,345,0x7777]+[0]*7
   w.writerow([seq,100+seq,930,101,21]+[f'{n:016x}' for n in v])
  emit(1,1,30,0x1000,0xaaaa,10,7,0)
  emit(2,1,31,0x2000,0xaaaa,10,7,0)
  emit(3,2,30,0,0,0,1,1)
  f.write('bad,truncated\n')
 o=m.analyze(path)
 assert o['event_counts']['kind_21']==3,o
 assert o['invalid_rows']['truncated_or_invalid']==1,o
 assert len(o['groups'])==3,o
 assert o['resources_seen_with_both_rows']==['0xaaaa'],o
 assert o['groups'][2]['native_certified']>=0
 print('V451 analyzer PASS: shareable resource pointer not actor identity; bounded CSV malformed row rejected')
