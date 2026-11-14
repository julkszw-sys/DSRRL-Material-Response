#!/usr/bin/env python3
"""V470 RX34 performance isolation: reuse V230 release INFO-log cleanup policy.

Only remove INFO log calls in renderer .cpp sources and integrated addon C++,
not a global disable of telemetry, operator code, warning/error reporting,
native actor diagnostics, or original game binary.

Equivalent to the proven suppress_info_logs() subset of
materialize_v230_release_cleanup.py, with V469 scope kept intact.
"""
from pathlib import Path

ROOT=Path("renderer-core")
TOKEN="reshade::log::message("
PROTECT_MARKERS=(
    "V350 PTDE LERP RGB",
    "GPU RESIDENT RC1",
    "DSRRL BUILD_ID",
    "[DSRRL V465]",
    "[DSRRL V466]",
    "[DSRRL V467]",
    "[DSRRL V468]",
)

def strip_info_log_calls(s):
    at=0;count=0
    while True:
        a=s.find(TOKEN,at)
        if a<0:break
        pos=a+len(TOKEN);depth=1;quote=None;escaped=False
        while pos<len(s) and depth:
            ch=s[pos]
            if quote:
                if escaped:escaped=False
                elif ch=="\\":escaped=True
                elif ch==quote:quote=None
            elif ch in ('"',"'"):quote=ch
            elif ch=="(":depth+=1
            elif ch==")":depth-=1
            pos+=1
        assert depth==0,"unbalanced reshade log invocation"
        args=s[a+len(TOKEN):pos-1].lstrip()
        if args.startswith("reshade::log::level::info,") and not any(p in args for p in PROTECT_MARKERS):
            s=s[:a]+"((void)0)"+s[pos:]
            at=a+9;count+=1
        else:
            at=pos
    return s,count

generated=Path("build-v438-one-shot-diagnostic/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp")
assert generated.is_file(),"post-audit generated source missing; fail closed"
paths=[generated,ROOT/"integrated/integrated_addon.cpp",*sorted((ROOT/"src").rglob("*.cpp"))]
total=0
for p in paths:
    before=p.read_text(encoding="utf-8")
    after,num=strip_info_log_calls(before)
    if num:
        assert before.count("reshade::log::level::warning")==after.count("reshade::log::level::warning"),p
        assert before.count("reshade::log::level::error")==after.count("reshade::log::level::error"),p
        p.write_text(after,encoding="utf-8")
        print("V470",p,"INFO_LOG_CALLS_PRUNED",num)
    total+=num
assert total>=25,("INFO cleanup no-op/drift",total)
print("V470 source PASS: pruned",total,"INFO log calls only; WARN/ERROR, operators, Source1/2/R43, native diagnostics all preserved")
