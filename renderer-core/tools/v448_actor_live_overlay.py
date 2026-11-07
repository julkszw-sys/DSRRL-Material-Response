#!/usr/bin/env python3
"""V448 overlay AFTER V447 read-only source overlay. No new hook sites.
Current V448 never changes Source1, R43, shader, CB, SRV or draw admission.
"""
from pathlib import Path
ROOT=Path('renderer-core')
source=ROOT/'src/runtime/pmetal_env_source_runtime.cpp'
flver=ROOT/'src/runtime/flver_engine_hooks.cpp'
integrated=ROOT/'integrated/integrated_addon.cpp'
trace=ROOT/'include/dsrrl/runtime/v438_one_shot_trace.hpp'

def replace_once(path,needle,new):
    s=path.read_text(encoding='utf-8')
    if s.count(needle)!=1:raise RuntimeError(f'V448 drift {path}: {s.count(needle)} occurrences for {needle[:75]}')
    path.write_text(s.replace(needle,new,1),encoding='utf-8')

replace_once(trace,'V447_RX34_BIND=17',
             'V447_RX34_BIND=17,V448_SELECTOR_FRAME=18,V448_ISSUED_FRAME=19')

# Make V448 core visible only inside the original read-only capture scope.
for p in (source,flver,integrated):
    include='#include "dsrrl/runtime/v448_live_builder_core.hpp"'
    native='#include "dsrrl/runtime/v448_live_builder_probe.hpp"'
    s=p.read_text(encoding='utf-8')
    if p==flver:
        needle='#include "dsrrl/runtime/v415_model_draw_identity.hpp"'
        replacement=needle+'\n#if defined(DSRRL_V448_LIVE_FRAME_SHADOW)\n'+include+'\n'+native+'\n#include "dsrrl/runtime/v438_one_shot_trace.hpp"\n#endif'
    elif p==source:
        needle='#include "dsrrl/runtime/v447_actor_source_router.hpp"'
        replacement=needle+'\n#if defined(DSRRL_V448_LIVE_FRAME_SHADOW)\n'+include+'\n'+native+'\n#endif'
    else:
        needle='#include "dsrrl/runtime/v447_actor_source_router.hpp"'
        replacement=needle+'\n#if defined(DSRRL_V448_LIVE_FRAME_SHADOW)\n'+include+'\n'+native+'\n#endif'
    replace_once(p,needle,replacement)

# Existing original packer hook, already V443/V447 certified: stage a same-
# thread source *before* the engine enters FlverModel::draw's material selector.
needle='''                const auto verdict=v447::observations().observe(ticket);
                v438::emit(v438::V447_NATIVE_TICKET,{'''
s=source.read_text(encoding='utf-8')
if s.count(needle)!=2:raise RuntimeError(f'V448 expected 2 V447 producer sites, got {s.count(needle)}')
s=s.replace(needle,'''                const auto verdict=v447::observations().observe(ticket);
#if defined(DSRRL_V448_LIVE_FRAME_SHADOW)
                if(verdict==v447::verdict::exact_actor_packet)
                    v448::tls.packer(ticket);
#endif
                v438::emit(v438::V447_NATIVE_TICKET,{''')
source.write_text(s,encoding='utf-8')

