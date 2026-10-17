#!/usr/bin/env python3
"""Rank measured CPU candidates and correlate (not attribute) with Present cadence.
Usage: python analyze_v353_perf_log.py ReShade.log --out reportdir
No GPU timing. Nested/inclusive phases must NOT be added to get total addon CPU.
"""
import argparse, collections, csv, json, math, re
from pathlib import Path

def kv(line):
    return dict(re.findall(r'\b([a-zA-Z_][\w]*)=([^\s]+)',line))
def tofloat(value):
    try:return float(value)
    except (TypeError,ValueError):return 0.

p=argparse.ArgumentParser()
p.add_argument("log",type=Path)
p.add_argument("--out",type=Path,default=Path("v353_report"))
args=p.parse_args()
content=args.log.read_text(encoding="utf-8",errors="replace").splitlines()
perfs={}
stages=[]
for line in content:
    if "[DSRRL V352 PERF]" in line:
        r=kv(line)
        w=int(r.get("window",0))
        if w>0:perfs[w]=r
    if "[DSRRL V353 CPU_STAGE]" in line:
        r=kv(line)
        if int(r.get("window",0))>0:stages.append(r)

args.out.mkdir(parents=True,exist_ok=True)
if not stages:
    raise SystemExit("No V353 CPU_STAGE entries: wrong binary/log or fewer than 30 seconds.")

with (args.out/"v353_cpu_windows.csv").open("w",newline="",encoding="utf-8") as f:
    cols=["window","stage","samples","sample_stride","estimated_invocations",
          "mean_sample_us","max_sample_us","projected_cpu_ms_per_s",
          "avg_fps","p99_interval_ms_le","interval_ge_50ms","interval_ge_100ms"]
    writer=csv.DictWriter(f,fieldnames=cols)
    writer.writeheader()
    for r in stages:
        p=perfs.get(int(r["window"]),{})
        writer.writerow({c:r.get(c,p.get(c,"")) for c in cols})

groups=collections.defaultdict(list)
for row in stages:
    groups[row["stage"]].append(row)
stats=[]
for name,entries in groups.items():
    populated=[e for e in entries if int(e.get("samples",0))>0]
    measured=sum(int(x.get("samples",0)) for x in populated)
    if not measured:continue
    cpu=[tofloat(e.get("projected_cpu_ms_per_s")) for e in populated]
    means=[tofloat(e.get("mean_sample_us")) for e in populated]
    weighted_mean=sum(m*int(e["samples"]) for e,m in zip(populated,means))/measured
    stats.append({
        "stage":name,
        "sampled_windows":len(populated),
        "samples":measured,
        "mean_sample_us_weighted":round(weighted_mean,4),
        "projected_cpu_ms_per_s_avg":round(sum(cpu)/len(cpu),4),
        "projected_cpu_ms_per_s_peak":round(max(cpu),4),
        "inclusive_nested":True
    })
stats.sort(key=lambda x:x["projected_cpu_ms_per_s_avg"],reverse=True)

def pearson(pairs):
    if len(pairs)<10:return None
    x=[a for a,_ in pairs];y=[b for _,b in pairs]
    ax=sum(x)/len(x);ay=sum(y)/len(y)
    u=sum((a-ax)*(b-ay) for a,b in pairs)
    vx=sum((a-ax)**2 for a in x);vy=sum((b-ay)**2 for b in y)
    if vx*vy<=0:return None
    return round(u/math.sqrt(vx*vy),4)

corr=[]
for name,entries in groups.items():
    pts=[(tofloat(e.get("projected_cpu_ms_per_s")),
          tofloat(perfs[int(e["window"])].get("avg_fps")))
         for e in entries if int(e["window"]) in perfs and int(e.get("samples",0))>=2]
    corr.append({"stage":name,"window_count":len(pts),
                 "pearson_cpu_load_vs_fps":pearson(pts)})
corr.sort(key=lambda x:x["pearson_cpu_load_vs_fps"] if x["pearson_cpu_load_vs_fps"] is not None else 1.)

fps=[tofloat(x.get("avg_fps")) for x in perfs.values()]
out={
    "windows_cpu":len(set(int(e["window"]) for e in stages)),
    "windows_present":len(perfs),
    "present_avg_fps_unweighted":sum(fps)/len(fps) if fps else None,
    "stage_priorities_by_inclusive_estimated_CPU":stats,
    "cpu_load_fps_correlations_not_causal":corr,
    "warnings":[
        "CPU estimates are QPC sampled 1/128 invocations, not exhaustive exact CPU.",
        "Draw and nested stage times OVERLAP; do NOT sum projected_cpu_ms_per_s.",
        "Present cadence may be affected by GPU, limiter, driver, scheduling, game load.",
        "Correlation does not establish causality; isolate high-ranking modules via A/B builds.",
        "QPC instrumentation and full runtime counters add overhead compared with release.",
        "GPU cost of replacement shaders is not measured in this session."
    ]
}
(args.out/"v353_summary.json").write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding="utf-8")
print(f"Windows: CPU={out['windows_cpu']} Present={out['windows_present']}")
print("CPU priority candidates (inclusive estimates, not additive):")
for x in stats:
    print(f"  {x['stage']:34} mean {x['mean_sample_us_weighted']:10.4f}us "
          f"approx {x['projected_cpu_ms_per_s_avg']:9.3f}ms/s "
          f"peak {x['projected_cpu_ms_per_s_peak']:9.3f}ms/s")
print("Written",args.out/"v353_summary.json",args.out/"v353_cpu_windows.csv")
