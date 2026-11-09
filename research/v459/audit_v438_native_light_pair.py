#!/usr/bin/env python3
"""V459 exact joins for prior user-provided V438 snapshot.
No actor/pixel conclusions. Read-only, depends on explicit original CSV SHA256.
"""
import argparse,collections,csv,hashlib,json
from pathlib import Path
CSV_SHA="744d3af90a4f99eff8e7d22d8e78c2c6ced2e09141bda3ea0a23906d3e7d4329"

def audit(path,check_hash=True):
    b=path.read_bytes()
    got=hashlib.sha256(b).hexdigest()
    if check_hash and got!=CSV_SHA:raise ValueError("not exact original V438 runtime CSV")
    native={}
    tickets={}
    issued=[]
    kinds=collections.Counter()
    default_count=0
    for r in csv.reader(b.decode("utf-8-sig",errors="replace").splitlines()):
        if not r or r[0].startswith("#") or r[0]=="seq":continue
        if len(r)<29:continue
        kind=int(r[4]);kinds[kind]+=1
        if kind not in (15,16,17,19):continue
        seq,frame,tid=int(r[0]),int(r[2]),int(r[3])
        v=[int(z,16) for z in r[5:29]]
        if kind==15 and v[2]==9: # Exact native default-builder packet certificate
            default_count+=1
            serial=v[21]
            if serial in native:raise AssertionError("reused producer serial without generation")
            native[serial]=(seq,frame,tid,v)
        elif kind==16:
            serial=v[0]
            if serial in tickets:raise AssertionError("duplicate typed native source ticket")
            tickets[serial]=(seq,frame,tid,v)
        elif kind==17:issued.append((seq,frame,tid,v))
    per_entity=collections.defaultdict(lambda:{"count":0,"rows":set(),"pairs":set(),
                 "frames":set(),"model":set(),"default_native":0})
    valid_native=0
    native_fail=[]
    for serial,(seq,frame,tid,v) in tickets.items():
        n=native.get(serial)
        if n is None:native_fail.append((serial,"no_native"));continue
        _,nf,nt,w=n
        # V443 v06 entity/v07 DP, v12/13 signed 16-bit keys, v14 alpha bits,
        # v15/16 builder packet, v17 builder alpha. V447 v01 entity/v02 DP,
        # v06/07 native ids, v08/09 row low bytes, v10 alpha bits.
        ok=(nf==frame and nt==tid and w[6]==v[1] and w[7]==v[2]
           and w[12]==w[15] and w[13]==w[16] and w[14]==w[17]
           and w[12]==v[6] and w[13]==v[7] and w[14]==v[10]
           and (w[12]&255)==v[8] and (w[13]&255)==v[9]
           and w[12]==w[13] and w[14]==0)
        if not ok:native_fail.append((serial,"mismatch"));continue
        valid_native+=1
        g=per_entity[v[1]]
        g["count"]+=1
        g["rows"].add(v[8]);g["pairs"].add((w[12],w[13],w[14]))
        g["frames"].add(frame);g["model"].add(v[5]);g["default_native"]+=1
    source1=0;source2=0;joined_rx34=0
    failures=[]
    by_entity_source1=collections.Counter()
    for seq,frame,tid,v in issued:
        if v[14]!=34 or v[15]!=345 or v[2] not in (30,31):continue
        if v[13]==2:
            source2+=1
            continue
        if v[13]!=1:failures.append((seq,"unrecognised_source_kind"));continue
        source1+=1
        ticket=tickets.get(v[0]);cert=native.get(v[0])
        if ticket is None or cert is None:failures.append((seq,"missing_ticket"));continue
        _,tf,tt,t=ticket; _,nf,nt,w=cert
        if not (tf==frame==nf and tt==tid==nt and t[1]==w[6] and
                t[8]==t[9]==v[2] and w[12]==w[13] and w[14]==0 and
                (w[12]&255)==v[2]):
            failures.append((seq,"source1_wrong_entity_or_pair"))
        else:
            joined_rx34+=1
            by_entity_source1[(t[1],v[2])]+=1
    entities=[]
    for ptr,g in sorted(per_entity.items()):
        entities.append({"entity":f"0x{ptr:08x}","samples":g["count"],
           "rows":sorted(g["rows"]),"native_pairs":sorted([list(x) for x in g["pairs"]]),
           "frame_buckets":len(g["frames"]),"engine_models":[f"0x{x:08x}" for x in sorted(g["model"])]})
    return {
       "original_csv_sha256":got,
       "events":dict(kinds),
       "native_default_packet_events":default_count,
       "native_tickets":len(tickets),
       "native_exact_joins":valid_native,
       "native_failed":native_fail,
       "issued_RX34_source1":source1,
       "issued_RX34_source1_exact_joins":joined_rx34,
       "issued_RX34_source2_unverified":source2,
       "issued_RX34_failures":failures,
       "per_entity":entities,
       "source1_entity_rows":[{"entity":f"0x{e:08x}","row":r,"count":c} for (e,r),c in sorted(by_entity_source1.items())],
       "native_distinct_entities":len(entities),
       "native_entities_changing_light_pair":[x["entity"] for x in entities if len(x["native_pairs"])>1],
       "native_entities_with_both_rows":[x["entity"] for x in entities if len(x["rows"])>1],
       "interpretation":"For this exact session: different render entities carry row30 vs row31, never a native Light lerp in observed default-builder packets.",
       "limits":["User's scene runtime and exact CSV only","12 Source2 RX34 draws lack independent native joins","not gameplay actor ID/generation","not GPU shader/CB or same-pixel overlap","not PTDE pixel pass"]
    }

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("csv",type=Path);ap.add_argument("--json",type=Path)
    args=ap.parse_args()
    result=audit(args.csv)
    print(json.dumps(result,ensure_ascii=False,indent=2))
    if args.json:args.json.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