# The selector hook is ALREADY installed at an EXE-verified native callsite.
# Probe during the original model-builder frame, before it unwinds.
needle='''             v438::emit(v438::MODEL_SELECTOR,{model_addr,'''
addition='''#if defined(DSRRL_V448_LIVE_FRAME_SHADOW)
             if(v438::state().active.load(std::memory_order_acquire)) {
                 const auto *native_descriptor=static_cast<const std::uint8_t *>(
                      rva==k_ret_sel_1 || rva==k_ret_sel_3 ? r15 : r14);
                 std::uint16_t a448=0u,b448=0u;
                 if(native_descriptor &&
                    v443::read(reinterpret_cast<std::uintptr_t>(native_descriptor)+0x4Cu,a448) &&
                    v443::read(reinterpret_cast<std::uintptr_t>(native_descriptor)+0x4Eu,b448) &&
                    a448==b448 && (a448==1310u || a448==1311u)) {
                     const auto native448=v448::probe_native_builder(g_base);
                     v448::tls.select(native448.w,model_addr,material_index);
                     v438::emit(v438::V448_SELECTOR_FRAME,{
                         static_cast<unsigned>(native448.status),rva,
                         model_addr,reinterpret_cast<std::uintptr_t>(owner),
                         static_cast<std::uint64_t>(a448),
                         native448.w.entity,native448.w.dp,
                         native448.w.packet,native448.w.context,
                         native448.w.engine_model,native448.w.frame_rbp,
                         native448.w.a,native448.w.b,native448.w.beta_bits,
                         v448::tls.selector.sequence,
                         static_cast<std::uint32_t>(material_index)});
                 } else v448::tls.cancel();
             }
#endif
'''
replace_once(flver,needle,addition+needle)
# Same-thread one-shot material owner channel: on its *successful* consume,
# transport the live builder certificate for only the next issued draw.
needle='''    if(!material_owner_selection_consume(material)){
        telemetry::hot_count(g_owner_consume_misses);'''
replace_once(flver,needle,'''    if(!material_owner_selection_consume(material)){
#if defined(DSRRL_V448_LIVE_FRAME_SHADOW)
        v448::tls.cancel();
#endif
        telemetry::hot_count(g_owner_consume_misses);''')
needle='''    telemetry::hot_count(g_owner_consumed);
    return true;'''
replace_once(flver,needle,'''#if defined(DSRRL_V448_LIVE_FRAME_SHADOW)
    v448::tls.consume(material.model_owner_instance);
#endif
    telemetry::hot_count(g_owner_consumed);
    return true;''')

# The independent issued native stack is NOT taken from the producer cache.
# A missing/unwound frame yields a distinct failure status; V447 still emits.
needle='''        const auto binding447=dsrrl::runtime::v447::observations().resolve(rx447);
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V447_RX34_BIND,{'''
replace_once(integrated,needle,'''        const auto binding447=dsrrl::runtime::v447::observations().resolve(rx447);
#if defined(DSRRL_V448_LIVE_FRAME_SHADOW)
        const auto selector448=dsrrl::runtime::v448::tls.take(rx447.context);
        const auto issued448=dsrrl::runtime::v448::probe_native_builder(
            reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
        dsrrl::runtime::v448::issue_witness issue448{};
        issue448.builder=issued448.w;
        issue448.context=rx447.context;
        issue448.native_sequence=rx447.native_sequence;
        issue448.issued_row=rx447.row_a;
        const auto result448=dsrrl::runtime::v448::evaluate(
            binding447.producer,selector448,issue448);
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V448_ISSUED_FRAME,{
            rx447.native_sequence,rx447.context,rx447.row_a,
            static_cast<unsigned>(result448.state),selector448.sequence,
            selector448.builder.frame_rbp,selector448.builder.entity,
            selector448.builder.dp,selector448.builder.packet,
            selector448.builder.engine_model,selector448.selector_flver_model,
            static_cast<unsigned>(issued448.status),issued448.w.frame_rbp,
            issued448.w.entity,issued448.w.packet,issued448.w.engine_model,
            binding447.producer.sequence,binding447.producer.entity,
            binding447.producer.packet,binding447.producer.model,
            material.material_slot,material.route_index,
            dsrrl::runtime::v438::p(prepared.shader),
            // This flag MUST remain zero until native game runtime proves a
            // matching actor/frame on RX34; never enable renderer correction.
            0u});
#endif
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V447_RX34_BIND,{''')
needle='''        "[DSRRL V447] ACTOR_SOURCE_BIND_SHADOW source_by_native_seq=1 receiver_actor=OPEN source1_unchanged=1");'''
replace_once(source,needle,needle+'''
#if defined(DSRRL_V448_LIVE_FRAME_SHADOW)
    reshade::log::message(reshade::log::level::warning,
        "[DSRRL V448] LIVE_BUILDER_ACTOR_RX34_SHADOW enabled=1 patch=OFF generation=OPEN");
#endif''')
print('V448 applied after V447; native existing hooks only; actor-dependent LightBank bridge OFF')
