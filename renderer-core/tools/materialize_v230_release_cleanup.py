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

p=r.parent/"build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp"
s=p.read_text("utf-8")
s=one(s,"    dsrrl::runtime::qpc_bisect::report();","    // v2.3.0: no QPC unload profiler report.","qpc report")
# AddonInit's old diagnostic snprintf remains reachable even when its logger
# is removed, so cut the formatting itself; retain QPC scope RAII callsites.
start=s.find("    char qpc_mode_line[200]{};")
assert start>=0 and s.find("    char qpc_mode_line[200]{};",start+1)<0
end=s.find("    reshade::log::message(reshade::log::level::info,qpc_mode_line);",start)
assert end>start
end+=len("    reshade::log::message(reshade::log::level::info,qpc_mode_line);")
s=s[:start]+"    // v2.3.0: QPC mode banner removed, scope shape retained."+s[end:]

# V296 operator-crossing veto: confirmed SOURCE implementation but no
# witnessed avoided material-rendering failure or PTDE pixel improvement.
# It matched mere Oct10 stock-t1 membership, then suppressed separately
# certified diffuse-MR and t0/t2 resource operators even when the whole
# SpecRGB PTDE island was unavailable. The owner has explicitly forbidden
# new or inherited gates with no actual functional necessity proof.
# Remove this shared runtime veto only from V230 release generated source;
# keep individually verified MR consumer, stock t1, exact PTDE companion,
# shader identity, receiver and D3D state gates. The frozen V359 recipe and
# original V296 upstream source/contract remain unchanged.
gate_start="    bool v296_partial_material = false;"
gate_end="    const bool diffuse_mr_active =\\n        decision.active &&\\n        !v296_partial_material &&"
# gate_end is a Python string with literal backslash-n? See below.
gate_end = "    const bool diffuse_mr_active =\n        decision.active &&\n        !v296_partial_material &&"
if s.count(gate_start) != 1 or s.count(gate_end) != 1:
    raise RuntimeError("V230 V296 cleanup: no unique compiled nonmetal veto")
a=s.index(gate_start)
b=s.index(gate_end,a)
old_region=s[a:b]
if not (500 <= len(old_region) <= 3000 and
        "probe_exact_specular_companion" in old_region and
        "suppress_partial_material_response" in old_region and
        "V296_ATOMIC_SIDECAR_NO_HYBRID" in old_region):
    raise RuntimeError("V230 V296 cleanup: unexpected preimage")
s=s[:a]+"    // V230 no speculative SpecRGB->Diffuse/Normal global veto.\n"+s[b:]
s=one(s,gate_end,
      "    const bool diffuse_mr_active =\n        decision.active &&",
      "v296 partial MR veto")
s=one(s,
    "    if (context != nullptr &&\n        !v296_partial_material) {\n        // MR reset invariant: generic stable MR is diffuse-v1 only.",
    "    if (context != nullptr) {\n        // MR reset invariant: generic stable MR is diffuse-v1 only.",
    "v296 normal resource veto")
if "v296_partial_material" in s:
    raise RuntimeError("V296 speculative gate still in generated release")
print("V230_V296_UNPROVEN_CROSS_OPERATOR_VETO_REMOVED_FROM_RELEASE")

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
    s=s[:op+1]+"\n    (void)"+("cmd_list" if name=="observe_pointlight_render_target" else "tag")+";\n    // Release: diagnostic observer disabled; no operator mutation.\n"+s[i:]
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
for p in [r.parent/"build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp",*sorted((r/"src").rglob("*.cpp"))]:
    old=p.read_text("utf-8")
    new,count=suppress_info_logs(old)
    if count:p.write_text(new,"utf-8");stats[str(p.relative_to(r) if p.is_relative_to(r) else p.relative_to(r.parent))]=count
if sum(stats.values())<25:raise RuntimeError("Unexpectedly few telemetry sites")
print("V230_RELEASE_INFO_OBSERVERS_CUT",sum(stats.values()))
print("V230_RELEASE_QPC_SCOPE_SHELL_AND_PERFORMANCE_PATH_PRESERVED")
print("V230_RELEASE_FAILOPEN_WARNING_AND_ERROR_LOGS_PRESERVED")
