#!/usr/bin/env python3
"""V438 source-only overlay for native packer + typed model selector."""
from pathlib import Path
from v438_wrapper_payload import WRAPPER_SOURCE
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'src/runtime/pmetal_env_source_runtime.cpp'
flver=ROOT/'src/runtime/flver_engine_hooks.cpp'
integrated=ROOT/'integrated/integrated_addon.cpp'

def patch(path,old,new):
    text=path.read_text(encoding='utf-8')
    if new in text:return
    n=text.count(old)
    if n!=1:raise RuntimeError(f'V438 drift {path}: {n} hits for {old[:90]!r}')
    path.write_text(text.replace(old,new,1),encoding='utf-8')

def patch_last(path,old,new):
    text=path.read_text(encoding='utf-8')
    if new in text:return
    if text.count(old)!=2:raise RuntimeError(f'V438 expected two draw occurrences in {path}')
    a,b=text.rsplit(old,1)
    path.write_text(a+new+b,encoding='utf-8')

patch(source,'#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"',
      '#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"\n'
      '#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)\n'
      '#include "dsrrl/runtime/v438_one_shot_trace.hpp"\n'
      '#include <intrin.h>\n#endif')

patch(source,'bool install_envspec_source_hooks(\n    std::uintptr_t base) noexcept',
      WRAPPER_SOURCE+'\nbool install_envspec_source_hooks(\n    std::uintptr_t base) noexcept')

patch(source,'''    return true;
}

bool uninstall_envspec_source_hooks() noexcept''',
'''#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    if(!v438_install_wrappers(base))
        reshade::log::message(reshade::log::level::error,
           "[DSRRL V438] WRAPPER_HOOKS_INCOMPLETE; do not certify invocation correlation");
#endif
    return true;
}

bool uninstall_envspec_source_hooks() noexcept''')

patch(source,'bool uninstall_envspec_source_hooks() noexcept\n{',
'''bool uninstall_envspec_source_hooks() noexcept
{
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    if(!v438_uninstall_wrappers()) {
        reshade::log::message(reshade::log::level::error,
           "[DSRRL V438] WRAPPER_HOOK_RESTORE_FAILED");
        return false;
    }
#endif''')

patch(source,'''    next.native_producer_sequence = serial;
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)''',
'''    next.native_producer_sequence = serial;
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    v438::emit(v438::PACKER,{v438::current.id,serial,
       v438::p(source_a),v438::p(source_b),
       static_cast<std::uint16_t>(selector_a),static_cast<std::uint16_t>(selector_b),
       row_a,row_b,bank_a,bank_b,v438::fbits(beta),
       v438::fbits(a.x),v438::fbits(a.y),v438::fbits(a.z),
       v438::fbits(b.x),v438::fbits(b.y),v438::fbits(b.z)});
#endif
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)''')

patch(source,'''    const auto character =
        descriptor[0x150u];''',
'''    const auto character =
        descriptor[0x150u];
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    v438::emit(v438::MODEL_SELECTOR,{v438::p(owner),v438::p(descriptor),rva,
        raw_a,raw_b,v438::fbits(beta),character,
        v438::current.id,v438::current.model,v438::current.entity});
#endif''')

patch(source,'''        pmetal_producer_state_publish(
            material,
            next,
            epoch);

#if defined(DSRRL_V411_OWNER_SEQUENCE_DIAG)''',
'''        pmetal_producer_state_publish(
            material,
            next,
            epoch);
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
        v438::emit(v438::SOURCE1_JOIN,{v438::p(owner),
            next.native_producer_sequence,epoch,
            v438::p(source_a),v438::p(source_b),
            static_cast<std::uint16_t>(endpoints.a),
            static_cast<std::uint16_t>(endpoints.b),
            v438::fbits(endpoints.beta),next.row_id_a,next.row_id_b,
            next.bank_signature_a,next.bank_signature_b});
#endif

#if defined(DSRRL_V411_OWNER_SEQUENCE_DIAG)''')

patch(source,'''    static std::atomic<std::uint64_t> native_miss{0u};''',
'''#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
    v438::emit(v438::SOURCE1_REJECT,{v438::p(owner),rva,
       raw_a,raw_b,v438::fbits(beta),
       v438::p(source_a),v438::p(source_b),epoch});
#endif
    static std::atomic<std::uint64_t> native_miss{0u};''')

patch(flver,'#include <vector>', '''#include <vector>
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
#include "dsrrl/runtime/v438_one_shot_trace.hpp"
#endif''')
patch(flver,'''             v415::record_selector(
                 model_addr,
                 reinterpret_cast<std::uintptr_t>(container),
                 reinterpret_cast<std::uintptr_t>(owner),
                 model_vtable,material_index,class_kind);''',
'''             v415::record_selector(
                 model_addr,
                 reinterpret_cast<std::uintptr_t>(container),
                 reinterpret_cast<std::uintptr_t>(owner),
                 model_vtable,material_index,class_kind);
#if defined(DSRRL_V438_ONE_SHOT_CAPTURE)
             v438::emit(v438::MODEL_SELECTOR,{model_addr,
                 v438::p(owner),v438::p(container),
                 rva,static_cast<std::uint32_t>(material_index),
                 class_kind,model_vtable,v438::last.id,
                 v438::last.model,v438::last.entity,
                 v438::last.caller});
#endif''')
