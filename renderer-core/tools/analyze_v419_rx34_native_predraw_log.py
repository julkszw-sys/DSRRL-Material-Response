#!/usr/bin/env python3
"""V419 RX34 native post-mutation/pre-DrawIndexed D3D11 state audit.

Groups exact FLVER/MTD, model, parent and raw FgModel generation alongside
real PS b12 RTV/DSV at native DrawIndexed. Does NOT infer GPU execution,
visible pixel or PTDE equivalence. Sampled observations only.
"""
from collections import Counter,defaultdict
from pathlib import Path
import argparse,json,re
P="[DSRRL V419 RX34 PRE-DRAW NATIVE STATE]"
KV=re.compile(r"([A-Za-z_]\w*)=([^\s]+)")
def scan(path):
    counts=Counter()
    buckets=defaultdict(lambda:defaultdict(set))
    examples=defaultdict(list)
    for ln,line in enumerate(Path(path).open(encoding="utf8",errors="replace"),1):
        if P not in line:continue
        d=dict(KV.findall(line))
        counts["v419_sample"]+=1
        if d.get("captured")!="1":
            counts["capture_missing_or_context_mismatch"]+=1
            continue
        if d.get("issued")!="1":
            counts["draw_not_issued"]+=1
            continue
        if d.get("mismatch")!="0":
            counts["native_context_mismatch"]+=1
            continue
        counts["native_pre_draw_record"]+=1
        if d.get("deferred")=="1":counts["deferred_context_command_record"]+=1
        if d.get("ps_match")=="0":counts["unexpected_native_ps"]+=1
        if d.get("expected_cb12")!="(nil)" and d.get("cb12_match")=="0":
            counts["expected_cb12_not_native_bound"]+=1
        key=tuple(d.get(k,"") for k in
                  ("owner","model","fg_parent","registry","slot",
                   "flver_sha","mtd_sha"))
        native_state=tuple(d.get(k,"") for k in
                           ("bound_ps","bound_cb12",
                            "cb12_first_constant","cb12_num_constants",
                            "cb12_window_buffer_matches",
                            "rtv0","rtv1","rtv2",
                            "rtv3","dsv","rtv0_fmt","size","dsv_fmt",
                            "viewport","index_format"))
        row=d.get("row","")
        buckets[key][row].add(native_state)
        if len(examples[key])<24:
            examples[key].append({
                "line":ln,"row":row,
                "seq":d.get("native_producer_seq"),
                "state":{
                    "ps":d.get("bound_ps"),"b12":d.get("bound_cb12"),
                    "ps_match":d.get("ps_match"),"b12_match":d.get("cb12_match"),
                    "b12_ctx1":d.get("cb12_ctx1"),
                    "b12_first_constant":d.get("cb12_first_constant"),
                    "b12_num_constants":d.get("cb12_num_constants"),
                    "rtv0":d.get("rtv0"),"dsv":d.get("dsv"),
                    "rtv0_fmt":d.get("rtv0_fmt"),"size":d.get("size"),
                    "viewport":d.get("viewport"),"ib":d.get("ib"),
                    "index_count":d.get("index_count"),
                    "first_index":d.get("first_index")}})
    pairs=[]
    labels=("bound_ps","bound_cb12",
            "cb12_first_constant","cb12_num_constants",
            "cb12_window_buffer_matches",
            "rtv0","rtv1","rtv2","rtv3",
            "dsv","rtv0_fmt","size","dsv_fmt","viewport","index_format")
    for key,byrow in buckets.items():
        if "30" not in byrow or "31" not in byrow:continue
        a,b=byrow["30"],byrow["31"]
        pairs.append({
            "exact_model_material_key":dict(zip((
                "owner","model","fg_parent","registry","slot",
                "flver_sha","mtd_sha"),key)),
            "row30_native_states":[dict(zip(labels,v)) for v in sorted(a)],
            "row31_native_states":[dict(zip(labels,v)) for v in sorted(b)],
            "shared_native_state":[dict(zip(labels,v)) for v in sorted(a&b)],
            "has_shared_pre_draw_PS_CB_RTV_signature":bool(a&b),
            "sample_events":examples[key],
            "GPU_execution":"OPEN",
            "visible_armor_pixel":"OPEN",
            "PTDE_pixel_equivalence":"OPEN"
        })
    return {
      "counts":dict(counts),"exact_row30_row31_pre_draw_state_pairs":pairs,
      "limits":[
        "Bound PS/CB12 and RTV state captured before native DrawIndexed, not after restore.",
        "A D3D11 deferred context records commands; actual GPU execute/present order is OPEN.",
        "D3D11.1 CB12 window first/num constants is observed when supported; effective C86 bytes/consumer mapping are NOT attested.",
        "RTV/DSV and viewport identity do not certify a fragment of armor was visible.",
        "Captures are bounded to first 256 exact model witnesses plus every 8192nd.",
        "An absent row due to sampling or 8-deep producer journal is UNKNOWN, not evidence of another owner.",
        "This tool does not solve or change the PTDE operator."]}
if __name__=="__main__":
    a=argparse.ArgumentParser()
    a.add_argument("log",type=Path);a.add_argument("-o","--output",type=Path)
    args=a.parse_args()
    txt=json.dumps(scan(args.log),indent=2,ensure_ascii=False)+"\n"
    if args.output:args.output.write_text(txt,encoding="utf8")
    else:print(txt,end="")
