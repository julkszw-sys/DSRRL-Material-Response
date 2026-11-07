#!/usr/bin/env python3
"""V447 diagnostic-only per-actor LightBank source journal. Run AFTER V443 overlay.
Deliberately does not change Source1, latest_hook_source, donor/row policy,
GPU resources, wrapper hooks or D3D11 state. No V438 wrapper detours.
"""
from pathlib import Path
ROOT = Path('renderer-core')
source = ROOT/'src/runtime/pmetal_env_source_runtime.cpp'
integrated = ROOT/'integrated/integrated_addon.cpp'
trace = ROOT/'include/dsrrl/runtime/v438_one_shot_trace.hpp'

def patch_once(path, old, new):
    s=path.read_text(encoding='utf-8')
    if new in s:return
    if s.count(old)!=1:
        raise RuntimeError(f'V447 source drift: {path}, {s.count(old)} hits for {old[:90]!r}')
    path.write_text(s.replace(old,new,1),encoding='utf-8')

patch_once(trace,'ENV_JOURNAL=14,ACTOR_NATIVE=15',
           'ENV_JOURNAL=14,ACTOR_NATIVE=15,V447_NATIVE_TICKET=16,V447_RX34_BIND=17')
for p in (source,integrated):
    patch_once(p, '#include "dsrrl/runtime/v438_one_shot_trace.hpp"',
               '#include "dsrrl/runtime/v438_one_shot_trace.hpp"\n'
               '#include "dsrrl/runtime/v447_actor_source_router.hpp"')

# The original packer hook is already activated and captures V443's native
# actor/packet certificate. Publish a *source-only* ticket after V443's event.
# Narrowed to the authenticated A=B steady path recorded for RX34 row30/31.
shared = '''        if(donor_ok && g_hook_source_tls.valid && w.status==9u &&
           w.packet && w.entity && w.dp && w.model &&
           w.dp_a==w.dp_b && w.dp_beta==0u &&
           g_hook_source_tls.source.native_producer_sequence!=0u &&
           g_hook_source_tls.source.row_id_a==(w.dp_a & 0xffu) &&
           g_hook_source_tls.source.row_id_b==(w.dp_b & 0xffu)) {
            std::uintptr_t ctx=0u;
            if(v443::read(static_cast<std::uintptr_t>(w.packet),ctx) && ctx){
                v447::source_ticket ticket{};
                ticket.sequence=g_hook_source_tls.source.native_producer_sequence;
                ticket.entity=static_cast<std::uintptr_t>(w.entity);
                ticket.drawparam=static_cast<std::uintptr_t>(w.dp);
                ticket.packet=static_cast<std::uintptr_t>(w.packet);
                ticket.context=ctx;
                ticket.model=static_cast<std::uintptr_t>(w.model);
                // No constructor/destructor generation certificate yet.
                ticket.actor_generation=0u;
                ticket.native_a=static_cast<std::uint16_t>(w.dp_a);
                ticket.native_b=static_cast<std::uint16_t>(w.dp_b);
                ticket.beta_bits=static_cast<std::uint32_t>(w.dp_beta);
                ticket.row_a=g_hook_source_tls.source.row_id_a;
                ticket.row_b=g_hook_source_tls.source.row_id_b;
                const auto verdict=v447::observations().observe(ticket);
                v438::emit(v438::V447_NATIVE_TICKET,{
                    ticket.sequence,ticket.entity,ticket.drawparam,
                    ticket.packet,ticket.context,ticket.model,
                    ticket.native_a,ticket.native_b,ticket.row_a,
                    ticket.row_b,ticket.beta_bits,
                    static_cast<unsigned>(verdict),ticket.actor_generation});
            }
        }
'''
# Two distinct original packer functions; both V443 snippets include the
# actor-native event and end with exactly one unique source/serial suffix.
single='''            v438::p(source),(donor_ok&&g_hook_source_tls.valid)?g_hook_source_tls.serial:0u});
    }
#endif
}'''
blend='''            v438::p(source_a),v438::p(source_b),
            (donor_ok&&g_hook_source_tls.valid)?g_hook_source_tls.serial:0u});
    }
#endif
}'''
patch_once(source,single,single.replace('    }\n#endif\n}',shared+'    }\n#endif\n}'))
patch_once(source,blend,blend.replace('    }\n#endif\n}',shared+'    }\n#endif\n}'))

# Consumer has only FrpgDrawContext and Source1 producer serial, NOT native
# entity/actor generation/packet. Resolve as a forensic join and leave those
# fields zero: the registry MUST return missing_receiver_identity, not PASS.
needle='''        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::ENV_ISSUED,{
'''
extra='''        dsrrl::runtime::v447::receiver_ticket rx447{};
        rx447.native_sequence=prepared.stable_envdiffuse_native_producer_sequence;
        rx447.context=material.model_owner_instance;
        rx447.row_a=prepared.stable_envdiffuse_row;
        const auto binding447=dsrrl::runtime::v447::observations().resolve(rx447);
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V447_RX34_BIND,{
            rx447.native_sequence,rx447.context,rx447.row_a,
            static_cast<unsigned>(binding447.state),
            binding447.producer.entity,binding447.producer.drawparam,
            binding447.producer.packet,binding447.producer.context,
            binding447.producer.model,binding447.producer.native_a,
            binding447.producer.native_b,binding447.producer.row_a,
            binding447.producer.row_b,
            static_cast<unsigned>(prepared.source_kind),
            decision.receiver_id,material.route_index,
            material.material_slot,dsrrl::runtime::v438::p(prepared.shader)});
'''
patch_once(integrated,needle,extra+needle)
patch_once(source,
           '"[DSRRL V443] PACKER_ACTOR_OBSERVER read_only=1 stage=UNVERIFIED_RUNTIME");',
           '"[DSRRL V443] PACKER_ACTOR_OBSERVER read_only=1 stage=UNVERIFIED_RUNTIME");\n'
           '    reshade::log::message(reshade::log::level::warning,\n'
           '        "[DSRRL V447] ACTOR_SOURCE_BIND_SHADOW source_by_native_seq=1 receiver_actor=OPEN source1_unchanged=1");')
print('V447 overlay: packer native actor ticket + RX34 source serial join, STRICT SHADOW ONLY')
