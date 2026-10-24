#!/usr/bin/env python3
"""V314 non-invasive genuine PS shader/MTD/FLVER receiver discovery.

Attach real ReShade pipeline SHA to stock-unrouted original MTD source.
Never authorize or replace a pipeline; no generic "Lit is RX33" guess.
After V313 full source/build materialization only.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=r/"integrated/integrated_addon.cpp"
s=p.read_text("utf-8")
def replace(a,b,label):
    global s
    assert s.count(a)==1, (label,s.count(a))
    s=s.replace(a,b,1)
def replace_twice(a,b,label):
    global s
    assert s.count(a)==2,(label,s.count(a))
    s=s.replace(a,b)
replace('#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"',
'''#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#if defined(DSRRL_V314_UNROUTED_LIT_MUL_CENSUS)
#include "dsrrl/runtime/v314_unrouted_ps_receiver_census.hpp"
#endif''',"include")
replace('''    std::uint8_t draw_route_mask = 0u;

    if (g_a1_bridge.pipeline_attested(''',
'''#if defined(DSRRL_V314_UNROUTED_LIT_MUL_CENSUS)
    if (pixel_shader != nullptr &&
        pixel_shader->code != nullptr &&
        pixel_shader->code_size != 0u)
        dsrrl::runtime::v314::observe_pipeline(
            pipeline.handle,pixel_shader->code,pixel_shader->code_size);
#endif
    std::uint8_t draw_route_mask = 0u;

    if (g_a1_bridge.pipeline_attested(''',"actual PS create SHA")
replace('''void on_destroy_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline pipeline)
{
    if (k_drawtime_islands_runtime_enabled) {''',
'''void on_destroy_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline pipeline)
{
#if defined(DSRRL_V314_UNROUTED_LIT_MUL_CENSUS)
    dsrrl::runtime::v314::forget_pipeline(pipeline.handle);
#endif
    if (k_drawtime_islands_runtime_enabled) {''',"destroy")
replace('''    if (!k_drawtime_islands_runtime_enabled)
        return;

    auto route_mask =
        observe_integrated_draw_route_bind(''',
'''    if (!k_drawtime_islands_runtime_enabled)
        return;

#if defined(DSRRL_V314_UNROUTED_LIT_MUL_CENSUS)
    if(pixel_stage_bound)
        dsrrl::runtime::v314::observe_bind(cmd_list,pipeline.handle);
#endif
    auto route_mask =
        observe_integrated_draw_route_bind(''',"bind real PS pipeline")
replace_twice('''#if defined(DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS)
        // Capture sidecar-bound draws *outside* current shader families,''',
'''#if defined(DSRRL_V314_UNROUTED_LIT_MUL_CENSUS)
        // On this route, the stock path already discards the selector
        // identity below. Consume ONLY to record actual original MTD/FLVER
        // provenance against exact DSR runtime PS SHA. No draw replay,
        // no CB/SRV/PS writing, no shader-family authorization.
        dsrrl::operators::material_response::material_identity diagnostic{};
        (void)dsrrl::runtime::material_owner_selection_consume(diagnostic);
        ID3D11ShaderResourceView *census_t1[1]{};
        const bool stock_t1=
            dsrrl::runtime::pixel_srv_shadow_snapshot(
                cmd_list,1u,1u,census_t1) &&
            census_t1[0]!=nullptr;
        dsrrl::runtime::v314::report_stock_unrouted(
            cmd_list,diagnostic,stock_t1);
#endif
#if defined(DSRRL_V290_FULL_SIDECAR_DRAW_CENSUS)
        // Capture sidecar-bound draws *outside* current shader families,''',"unrouted read-only stage")
p.write_text(s,"utf-8")
print("V314_ACTUAL_SHADER_SHA_RX_CENSUS_INTEGRATED_STOCK_FAIL_OPEN_PASS")
