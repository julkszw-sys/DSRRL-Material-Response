#!/usr/bin/env python3
"""Read-only V411 ReShade log analyzer; no native actor ownership inference."""
import argparse, collections, json, re
from pathlib import Path
KV=re.compile(r'([A-Za-z_][A-Za-z_0-9]*)=([^\s]+)')
STAMP='[DSRRL V411 OWNER SEQ STAMP]'
DRAW='[DSRRL V411 RX34 OWNER SEQ DRAW]'
STATUS={0:'no_record',1:'matched_snapshot',2:'producer_changed',
        3:'epoch_changed',4:'context_tuple_changed',
        5:'capacity_miss',6:'history_evicted',
        7:'ambiguous_context_identity',8:'context_unreadable',
        9:'native_manager_disagreement',255:'source2_not_applicable'}
def events(text):
    for n,line in enumerate(text.splitlines(),1):
        kind='stamp' if STAMP in line else 'draw' if DRAW in line else None
        if kind: yield kind,{'line':n,**dict(KV.findall(line))}
def classify_draw(d):
    if d.get('kind')=='2':return 'protected_source2_no_native_owner_join'
    if d.get('kind')!='1':return 'other_source_not_certified'
    if d.get('native_producer_seq') in ('0',None):
        return 'source1_missing_native_sequence'
    status=int(d.get('stamp_state','0'))
    if status!=1:return 'source1_'+STATUS.get(status,'unknown_stamp')
    if d.get('manager_matches')!='1':
        return 'source1_context_manager_not_confirmed'
    if d.get('historical_snapshot')=='1':
        # A newer native producer can legitimately update the *current*
        # cache before a previously prepared RX34 draw is issued.
        # Neither state proves native actor lifetime or GPU command order.
        if d.get('cache_seq_matches')!='1':
            return 'source1_historical_stamp_cache_advanced_actor_open'
        return 'source1_historical_stamp_consistent_actor_open'
    if d.get('cache_readback')!='1':return 'source1_cache_readback_missing'
    if d.get('cache_seq_matches')!='1':return 'source1_cache_sequence_mismatch'
    return 'source1_snapshot_consistent_native_actor_open'
def analyze(text):
    stamps=[];draws=[]
    for kind,event in events(text):
        if kind=='stamp':stamps.append(event)
        else:
            event['classification']=classify_draw(event)
            draws.append(event)
    by_key=collections.defaultdict(list)
    for s in stamps:
        if s.get('journaled')=='1':
            by_key[(s.get('owner'),s.get('seq'))].append(s)
    for d in draws:
        if d.get('kind')!='1':
            d['logged_stamp_count']=0;d['source_stamp_match']='NOT_APPLICABLE'
            continue
        matched=[s for s in by_key.get(
            (d.get('owner'),d.get('native_producer_seq')),[])
            if s.get('epoch')==d.get('serial')]
        d['logged_stamp_count']=len(matched)
        d['source_stamp_match']=(
            'SAMPLED_OWNER_SEQUENCE_EPOCH_MATCH' if matched
            else 'UNSAMPLED_OR_NO_MATCH')
    return {'status':'DIAGNOSTIC_SAMPLED_LOG_ONLY',
       'stamp_samples':len(stamps),'gpu_draw_samples':len(draws),
       'classifications':dict(collections.Counter(
            d['classification'] for d in draws)),
       'logged_owner_seq_epoch_matches':sum(
            d['source_stamp_match']=='SAMPLED_OWNER_SEQUENCE_EPOCH_MATCH'
            for d in draws),
       'native_actor_owner_certification':'OPEN',
       'context_lifetime_generation':'OPEN','gpu_command_order':'OPEN',
       'ptde_pixel':'OPEN',
       'caveat':'Missing sampled log events are not absent native events; owner+seq+epoch match is not native actor call proof.',
       'draws':draws}
def main():
    p=argparse.ArgumentParser()
    p.add_argument('reshade_log',type=Path)
    p.add_argument('-o','--output',type=Path,required=True)
    args=p.parse_args()
    result=analyze(args.reshade_log.read_text(errors='replace'))
    args.output.write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n')
    print('V411',result['gpu_draw_samples'],result['classifications'])
if __name__=='__main__':main()
