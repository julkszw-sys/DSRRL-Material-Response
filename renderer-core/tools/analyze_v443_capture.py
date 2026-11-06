#!/usr/bin/env python3
"""Conservative V443 evidence join: native actor/packet -> issued RX34.
Never declares PTDE or pixel PASS, actor lifetime generation is not in V443.
"""
import csv,sys,json,collections,pathlib

def parse(path):
    out=[]
    with open(path,encoding='utf-8-sig',newline='') as f:
        for row in csv.reader(f):
            if not row or row[0].startswith('#') or row[0]=='seq':continue
            try:
                values=[int(x,16) if x else 0 for x in row[5:29]]
                if len(values)!=24:continue
                out.append(dict(seq=int(row[0]),qpc=int(row[1]),frame=int(row[2]),
                    tid=int(row[3]),kind=int(row[4]),v=values))
            except (ValueError,IndexError):continue
    return out

def analyze(events):
    native=[]; issued=[]
    for e in events:
        if e['kind']==15:
            v=e['v']; kind=v[0]
            serial=v[21] if kind==1 else v[22] if kind==2 else 0
            native.append(dict(seq=e['seq'],tid=e['tid'],frame=e['frame'],
              producer_serial=serial,kind=kind,selector=v[1],status=v[2],
              family=v[3],emitter_rva=v[4],builder_rva=v[5],entity=v[6],
              dp=v[7],model=v[9],packet=v[11],a=v[12],b=v[13],
              packet_a=v[15],packet_b=v[16],same_packet=(v[12]==v[15] and v[13]==v[16] and v[14]==v[17])))
        elif e['kind']==7:
            v=e['v']
            issued.append(dict(seq=e['seq'],tid=e['tid'],frame=e['frame'],
                model=v[0],row=v[5],producer_serial=v[8],slot=v[3],receiver=v[15]))
    by_key=collections.defaultdict(list)
    for n in native:
        if n['status']==9 and n['producer_serial'] and n['model']:
            by_key[n['model'],n['producer_serial']].append(n)
    joined=[];ambiguous=0;unmatched=0
    for x in issued:
        candidates=by_key[x['model'],x['producer_serial']]
        if len(candidates)==1:
            n=candidates[0]
            joined.append(dict(frame=x['frame'],issued_row=x['row'],slot=x['slot'],
                model=f"0x{x['model']:x}",entity=f"0x{n['entity']:x}",
                native_a=n['a'],native_b=n['b'],selector=n['selector'],
                native_serial=n['producer_serial'],packet_matches_dp=n['same_packet'],
                native_kind=n['kind'],builder_rva=f"0x{n['builder_rva']:x}"))
        elif len(candidates)>1:ambiguous+=1
        else:unmatched+=1
    # One generation can reuse the same entity pointer after free/reallocation.
    # Therefore a unique serial+model candidate is only per-call evidence.
    statuses=collections.Counter(n['status'] for n in native)
    return dict(event_count=len(events),actor_observer_samples=len(native),
        native_statuses=dict(statuses),issued_events=len(issued),
        joined_unique_by_model_serial=len(joined),issued_ambiguous=ambiguous,
        issued_unmatched=unmatched,examples=joined[:16],
        exact_actor_generation='OPEN',typed_model_packet_to_issued='OPEN',
        PTDE_covered='OPEN',bridge_activation='NOT_APPLICABLE_DIAGNOSTIC',
        pixel='OPEN',root_cause='OPEN')

if __name__=='__main__':
    src=pathlib.Path(sys.argv[1])
    result=analyze(parse(src))
    print(json.dumps(result,indent=2))
    if len(sys.argv)>2:pathlib.Path(sys.argv[2]).write_text(json.dumps(result,indent=2)+'\n')
