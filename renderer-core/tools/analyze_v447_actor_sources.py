#!/usr/bin/env python3
"""Offline V447 CSV provenance audit; never infer actor generation or pixel PASS."""
import collections
import csv
import hashlib
import json
import pathlib
import sys

def parse_events(path):
    with path.open(newline='',encoding='utf-8-sig') as handle:
        for columns in csv.reader(handle):
            if len(columns)<29 or columns[0].startswith('#') or columns[0]=='seq':
                continue
            try:
                yield dict(seq=int(columns[0]),qpc=int(columns[1]),
                           frame=int(columns[2]),tid=int(columns[3]),
                           kind=int(columns[4]),
                           v=[int(x,16) for x in columns[5:29]])
            except ValueError:
                continue

def analyze(path):
    producers={}
    issued=[]
    conflicting_serial=set()
    all_producers=[]
    for event in parse_events(path):
        v=event['v']
        if event['kind']==16:
            seq=v[0]
            if not seq:continue
            data=dict(seq=seq,entity=v[1],drawparam=v[2],packet=v[3],
                      context=v[4],model=v[5],a=v[6],b=v[7],
                      row_a=v[8],row_b=v[9],beta_bits=v[10],
                      status=v[11],generation=v[12])
            if seq in producers and producers[seq]!=data:
                conflicting_serial.add(seq)
            producers[seq]=data
            all_producers.append(data)
        elif event['kind']==17:
            issued.append(dict(seq=v[0],context=v[1],row=v[2],verdict=v[3],
                               producer_entity=v[4],producer_packet=v[6],
                               producer_context=v[7],producer_model=v[8],
                               source_kind=v[13],receiver=v[14],route=v[15],
                               material_slot=v[16],shader=v[17]))
    contexts=collections.defaultdict(lambda:collections.defaultdict(set))
    for p in all_producers:
        contexts[p['context']][p['entity']].add(p['row_a'])
    joins=collections.Counter()
    row_pairs=collections.Counter()
    for event in issued:
        t=producers.get(event['seq'])
        if t is None:
            joins['no_producer_ticket']+=1;continue
        if event['seq'] in conflicting_serial:
            joins['conflicting_serial']+=1;continue
        if t['context']!=event['context']:
            joins['context_mismatch']+=1;continue
        if t['row_a']!=event['row']:
            joins['row_mismatch']+=1;continue
        joins['same_sequence_context_row_NO_RECEIVER_ACTOR']+=1
        row_pairs[(event['row'],t['entity'])]+=1
    shared={hex(ctx):{
        hex(entity):sorted(rows) for entity,rows in owners.items()
    } for ctx,owners in contexts.items() if len(owners)>1}
    return dict(version='V447',status='DIAGNOSTIC_ONLY',
                csv_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                producer_tickets=len(all_producers),unique_sequences=len(producers),
                issued_rx34_observations=len(issued),
                conflict_sequences=sorted(conflicting_serial),
                joins=dict(joins),
                shared_context_multiple_native_entities=shared,
                rows_by_observed_entity={f'{row}:{hex(entity)}':count
                                         for (row,entity),count in row_pairs.items()},
                actor_generation='OPEN',receiver_actor_identity='OPEN',
                pixel_behavior='OPEN',
                note='Same producer seq/context/row is not a proof of correct receiver actor or pass.')

if __name__=='__main__':
    if len(sys.argv)!=3:
        raise SystemExit('usage: analyze_v447_actor_sources.py V438_*.csv results.json')
    src=pathlib.Path(sys.argv[1]);dst=pathlib.Path(sys.argv[2]);result=analyze(src)
    dst.write_text(json.dumps(result,indent=2,sort_keys=True),encoding='utf-8')
    print(json.dumps({k:result[k] for k in ('producer_tickets','unique_sequences','issued_rx34_observations','joins','actor_generation','pixel_behavior')},indent=2))
