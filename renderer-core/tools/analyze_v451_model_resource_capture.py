#!/usr/bin/env python3
"""Analyze one V451 manually captured bounded CSV. Never infer gameplay actor
or pixel overlap from equal/unequal model-resource pointer addresses.
"""
import argparse,csv,json,collections,sys
from pathlib import Path
def analyze(filename):
 summary=collections.Counter();groups={};invalid=collections.Counter()
 with open(filename,encoding='utf-8-sig',newline='',errors='replace') as fh:
  reader=csv.reader(fh)
  for fields in reader:
   if not fields or fields[0].startswith('#') or fields[0]=='seq':continue
   if len(fields)<29:
    invalid['truncated_or_invalid']+=1;continue
   try:
    kind=int(fields[4]);frame=int(fields[2]);kind21=kind==21
    summary[f'kind_{kind}']+=1
    if not kind21:continue
    raw=[int(v,16) for v in fields[5:22]]
    seq,source,row,entity,dp,model,resource,controller,packet,stack_rbp,resource_valid,controller_valid,v450_status,probe,slot,route,context=raw
    if route!=345 or row not in (30,31):
     invalid['unexpected_route_or_row']+=1;continue
    identity=(entity,model,resource)
    g=groups.setdefault(identity,{'entity':hex(entity),'engine_model':hex(model),'model_resource':hex(resource),'controller':hex(controller),'rows':set(),'sources':set(),'slots':set(),'frames':set(),'events':0,'native_certified':0,'readable_resource':0,'native_seq_min':None,'native_seq_max':None})
    g['events']+=1;g['rows'].add(row);g['sources'].add(source);g['slots'].add(slot);g['frames'].add(frame)
    g['native_certified']+= int(v450_status==10 and probe==7)
    g['readable_resource']+=int(resource_valid==1)
    g['native_seq_min']=seq if g['native_seq_min'] is None else min(g['native_seq_min'],seq)
    g['native_seq_max']=seq if g['native_seq_max'] is None else max(g['native_seq_max'],seq)
   except (ValueError,IndexError):
    invalid['decode_error']+=1
 out=[]
 for g in groups.values():
  for k in ['rows','sources','slots','frames']:g[k]=sorted(g[k])
  g['frame_count']=len(g.pop('frames'))
  out.append(g)
 out.sort(key=lambda g:(g['rows'],g['entity'],g['model_resource']))
 by_entity=collections.defaultdict(set)
 by_resource=collections.defaultdict(set)
 for g in out:
  by_entity[g['entity']].add(g['model_resource'])
  by_resource[g['model_resource']].update(g['rows'])
 return {'format':'V451 model-resource only','status':'OBSERVATION_NOT_GAMEPLAY_ACTOR','event_counts':dict(summary),'invalid_rows':dict(invalid),'groups':out,'entities_with_multiple_resources':sorted(k for k,v in by_entity.items() if len(v)>1),'resources_seen_with_both_rows':sorted(k for k,v in by_resource.items() if len(v)>1 and k!='0x0'),'limits':['model_resource is a native lookup pointer, not gameplay actor ID','same address may be reused across generations','CPU frame is not GPU pixel coverage','source2 and source1 do not share certification','all rendered LightBank source/consumer code unchanged']}
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('csv',type=Path);ap.add_argument('--json',type=Path);a=ap.parse_args()
 result=analyze(a.csv)
 payload=json.dumps(result,ensure_ascii=False,indent=2)+'\n'
 if a.json:a.json.write_text(payload,encoding='utf-8')
 print(payload)
