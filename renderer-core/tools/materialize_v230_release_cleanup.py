#!/usr/bin/env python3
"""Release-only post-V359 instrumentation cut; no operator, routing or cache edits.
Apply after every V359 materializer and source contract, before CMake.
"""
from pathlib import Path

r=Path(__file__).resolve().parents[1]

def one(s,old,new,key):
    if s.count(old)!=1:
        raise RuntimeError(f"v2.3.0 release drift [{key}]: {s.count(old)}")
    return s.replace(old,new,1)

p=r/"include/dsrrl/runtime/runtime_hot_telemetry.hpp"
s=p.read_text("utf-8")
for name in ("hot_enabled","effect_enabled","native_state_verification_enabled"):
    anchor=f"inline bool {name}() noexcept\n{{"
    s=one(s,anchor,anchor+"\n#if defined(DSRRL_V230_PRODUCTION)\n    return false;\n#else",name)
    # Existing function body is still parsed but unreachable in production.
    start=s.index(anchor); pos=s.index("{",start); depth=0
    for i in range(pos,len(s)):
        if s[i]=="{": depth+=1
        if s[i]=="}":
            depth-=1
            if depth==0:break
    else:raise RuntimeError("Unbalanced telemetry function "+name)
    s=s[:i]+"#endif\n"+s[i:]
p.write_text(s,"utf-8")

# Scope Shell is performance-sensitive: preserve the EXACT RAII scope class
# and DSRRL_QPC_SCOPE_SHELL_NO_CLOCK. Disable only the startup diagnostic
# mode-file input and unload reporting, never the scope callsites.
p=r/"include/dsrrl/runtime/qpc_causality_bisect.hpp"
s=p.read_text("utf-8")
s=one(s,"inline void init_from_game_directory() noexcept {",
"""inline void init_from_game_directory() noexcept {
#if defined(DSRRL_V230_PRODUCTION)
    g_mode.store(0u, std::memory_order_release);
    return;
#else""","qpc initialization")
s=one(s,"inline void update_max(std::atomic<std::uint64_t> &maximum,",
"#if defined(DSRRL_V230_PRODUCTION)\n// No file/console profiler activity in the release.\n#endif\ninline void update_max(std::atomic<std::uint64_t> &maximum,","qpc anchor")
# close only init_from_game_directory function's preprocessor branch
anchor="inline void init_from_game_directory() noexcept {"
start=s.index(anchor); pos=s.index("{",start); depth=0
for i in range(pos,len(s)):
    if s[i]=="{": depth+=1
    elif s[i]=="}":
        depth-=1
        if depth==0:break
else:raise RuntimeError("QPC source drift")
s=s[:i]+"#endif\n"+s[i:]
# Report path is unnecessary; delete report() invocation in the integrated
# addon so it cannot export debug telemetry.
p.write_text(s,"utf-8")

p=r/"integrated/integrated_addon.cpp"
s=p.read_text("utf-8")
s=one(s,"    dsrrl::runtime::qpc_bisect::report();","    // v2.3.0: no QPC unload profiler report.","qpc report")
# Neither diagnostic dump computes operator authority: keep all real state
# counters, D3D11 lifetime/restore code, all material and shader routing.
for name in ("log_state","log_effect_matrix","observe_pointlight_render_target"):
    marker=("void log_state(const char *tag) noexcept" if name=="log_state" else "void "+name+"(")
    if s.count(marker)!=1:raise RuntimeError("Ambiguous diagnostic "+name)
    begin=s.index(marker); op=s.index("{",begin)
    depth=0
    for i in range(op,len(s)):
        if s[i]=="{":depth+=1
        elif s[i]=="}":
            depth-=1
            if depth==0:break
    else:raise RuntimeError("Unbalanced diagnostic "+name)
    # Preserve API and its arguments; an empty function still compiles.
    s=s[:op+1]+"\n    // Release: diagnostic observer disabled; no operator mutation.\n"+s[i:]
p.write_text(s,"utf-8")

def suppress_info_logs(s):
    token="reshade::log::message("
    offset=0; cut=0
    while True:
        a=s.find(token,offset)
        if a<0:break
        pos=a+len(token);depth=1;quote=None;escaped=False
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
        if depth:raise RuntimeError("Unbalanced log::message")
        args=s[a+len(token):pos-1].lstrip()
        # Boot identity / GPU residency are operational state, not frame
        # sampling; the inherited V350 native binary gate expects the latter.
        essential=("V350 PTDE LERP RGB","GPU RESIDENT RC1","DSRRL BUILD_ID")
        if args.startswith("reshade::log::level::info,") and not any(w in args for w in essential):
            s=s[:a]+"((void)0)"+s[pos:]
            offset=a+9;cut+=1
        else:offset=pos
    return s,cut

stats={}
for p in [r/"integrated/integrated_addon.cpp",*sorted((r/"src").rglob("*.cpp"))]:
    old=p.read_text("utf-8")
    new,count=suppress_info_logs(old)
    if count:p.write_text(new,"utf-8");stats[str(p.relative_to(r))]=count
if sum(stats.values())<25:raise RuntimeError("Unexpectedly few telemetry sites")
print("V230_RELEASE_INFO_OBSERVERS_CUT",sum(stats.values()))
print("V230_RELEASE_QPC_SCOPE_SHELL_AND_PERFORMANCE_PATH_PRESERVED")
print("V230_RELEASE_FAILOPEN_WARNING_AND_ERROR_LOGS_PRESERVED")
