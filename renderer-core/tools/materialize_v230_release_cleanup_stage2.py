#!/usr/bin/env python3
"""v2.3.0: compile out diagnostic-only callback bodies after V359 source audit.

Do not remove operator or D3D11 state/restore calls. Original bodies remain
auditable in #else branches, and are absent from the release binary.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
physical=root.parent/"build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp"
pmetal=root/"src/runtime/pmetal_envspec_draw_runtime.cpp"

def stub(path,name,args):
    s=path.read_text("utf-8")
    sig="void "+name+"("
    if s.count(sig)!=1: raise RuntimeError(f"missing/ambiguous {name}")
    start=s.index(sig)
    op=s.index("{",start+len(sig))
    depth=0
    for end in range(op,len(s)):
        if s[end]=="{":depth+=1
        elif s[end]=="}":
            depth-=1
            if depth==0:break
    else:raise RuntimeError("unbalanced "+name)
    existing=s[op+1:end]
    if not any(t in existing for t in ("std::snprintf","((void)0)","v287_nonmetal_trace::log")):
        raise RuntimeError("not a diagnostic function: "+name)
    noop="\n".join("    (void)"+a+";" for a in args.split())
    s=s[:op+1]+"\n#if defined(DSRRL_V230_PRODUCTION)\n"+noop+"\n#else\n"+existing+"\n#endif\n"+s[end:]
    path.write_text(s,"utf-8")

for path,name,args in (
(physical,"mark_effect_probe","id stage receiver_id route_index"),
(physical,"log_pointlight_gate_once","bit stage fixed_pointlight_receiver clustered_pointlight_receiver clustered_pointlight_spc owner_ok material decision"),
(physical,"log_pointlight_prep_once","bit stage spc blended material_ready operator_gate_ready sidecar_ready failure sidecar_result_code"),
(physical,"observe_equipment_specrgb_carrier","cmd_list material decision"),
(pmetal,"log_prepare_stage_once","bit stage material decision family"),
(pmetal,"v357_trace_rejected_srv","context binding receiver route material_slot"),
(pmetal,"v358_log_probe_reject_native_origin","view rx route slot binding"),
): stub(path,name,args)
print("V230_DIAGNOSTIC_ONLY_CALLBACKS_DISABLED=7")
