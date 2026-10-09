#!/usr/bin/env python3
"""Route R43 P_Metal draw source by actual bound GI probe map, not global latest."""
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
s=p.read_text()
def sub(a,b):
 global s
 assert s.count(a)==1,(a[:60],s.count(a))
 s=s.replace(a,b,1)
sub('#include "dsrrl/runtime/pixel_srv_shadow.hpp"',
    '#include "dsrrl/runtime/pixel_srv_shadow.hpp"\n#include "dsrrl/runtime/pmetal_map_source.hpp"')
old="""    pmetal_envspec_source source{};
    if (!source_.latest(material, source) ||
        !std::isfinite(source.beta)) {"""
new="""    pmetal_envspec_source source{};
#if defined(DSRRL_PMETAL_R43_MAP_SOURCE_JOIN)
    // The GI view already bound on THIS draw is authoritative for its map.
    // Read-only and before any PTDE b12/PS replacement. A global latest
    // LightBank snapshot never authorizes the source.
    ID3D11ShaderResourceView *map_views[3]{};
    const bool map_b_required =
        family == pmetal_envspec_receiver_family::hemenvlerp;
    bool map_bound = pixel_srv_shadow_snapshot(
        cmd_list,12u,map_b_required ? 3u : 1u,map_views);
    std::uint16_t map_probe_a=0u,map_probe_b=0u;
    bool map_resolved =
        map_bound &&
        env_resources_.identify_bound_probe(
            map_views[0],map_b_required ? map_views[2] : nullptr,
            map_b_required,map_probe_a,map_probe_b);
    if (!map_resolved) {
        auto *map_ctx = reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
        if (map_ctx != nullptr) {
            ID3D11ShaderResourceView *live_views[3]{};
            map_ctx->PSGetShaderResources(
                12u,map_b_required ? 3u : 1u,live_views);
            map_resolved=env_resources_.identify_bound_probe(
                live_views[0],map_b_required ? live_views[2] : nullptr,
                map_b_required,map_probe_a,map_probe_b);
            for(auto *&view:live_views) {
                if(view!=nullptr){view->Release();view=nullptr;}
            }
        }
    }
    const unsigned draw_map = map_resolved
        ? pmetal_map_source::probe_area(map_probe_a) : 0u;
    unsigned exact_bank_area=0u;
    const bool map_source_valid =
        draw_map != 0u &&
        (!map_b_required ||
         pmetal_map_source::probe_area(map_probe_b) == draw_map) &&
        pmetal_map_source::latest_for_probe(
            material,draw_map,source,exact_bank_area);
    if (!map_source_valid || !std::isfinite(source.beta)) {
#else
    if (!source_.latest(material, source) ||
        !std::isfinite(source.beta)) {
#endif"""
sub(old,new)
needle="""    effect_latch(effect_probe_ready_);

    if (!g_resource_mode_logged.exchange("""
replacement="""#if defined(DSRRL_PMETAL_R43_MAP_SOURCE_JOIN)
    // Reconfirm the exact resource identity at the actual consumer point.
    // If shader resources changed while preparing the draw, never transport
    // even a valid source for a different map.
    if (pmetal_map_source::probe_area(
            prepared.env_resources.probe_a) != draw_map ||
        (probe_b_required &&
         pmetal_map_source::probe_area(
             prepared.env_resources.probe_b) != draw_map) ||
        !pmetal_map_source::source_in_area(source,exact_bank_area) ||
        !(exact_bank_area == draw_map || exact_bank_area == 99u ||
          exact_bank_area == 100u)) {
        if(shader!=nullptr)shader->Release();
        env_resources_.release(prepared.env_resources);
        telemetry::hot_count(probe_rejects_);
        effect_fail(effect_fail_mask_,k_effect_fail_probe);
        log_prepare_stage_once(1u<<14u,"material_map_probe_mismatch",
                               material,decision,family);
        return false;
    }
#endif
    effect_latch(effect_probe_ready_);

    if (!g_resource_mode_logged.exchange("""
sub(needle,replacement)
p.write_text(s)
print("PASS R43 map-qualified consumer pre-b12 and post-prepare guard")
