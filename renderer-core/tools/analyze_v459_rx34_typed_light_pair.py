#!/usr/bin/env python3
"""V459 read-only per-issued-RX34 Light transition analysis.
Never infer actor identity or pixel overlap from draw pointers/model resources.
Payload kind21 v00..v16 V451, v17..v23 V459.
"""
import argparse,collections,csv,json,math,struct
from pathlib import Path

def hexv(x): return f"0x{x:x}"
def float_bits(bits):
    return struct.unpack("<f",struct.pack("<I",bits&0xffffffff))[0]

def analyze(path):
    totals=collections.Counter()
    groups={}
    invalid=collections.Counter()
    frames_all=set()
    with open(path,encoding="utf-8-sig",errors="replace",newline="") as file:
        for line in csv.reader(file):
            if not line or line[0].startswith("#") or line[0]=="seq":continue
            if len(line)<29:
                invalid["truncated"]+=1
                continue
            try:
                kind=int(line[4])
                totals[f"kind_{kind}"]+=1
                if kind!=21:continue
                frame=int(line[2])
                frames_all.add(frame)
                v=[int(z,16) for z in line[5:29]]
                source,row,entity,dp,resource,model,context=v[1],v[2],v[3],v[4],v[6],v[5],v[16]
                status,current,target=v[17],v[18],v[19]
                if v[15]!=345 or row not in (30,31):
                    invalid["out_of_scope"]+=1
                    continue
                # This key is a session-local native object address tuple,
                # NOT a gameplay actor/generation across sessions.
                key=(entity,dp,model,resource)
                g=groups.setdefault(key,{"entity":hexv(entity),"drawparam":hexv(dp),
                    "engine_model":hexv(model),"model_resource":hexv(resource),
                    "context":hexv(context),"rows":set(),"source_kinds":set(),
                    "native_sequences":set(),"materials":set(),"frames":set(),
                    "samples":0,"class_valid":0,"pair_valid":0,"dp_agrees":0,
                    "currents":set(),"targets":set(),"alpha_bits":set(),
                    "active_pairs":0,"cross_row_pairs":0,"lowbyte_row_match":0,
                    "nonfinite_weight":0,"statuses":collections.Counter()})
                g["samples"]+=1
                g["frames"].add(frame);g["rows"].add(row)
                g["source_kinds"].add(source);g["materials"].add(v[14]);g["native_sequences"].add(v[0])
                g["statuses"][hexv(status)]+=1
                if status&0x6==0x6: g["class_valid"]+=1
                if status&0x8: g["dp_agrees"]+=1
                if not status&0x10:
                    invalid["no_typed_read"]+=1
                    continue
                g["pair_valid"]+=1
                g["currents"].add(current);g["targets"].add(target)
                g["alpha_bits"].add(v[20])
                if (current&0xff)==row and (target&0xff)==row:g["lowbyte_row_match"]+=1
                if (current&0xff)!=row or (target&0xff)!=row:g["cross_row_pairs"]+=1
                w=float_bits(v[20])
                if not math.isfinite(w):g["nonfinite_weight"]+=1
                if current!=target:g["active_pairs"]+=1
            except (ValueError,IndexError,OverflowError):
                invalid["parse_error"]+=1

    out=[]
    for g in groups.values():
        for field in ("rows","source_kinds","materials","frames","native_sequences"):
            if field=="frames":
                g["frame_count"]=len(g.pop(field));continue
            if field=="native_sequences":
                g["native_sequence_count"]=len(g.pop(field));continue
            g[field]=sorted(g[field])
        for field in ("currents","targets"):g[field]=[hexv(v) for v in sorted(g[field])]
        g["alpha_bits"]=[hexv(v) for v in sorted(g["alpha_bits"])]
        g["statuses"]=dict(sorted(g["statuses"].items()))
        g["changed_rows_on_one_pointer"]=len(g["rows"])>1
        out.append(g)
    out.sort(key=lambda x:(x["rows"],x["entity"]))
    return {
        "format":"V459 kind21 in-place extension; v17..v23 are Light pair",
        "observed_events":dict(totals),
        "invalid":dict(invalid),
        "native_entities":len(out),
        "sample_frame_buckets":len(frames_all),
        "groups":out,
        "summary":{
            "sample_count":sum(g["samples"] for g in out),
            "typed_pair_reads":sum(g["pair_valid"] for g in out),
            "native_pairs_differ":sum(g["active_pairs"] for g in out),
            "row_endpoint_mismatches":sum(g["cross_row_pairs"] for g in out),
            "entities_with_multiple_rows":[g["entity"] for g in out if g["changed_rows_on_one_pointer"]],
            "nonfinite_weights":sum(g["nonfinite_weight"] for g in out)
        },
        "limits":[
            "static and runtime tests must be kept separate",
            "entity pointer is not PlayerIns/EnemyIns ownership or generation",
            "identical resource/FLVER does not imply identical gameplay actor",
            "row matching compares only LOW BYTE of native packed key",
            "no exact GPU shader/CB or same-pixel coverage proven",
            "source_kind alone is not actor identity",
            "PTDE pixel behavior requires independent owner comparison"
        ],
        "hypotheses":{
            "single_entity_light_lerp":"CAN_FALSIFY_WITH_COMPLETE_TYPED_SAMPLES; no owner inference",
            "npc_cross_actor_contamination":"OPEN",
            "own_player_descriptor_switch":"OPEN",
            "gpu_pixel_effect":"OPEN"
        }
    }
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("csv_file",type=Path)
    ap.add_argument("--json",type=Path)
    a=ap.parse_args()
    out=json.dumps(analyze(a.csv_file),indent=2,ensure_ascii=False,allow_nan=False)+"\n"
    if a.json:a.json.write_text(out,encoding="utf-8")
    print(out)
if __name__=="__main__":main()
