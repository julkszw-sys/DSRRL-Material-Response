#!/usr/bin/env python3
"""V472: offline V469 RX34 V418 journal class census, exact log SHA locked.
Does not establish actor ownership or PTDE pixel equivalence.
"""
import argparse,hashlib,re,json
from pathlib import Path
SHA='a39ee3342813244e097bf5ba7fa402d4aa879689fe7b2b57f9a6fb5031d5871c'
def audit(data):
 assert hashlib.sha256(data).hexdigest()==SHA,'unexpected V469 log: fail closed'
 rows=data.decode('utf8',errors='replace').splitlines()
 matches=[]
 for line in rows:
  if '[DSRRL V418 RX34 MODEL ISSUED JOIN]' not in line:continue
  d=dict(re.findall(r'\b([a-z_]+)=([^\s]+)',line))
  if d.get('route')=='345':matches.append(d)
 one=[v for v in matches if v.get('kind')=='1']
 two=[v for v in matches if v.get('kind')=='2']
 count=lambda rows,key,val:sum(v.get(key)==str(val) for v in rows)
 out=dict(
  status='CONFIRMED_RUNTIME_LOG_SCOPE',log_sha256=SHA,
  rx34_v418_messages=len(matches),source1=len(one),source2=len(two),
  source1_class2=count(one,'class',2),
  source1_exact_model=count(one,'model_verified',1),
  source1_fg_reciprocal=count(one,'parent_reciprocal',1),
  source1_registry_readable=count(one,'registry_readable',1),
  any_exact_renderable_join=count(matches,'exact_renderable_join',1),
  source1_row30=count(one,'row',30),source1_row31=count(one,'row',31),
  actor_pixel='OPEN',ptde_pixel='OPEN')
 assert (out['rx34_v418_messages'],out['source1'],out['source2'])==(317,299,18)
 assert (out['source1_class2'],out['source1_exact_model'])==(299,299)
 assert out['source1_fg_reciprocal']==0 and out['source1_registry_readable']==0
 assert out['any_exact_renderable_join']==0
 return out
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('log',type=Path)
 a=parser.parse_args()
 print(json.dumps(audit(a.log.read_bytes()),indent=2))
