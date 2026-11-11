#!/usr/bin/env python3
"""V461 exact V438 session-local grouping of SOURCE1 issued native RX34.
Read-only original file; no actor ownership inference or pixel attribution.
"""
import argparse,collections,csv,hashlib,itertools,json
from pathlib import Path
SHA='744d3af90a4f99eff8e7d22d8e78c2c6ced2e09141bda3ea0a23906d3e7d4329'
EXPECTED={
 0x2189d9f0:(30,30,962,976),0x218d2070:(30,36,1001,1018),
 0x218d25d0:(31,78,930,970),0x218d2b30:(31,80,930,971),
 0x218d4b70:(31,34,1004,1020),0x218d50d0:(31,36,1003,1020)}
def analyze(path,strict=True):
 raw=path.read_bytes();hashv=hashlib.sha256(raw).hexdigest()
 if strict:assert hashv==SHA,'V438 CSV digest mismatch'
 frames=collections.defaultdict(set); counts=collections.Counter(); pairs=collections.defaultdict(set)
 all_rows=collections.Counter(); issues=collections.Counter(); frameslots=collections.defaultdict(set)
 with path.open('r',encoding='utf-8-sig',errors='replace',newline='') as f:
  for line in csv.reader(f):
   if not line or line[0]=='seq' or line[0].startswith('#'):continue
   if len(line)!=29:continue
   typ=int(line[4]);v=[int(s,16) for s in line[5:29]];frame=int(line[2])
   if typ!=17 or v[14]!=34 or v[15]!=345 or v[2] not in (30,31):continue
   if v[13]!=1:
    issues['non_Source1']+=1;continue
   ent=v[4];row=v[2]
   counts[(ent,row)]+=1;frames[ent].add(frame);pairs[frame].add(ent)
   all_rows[row]+=1;frameslots[frame].add(row)
 summary=[]
 for e in sorted(frames):
  rows={r:c for (x,r),c in counts.items() if x==e}
  assert len(rows)==1,('changed entity row',hex(e))
  row=next(iter(rows))
  summary.append({'entity':hex(e),'row':row,'issued_calls':sum(rows.values()),
   'distinct_CPU_frame_buckets':len(frames[e]),'first_frame':min(frames[e]),'last_frame':max(frames[e])})
 if strict:
  assert len(summary)==6 and sum(counts.values())==294
  assert all_rows=={30:66,31:228}
  assert len(frameslots)==67 and issues=={'non_Source1':12}
  for e,(row,c,start,end) in EXPECTED.items():
   x=next(x for x in summary if x['entity']==hex(e))
   assert (x['row'],x['issued_calls'],x['first_frame'],x['last_frame'])==(row,c,start,end)
  assert set(frameslots)==set(range(930,977))|set(range(1001,1021))
 concurrency=collections.Counter(len(ent) for ent in pairs.values())
 cooccurrence=[]
 for a,b in itertools.combinations(sorted(frames),2):
  inter=frames[a]&frames[b]
  if inter:cooccurrence.append({'entity_A':hex(a),'entity_B':hex(b),
    'common_frame_buckets':len(inter),'jaccard':round(len(inter)/len(frames[a]|frames[b]),4)})
 mixed=sum(len(x)==2 for x in frameslots.values())
 return {'version':'V461','status':'ORIGINAL_CSV_SHA_LOCKED_PASS' if strict else 'UNVERIFIED_INPUT',
    'csv_sha256':hashv,'Source1_RX34_issued':sum(counts.values()),'Source2_uncertified':issues['non_Source1'],
    'issued_row_counts':dict(all_rows),'native_render_entities':len(summary),'native_entity_groups':summary,
    'total_CPU_frame_buckets':len(frameslots),'temporal_cohorts':[
       {'first_frame':930,'last_frame':976,'frame_buckets':47,'render_entities':3,'rows':{'30':1,'31':2}},
       {'first_frame':1001,'last_frame':1020,'frame_buckets':20,'render_entities':3,'rows':{'30':1,'31':2}}],
    'max_distinct_render_entities_in_one_CPU_frame':max(concurrency),
    'concurrent_frame_buckets_by_entity_count':dict(concurrency),
    'mixed_row30_row31_frame_buckets':mixed,'all_three_concurrent_frame_buckets':concurrency[3],
    'cooccurrence':cooccurrence,
    'actor_count':'NOT_DETERMINED','separate_actors_per_entity':'NOT_PROVEN',
    'actor_player_vs_npc_owner':'OPEN','per_entity_model_registration_handle':'NOT_CAPTURED',
    'GPU_pixel_overlap':'OPEN','PTDE_pixel':'OPEN'}
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('csv',type=Path);ap.add_argument('--json',type=Path)
 x=ap.parse_args();v=analyze(x.csv);text=json.dumps(v,indent=2,ensure_ascii=False)+'\n'
 if x.json:x.json.write_text(text,encoding='utf8')
 print(text)
