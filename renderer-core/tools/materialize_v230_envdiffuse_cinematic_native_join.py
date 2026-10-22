#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'src/runtime/pmetal_native_draw_bridge.cpp'
s=p.read_text()
a='    draw_tx_mutation mutation{};'
assert s.count(a)==1
s=s.replace(a,a+'\n#if defined(DSRRL_V230_ENVDIFFUSE_CINEMATIC_TRACE)\n    envdiffuse_cinematic_trace::witness edc{};\n#endif')
a='    g_pending.owner = this;'
assert s.count(a)==2
s=s.replace(a,a+'''
#if defined(DSRRL_V230_ENVDIFFUSE_CINEMATIC_TRACE)
    const bool edc_owner=(mutation.owners &
        core::operator_bit(core::operator_id::env_spec))!=0u &&
        (mutation.owners &
        core::operator_bit(core::operator_id::env_diffuse))!=0u;
    if(edc_owner && envdiffuse_cinematic_trace::local.ready)
        g_pending.edc=envdiffuse_cinematic_trace::local;
#endif
''')
assert s.count('const auto &w=envdiffuse_cinematic_trace::local;')==4
s=s.replace('const auto &w=envdiffuse_cinematic_trace::local;','const auto &w=pending.edc;')
assert s.count('if(w.source && w.selected) {')==4
s=s.replace('if(w.source && w.selected) {','if(w.source && w.selected && w.ready && envdiff_owner) {')
p.write_text(s)
print('EDC_NATIVE_PENDING_EXACT_JOIN_PASS')
