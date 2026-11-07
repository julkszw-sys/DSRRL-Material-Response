#!/usr/bin/env python3
"""Offline observer audit V448; source ticket and runtime join are not pixels.
Reject partial CSV completeness for timing claims; never output bridge PASS.
"""
import collections,csv,hashlib,json,pathlib,sys
V448_SELECTOR=18
V448_ISSUED=19
VERDICT={
 0:'no_selector',1:'no_issued_builder',2:'missing_packer',3:'source_seq_mismatch',
 4:'context_mismatch',5:'source_actor_mismatch',6:'source_packet_mismatch',
 7:'source_model_mismatch',8:'source_ab_mismatch',9:'selector_issue_mismatch',
 10:'row_mismatch',11:'live_builder_chain_only_generation_open'
}
def get_events(path):
    with path.open(encoding='utf-8-sig',newline='') as f:
        for row in csv.reader(f):
            if len(row)<29 or row[0]=='seq' or row[0].startswith('#'):continue
            try:
                yield {'seq':int(row[0]),'qpc':int(row[1]),'frame':int(row[2]),
                       'tid':int(row[3]),'kind':int(row[4]),
                       'v':[int(x,16) for x in row[5:29]]}
            except ValueError:continue

def audit(path):
    selectors=[];issued=[]
    for event in get_events(path):
        if event['kind']==V448_SELECTOR:selectors.append(event)
        elif event['kind']==V448_ISSUED:issued.append(event)
    status=collections.Counter(VERDICT.get(x['v'][3],f'UNKNOWN_{x["v"][3]}') for x in issued)
    f_status=collections.Counter(x['v'][11] for x in issued)
    cross=[];byctx=collections.defaultdict(lambda:collections.defaultdict(set))
    for x in issued:
        v=x['v'];ctx=v[1]
        if v[17]:byctx[ctx][v[17]].add(v[2])
        if v[3] in (3,4,5,6,7,8,9,10) and len(cross)<64:
            cross.append({'frame':x['frame'],'qpc':x['qpc'],'tid':x['tid'],
               'native_sequence':v[0],'row':v[2],'verdict':VERDICT.get(v[3],str(v[3])),
               'selector_actor':hex(v[6]),'issued_actor':hex(v[13]),
               'producer_actor':hex(v[17]),'context':hex(ctx),
               'producer_packet':hex(v[18]),'issued_packet':hex(v[14])})
    shared={hex(ctx):{hex(entity):sorted(rows) for entity,rows in actors.items()}
      for ctx,actors in byctx.items() if len(actors)>1}
    last=path.read_bytes()[-1600:]
    return {'version':'V448','status':'DIAGNOSTIC_ONLY',
      'sha256_csv':hashlib.sha256(path.read_bytes()).hexdigest(),
      'capture_complete':'# complete=1' in last.decode(errors='ignore'),
      'selector_frame_samples':len(selectors),'rx34_issued_samples':len(issued),
      'verdicts':dict(status),'issued_stack_probe_codes':dict(f_status),
      'mismatches_bounded':cross,'shared_context_distinct_source_entities':shared,
      'actor_generation':'OPEN','bridge_activation':'OFF','pixel':'OPEN',
      'note':'Verdict11 is a candidate synchronized LIVE frame, not actor lifetime proof and not an active bridge.'}
if __name__=='__main__':
    if len(sys.argv)!=3:raise SystemExit('usage: analyze_v448_capture.py V438_*.csv output.json')
    p=pathlib.Path(sys.argv[1]);out=pathlib.Path(sys.argv[2]);res=audit(p)
    out.write_text(json.dumps(res,indent=2,ensure_ascii=False),encoding='utf-8')
    print(json.dumps({k:res[k] for k in ('selector_frame_samples','rx34_issued_samples','verdicts','capture_complete','actor_generation','bridge_activation','pixel')},indent=2))
