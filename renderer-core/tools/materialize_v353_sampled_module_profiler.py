#!/usr/bin/env python3
"""V353 per-subsystem sampled profiling patch. Apply after V351 then V352.
No material, shader, source donor, resource state or draw transaction change.
QPC on only 1/128 calls per stage/thread, results every V352 30s window.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
hdr='#include "dsrrl/runtime/v353_sampled_cpu_module_profiler.hpp"'
files={
    "integrated/integrated_addon.cpp":[
        ('bool prepare_island_batch(\n    reshade::api::command_list *cmd_list,', 'receiver_material_batch'),
        ('bool on_draw(\n    reshade::api::command_list *cmd_list,','draw_regular'),
        ('bool on_draw_indexed(\n    reshade::api::command_list *cmd_list,','draw_indexed'),
    ],
    "src/runtime/material_owner_selection.cpp":[
        ('bool material_owner_selection_publish(\n    const operators::material_response::material_identity &identity) noexcept','flver_owner_route')],
    "src/operators/material_response/material_response_island.cpp":[
        ('decision material_response_island::evaluate(\n    std::uint32_t receiver_id,','material_decision')],
    "src/runtime/material_response_draw_transaction.cpp":[
        ('bool material_response_draw_runtime::prepare_draw_request(\n    const operators::material_response::decision &decision,','material_cb_shader_prepare')],
    "src/runtime/pmetal_env_source_runtime.cpp":[
        ('bool pmetal_env_source_runtime::latest(\n    const operators::material_response::','lightbank_source')],
    "src/runtime/pmetal_envspec_draw_runtime.cpp":[
        ('bool pmetal_envspec_draw_runtime::prepare(\n    reshade::api::command_list *cmd_list,\n    const mr::material_identity &material,\n    const mr::decision &decision,\n    pmetal_envspec_receiver_family family,','envspec_full_prepare')],
    "src/runtime/envspec_resource_runtime.cpp":[
        ('bool envspec_resource_runtime::prepare_bound(\n    ID3D11ShaderResourceView *stock_a,','cubemap_resource'),
        ('bool envspec_resource_runtime::prepare_native_dsr(\n', 'cubemap_resource')],
    "src/runtime/material_resource_draw_runtime.cpp":[
        ('bool material_resource_draw_runtime::\nprepare_draw_requests_bound(\n', 'material_texture_sidecars')],
    "src/runtime/island_draw_adapter.cpp":[
        ('island_draw_dispatch_result dispatch_island_draw_batch(\n', 'draw_transaction'),
        ('island_draw_dispatch_result dispatch_island_draw_indexed_batch(\n', 'draw_transaction')],
}
for rel,targets in files.items():
    p=root/rel
    s=p.read_text("utf-8")
    include=s.find('#include ')
    if include<0: raise RuntimeError(f"missing includes {rel}")
    line_end=s.find('\n',include)
    if hdr not in s:
        s=s[:line_end]+ '\n#if defined(DSRRL_V353_CPU_MODULE_PROFILER)\n'+hdr+'\n#endif'+s[line_end:]
    for signature,stage in targets:
        if s.count(signature)!=1:
            raise RuntimeError(f"{rel}: expected unique {stage} signature, got {s.count(signature)}")
        start=s.index(signature)
        open_brace=s.find('\n{\n',start)
        next_func=s.find('\n}\n',start)
        if open_brace<0 or (next_func>0 and open_brace>next_func):
            raise RuntimeError(f"{rel} {stage}: function opening not found")
        insert='''\n#if defined(DSRRL_V353_CPU_MODULE_PROFILER)
    dsrrl::runtime::v353::scoped_sample v353_sample(
        dsrrl::runtime::v353::stage::%s);
#endif'''%stage
        s=s[:open_brace+2]+insert+s[open_brace+2:]
    p.write_text(s,encoding="utf-8")
    print("V353_INSTRUMENTED",rel,len(targets))

p=root/"integrated/integrated_addon.cpp"
s=p.read_text("utf-8")
mark='''#if defined(DSRRL_V352_LONG_SOAK_PRESENT)
dsrrl::runtime::v352::long_soak_present_monitor g_v352_soak{};
#endif'''
if s.count(mark)!=1:raise RuntimeError("V352 monitor not available for V353")
s=s.replace(mark,mark+'''
#if defined(DSRRL_V353_CPU_MODULE_PROFILER)
std::uint64_t g_v353_window_index=0u;
#endif''',1)
mark='''#if defined(DSRRL_V352_LONG_SOAK_PRESENT)
    g_v352_soak.reset();
#endif'''
if s.count(mark)!=1:raise RuntimeError("V352 reset anchor changed")
s=s.replace(mark,mark+'''
#if defined(DSRRL_V353_CPU_MODULE_PROFILER)
    dsrrl::runtime::v353::reset();
    g_v353_window_index=0u;
#endif''',1)
mark='''    if (g_effect_telemetry_enabled &&
        (present == 1u || telemetry_window))
        log_effect_matrix("LIVE");'''
if s.count(mark)!=1:raise RuntimeError("V352 log window anchor changed")
s=s.replace(mark,mark+'''
#if defined(DSRRL_V353_CPU_MODULE_PROFILER)
    if (telemetry_window) {
        dsrrl::runtime::v353::report_window(
            g_v352_soak.last_window_seconds(),
            ++g_v353_window_index);
    }
#endif''',1)
p.write_text(s,encoding="utf-8")

p=root/"include/dsrrl/runtime/v352_long_soak_present_monitor.hpp"
s=p.read_text("utf-8")
needle='''    // Returns true only once per elapsed 30s reporting window.'''
if s.count(needle)!=1:raise RuntimeError("V352 monitor public insertion drift")
s=s.replace(needle,'''    double last_window_seconds() const noexcept { return last_window_s_; }

'''+needle,1)
needle='''        const double fps =
            static_cast<double>(window_frames_) / window_s;'''
if s.count(needle)!=1:raise RuntimeError("V352 computed window unavailable")
s=s.replace(needle,'''        last_window_s_ = window_s;
'''+needle,1)
needle='''    std::uint64_t first_tick_ = 0;'''
if s.count(needle)!=1:raise RuntimeError("V352 member drift")
s=s.replace(needle,'''    double last_window_s_ = 0.;
'''+needle,1)
needle='''        frequency_ = 0;
        emitted_start_ = false;'''
if s.count(needle)!=1:raise RuntimeError("V352 reset drift")
s=s.replace(needle,'''        frequency_ = 0;
        last_window_s_ = 0.;
        emitted_start_ = false;''',1)
p.write_text(s,encoding="utf-8")
print("V353_PER_MODULE_SAMPLED_TIMERS_AND_30S_REPORTING_APPLIED")
