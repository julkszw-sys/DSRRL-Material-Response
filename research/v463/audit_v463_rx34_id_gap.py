#!/usr/bin/env python3
"""SHA locked V438 kind16 original native draw telemetry identity audit."""
import argparse,csv,collections,hashlib,json
from pathlib import Path
SHA='744d3af90a4f99eff8e7d22d8e78c2c6ced2e09141bda3ea0a23906d3e7d4329'
ENTITIES={0x2189d9f0,0x218d2070,0x218d25d0,0x218d2b30,0x218d4b70,0x218d50d0}
def audit(p):
 h=hashlib.sha256(p.read_bytes()).hexdigest();assert h==SHA
 rows=collections.defaultdict(list)
 with p.open(newline='',encoding='utf8') as f:
  for r in csv.reader(f):
   if len(r)!=29 or r[4]!='16':continue
   v=[int(x,16) for x in r[5:]]
   if v[1] in ENTITIES:rows[v[1]].append((int(r[2]),v))
 assert set(rows)==ENTITIES
 out=[]
 for e,rr in sorted(rows.items()):
  vs=[v for _,v in rr];ids={v[0] for v in vs};lb={v[6] for v in vs};flver={v[5] for v in vs}
  assert len(ids)>1 and len(lb)==1 and len(flver)==1
  out.append({'entity_ptr':hex(e),'kind16_events':len(vs),'v00_first':hex(vs[0][0]),'v00_last':hex(vs[-1][0]),'v00_distinct':len(ids),'v00_constant_handle':False,'packed_light':hex(next(iter(lb))),'row':next(iter(lb))&0xff,'flver':hex(next(iter(flver))),'first_frame':min(f for f,_ in rr),'last_frame':max(f for f,_ in rr)})
 return {'status':'ORIGINAL_CSV_PASS','sha256':h,'entities':out,'native_entity_plus_0x10':'NOT_CAPTURED','gameplay_actor':'NOT_CAPTURED','pixel':'OPEN','falsifier':'kind16 v00 changes within each of six entities and cannot be treated as a stable registry handle'}
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('csv',type=Path);x=a.parse_args();print(json.dumps(audit(x.csv),indent=2))
