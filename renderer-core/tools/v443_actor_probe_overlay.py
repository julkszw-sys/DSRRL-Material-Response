#!/usr/bin/env python3
"""Overlay V443 native actor/packet observability onto V439 after V438 preparation.
Does not modify the game executable, the installed wrapper-detour mask, donor
eligibility, source selection, state restoration or shader/material response.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'src/runtime/pmetal_env_source_runtime.cpp'
trace=ROOT/'include/dsrrl/runtime/v438_one_shot_trace.hpp'

def replace_once(s,old,new,label):
    if new in s:return s
    count=s.count(old)
    if count!=1:raise AssertionError(f'V443 drift {label}: expected 1, got {count}')
    return s.replace(old,new,1)

t=trace.read_text(encoding='utf-8')
t=replace_once(t,'ENV_JOURNAL=14','ENV_JOURNAL=14,ACTOR_NATIVE=15','trace event')
trace.write_text(t,encoding='utf-8')
s=source.read_text(encoding='utf-8')
s=replace_once(s,
    '#include "dsrrl/runtime/v438_one_shot_trace.hpp"',
    '#include "dsrrl/runtime/v438_one_shot_trace.hpp"\n'
    '#include "dsrrl/runtime/v443_native_actor_certificate.hpp"',
    'include')

# The existing preimage-guarded native packer hooks are already active.
# Do not put a call in any part of the normal source-selection decision.
anchor='''    if (donor_ok)
        publish_hook_source(
            donor,'''
assert s.count(anchor)==1
# Actor probe is called AFTER the protected source publication has completed;
# failures are read-only telemetry and never change donor_ok or policy.
end='''            source,
            static_cast<std::int16_t>(
                selector));
}'''
assert s.count(end)==1
s=s.replace(end,'''            source,
            static_cast<std::int16_t>(
                selector));
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    if (v438::state().active.load(std::memory_order_acquire) &&
        (selector == 1310 || selector == 1311)) {
        const auto w=v443::probe(g_source_base);
        v438::emit(v438::ACTOR_NATIVE,{
            1u,static_cast<unsigned>(selector),w.status,w.emitter_family,
            w.emitter_return_rva,w.builder_return_rva,
            w.entity,w.dp,w.actor_vtable,w.model,w.model_vtable,
            w.packet,w.dp_a,w.dp_b,w.dp_beta,w.packet_a,w.packet_b,
            w.packet_beta,w.builder_rdi,w.builder_rbp,
            v438::p(source),(donor_ok&&g_hook_source_tls.valid)?g_hook_source_tls.serial:0u});
    }
#endif
}''',1)
blend_end='''            source_b,
            endpoints.b);
}'''
assert s.count(blend_end)==1
s=s.replace(blend_end,'''            source_b,
            endpoints.b);
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    if (v438::state().active.load(std::memory_order_acquire) &&
        (selector_a == 1310 || selector_a == 1311 ||
         selector_b == 1310 || selector_b == 1311)) {
        const auto w=v443::probe(g_source_base);
        v438::emit(v438::ACTOR_NATIVE,{
            2u,static_cast<unsigned>(selector_a),w.status,w.emitter_family,
            w.emitter_return_rva,w.builder_return_rva,
            w.entity,w.dp,w.actor_vtable,w.model,w.model_vtable,
            w.packet,w.dp_a,w.dp_b,w.dp_beta,w.packet_a,w.packet_b,
            w.packet_beta,w.builder_rdi,w.builder_rbp,
            v438::p(source_a),v438::p(source_b),
            (donor_ok&&g_hook_source_tls.valid)?g_hook_source_tls.serial:0u});
    }
#endif
}''',1)
s=replace_once(s,
    '"[DSRRL V439] WRAPPER_HOOKS_DISABLED crash_isolation=ON");',
    '"[DSRRL V439] WRAPPER_HOOKS_DISABLED crash_isolation=ON");\n'
    '    reshade::log::message(reshade::log::level::warning,\n'
    '        "[DSRRL V443] PACKER_ACTOR_OBSERVER read_only=1 stage=UNVERIFIED_RUNTIME");',
    'observer version marker')
source.write_text(s,encoding='utf-8')
print('V443 actor packer provenance observer overlay PASS: 2 existing hooks, no new detours')
