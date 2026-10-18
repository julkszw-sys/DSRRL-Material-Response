#!/usr/bin/env python3
"""v2.3.0: discard orphaned INFO-only formatting after INFO sink removal.

Only the formatting call is discarded, never enclosing operator control
flow, receiver checks, fallbacks, or warning/error diagnostics. Arguments
to an INFO log have no semantic receiver.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
paths=[r.parent/"build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp",*sorted((r/"src").rglob("*.cpp"))]
essential=("V350 PTDE LERP RGB","GPU RESIDENT RC1","DSRRL BUILD_ID")
def process(s):
    token="std::snprintf("
    pos=0;cut=0;labels=[]
    while True:
        at=s.find(token,pos)
        if at<0:break
        i=at+len(token);level=1;quote=None;escape=False
        while i<len(s) and level:
            c=s[i]
            if quote:
                if escape:escape=False
                elif c=="\\":escape=True
                elif c==quote:quote=None
            elif c in ('"',"'"):quote=c
            elif c=="(":level+=1
            elif c==")":level-=1
            i+=1
        if level:raise RuntimeError("unbalanced snprintf")
        args=s[at+len(token):i-1]
        end=i
        while end<len(s) and s[end].isspace():end+=1
        if end>=len(s) or s[end]!=";":raise RuntimeError("snprintf not a statement")
        # Previous pass changed INFO-only ReShade logging into ((void)0).
        # Never suppress source/warning/fail-open operational messages.
        tail=s[end+1:end+90].lstrip()
        if "[DSRRL " in args and not any(x in args for x in essential) and tail.startswith("((void)0)"):
            # Compile-time false condition leaves diagnostics traceable as
            # source and removes formatting/strings from the release PE.
            s=s[:at]+"if constexpr (false) "+s[at:]
            pos=at+len("if constexpr (false) ")+len(token)
            cut+=1
            labels.append(args[args.index("[DSRRL "):][:95])
        else:pos=i
    return s,cut,labels
count=0; files=0
for p in paths:
    before=p.read_text("utf-8")
    after,n,labels=process(before)
    if n:
        p.write_text(after,"utf-8")
        count+=n;files+=1
        print("V230_ORPHAN_INFO_FORMAT_CUT",p.name,n)
if count<3:raise RuntimeError("Expected orphaned telemetry formatting not found")
print("V230_INFO_FORMATTING_COMPILED_OUT",count,"FILES",files)
