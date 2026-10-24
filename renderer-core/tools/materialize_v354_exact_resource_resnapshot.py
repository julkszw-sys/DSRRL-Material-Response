#!/usr/bin/env python3
"""V354 verified resnapshot: retry rejected shadow SRVs through exact native D3D11 bindings.
Apply after V351/V352/V353 materializers, before CMake source generation.
NO relaxed identity/MTD/receiver/sidecar/BC6H atlas checks. No stock DSR false PASS.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/"src/runtime/pmetal_envspec_draw_runtime.cpp"
s=p.read_text("utf8")

def once(before,after,label):
    global s
    count=s.count(before)
    if count!=1:
        raise RuntimeError(f"V354 source anchor {label}: {count} matches")
    s=s.replace(before,after,1)

once("""    const bool env_resource_ready =
        k_native_dsr_cubemap_feed""",
"""    bool env_resource_ready =
        k_native_dsr_cubemap_feed""","mutable env resource")
once("""                : env_resources_.prepare(
                      context,
                      env_semantics.envspc_slot,
                      probe_b_required,
                      prepared.env_resources);

    if (!env_resource_ready) {""",
"""                : env_resources_.prepare(
                      context,
                      env_semantics.envspc_slot,
                      probe_b_required,
                      prepared.env_resources);

#if defined(DSRRL_V354_EXACT_RESOURCE_RESNAPSHOT)
    // A tracked shadow binding can outlive the current command-list state.
    // Native PSGetShaderResources obtains the draw's actual slots, but must
    // still pass the SAME PTDE probe SHA and exact slot gates in prepare().
    // Only retry on a reject. Never translate an unknown stock probe to a
    // visually similar PTDE probe, and never touch native-Dsr diagnostic mode.
    if (!env_resource_ready &&
        shadow_env_ready &&
        !k_native_dsr_cubemap_feed) {
        env_resource_ready = env_resources_.prepare(
            context,
            env_semantics.envspc_slot,
            probe_b_required,
            prepared.env_resources);
        if (env_resource_ready) {
            static std::atomic_bool first_recovery{false};
            if (!first_recovery.exchange(true, std::memory_order_relaxed))
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL V354 RESOURCE RESNAPSHOT]"
                    " stage=envspec_native_exact_sha_rescued"
                    " source=shadow_rejected native=authoritative"
                    " identity_guard=RETAINED pixel=OPEN");
        }
    }
#endif

    if (!env_resource_ready) {""","probe shadow native rescue")

once("""    const bool material_ready =
        shadow_material_ready""",
"""    bool material_ready =
        shadow_material_ready""","mutable material")
# Preserve exact negative chr-stock guard before native SpecRGB resnapshot:
# a verified character resource must never be retried as player equipment.
character_stock_guard = ""
marker = "    // Source-censused chr-bound stock t1 is not a missing player equipment"
if marker in s:
    start = s.index(marker)
    end = s.index("    if (!material_ready ||", start)
    character_stock_guard = s[start:end]

once("""                      true,
                      prepared.material_resources);

""" + character_stock_guard + """    if (!material_ready ||
        !prepared.material_resources.spec_rgb) {""",
"""                      true,
                      prepared.material_resources);

""" + character_stock_guard + """#if defined(DSRRL_V354_EXACT_RESOURCE_RESNAPSHOT)
    if (shadow_material_ready &&
        (!material_ready ||
         !prepared.material_resources.spec_rgb)) {
        // Do not leave a partial diffuse/normal request in the batch before
        // trying the real native SRV bindings. The same immutable certified
        // raw MTD/owner/receiver and sidecar/stock-hash gates remain required.
        material_resources_.release_prepared_draw(
            prepared.material_resources);
        material_ready = material_resources_.prepare_draw_requests(
            context,
            decision.receiver_id,
            query,
            true,
            true,
            prepared.material_resources);
        if (material_ready &&
            prepared.material_resources.spec_rgb) {
            static std::atomic_bool first_recovery{false};
            if (!first_recovery.exchange(true, std::memory_order_relaxed))
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL V354 RESOURCE RESNAPSHOT]"
                    " stage=spec_rgb_native_exact_sidecar_rescued"
                    " source=shadow_rejected native=authoritative"
                    " owner_mtd_allowlist=RETAINED pixel=OPEN");
        }
    }
#endif

    if (!material_ready ||
        !prepared.material_resources.spec_rgb) {
#if defined(DSRRL_V354_EXACT_RESOURCE_RESNAPSHOT)
        // Bounded per rx/slot rejection census, NEVER logs every draw and
        // never accepts a sidecar unless the normal exact gate passed.
        const auto rx_bit =
            decision.receiver_id >= 33u &&
            decision.receiver_id <= 35u
                ? decision.receiver_id - 33u : 3u;
        const auto slot_bit =
            material.material_slot_valid &&
            material.material_slot < 8u
                ? material.material_slot : 8u;
        const auto census_bit = 1u << ((rx_bit*9u+slot_bit)%32u);
        static std::atomic<std::uint32_t> logged_mask{0u};
        if ((logged_mask.fetch_or(
                census_bit,std::memory_order_relaxed)&census_bit)==0u) {
            const auto actual_spec =
                material_resources_.probe_exact_specular_companion(context);
            char line[512]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL V354 SPEC REJECT] rx=%u route=%u slot=%u"
                " shadow_ready=%u native_t1_bound=%u"
                " native_snapshot=%u native_hash=%016llx"
                " allowlisted=%u sidecar_ready=%u"
                " exact_owner=%u raw_mtd=%u"
                " resolution=NOT_AUTHORIZED_RELEASE_BLOCK",
                decision.receiver_id,decision.route_index,
                material.material_slot,
                shadow_material_ready?1u:0u,
                actual_spec.stock_bound?1u:0u,
                actual_spec.snapshot_resolved?1u:0u,
                static_cast<unsigned long long>(actual_spec.logical_hash),
                actual_spec.logical_hash_allowed?1u:0u,
                actual_spec.companion_ready?1u:0u,
                material.owner_tuple_exact?1u:0u,
                material.actual_material_exact?1u:0u);
            reshade::log::message(reshade::log::level::info,line);
        }
#endif""","bounded spec reject census")
# preserve formatting of old source branch: opening new if block continues original code
once("""    if (!env_resource_ready) {
        if (shader != nullptr)
            shader->Release();""",
"""    if (!env_resource_ready) {
#if defined(DSRRL_V354_EXACT_RESOURCE_RESNAPSHOT)
        // Bounded receiver/slot diagnostic: never mutates or aliases a probe.
        const auto rx_key =
            decision.receiver_id >= 33u && decision.receiver_id <= 35u
                ? decision.receiver_id - 33u : 3u;
        const auto census_bit = 1u << (rx_key*4u +
            static_cast<std::uint32_t>(env_semantics.envspc_slot));
        static std::atomic<std::uint32_t> logged{0u};
        if ((logged.fetch_or(census_bit,std::memory_order_relaxed)&census_bit)==0u) {
            ID3D11ShaderResourceView *native_view[3]{};
            context->PSGetShaderResources(
                12u,probe_b_required?3u:1u,native_view);
            std::uint16_t probe_a=0u,probe_b=0u;
            const bool native_a_known =
                v354_native_probe_for_view(native_view[0],probe_a);
            const bool native_b_known =
                !probe_b_required ? native_a_known :
                v354_native_probe_for_view(native_view[2],probe_b);
            const auto stats=env_resources_.telemetry();
            char line[512]{};
            std::snprintf(line,sizeof(line),
                "[DSRRL V354 PROBE REJECT] rx=%u route=%u slot=%u"
                " shadow_ready=%u blend=%u pack_ready=%u sampler_ready=%u"
                " native_t12_bound=%u native_t14_bound=%u"
                " native_a_known=%u native_a_probe=%u"
                " native_b_known=%u native_b_probe=%u"
                " native_matched=%llu views=%llu cube_created=%llu"
                " cube_fail=%llu resolution=NOT_AUTHORIZED_RELEASE_BLOCK",
                decision.receiver_id,decision.route_index,
                static_cast<unsigned>(env_semantics.envspc_slot),
                shadow_env_ready?1u:0u,probe_b_required?1u:0u,
                stats.pack_ready?1u:0u,stats.sampler_ready?1u:0u,
                native_view[0]!=nullptr?1u:0u,
                native_view[2]!=nullptr?1u:0u,
                native_a_known?1u:0u,static_cast<unsigned>(probe_a),
                native_b_known?1u:0u,static_cast<unsigned>(probe_b),
                static_cast<unsigned long long>(stats.native_matches),
                static_cast<unsigned long long>(stats.view_matches),
                static_cast<unsigned long long>(stats.cube_created),
                static_cast<unsigned long long>(stats.cube_fail));
            reshade::log::message(reshade::log::level::info,line);
            for (auto *&view : native_view) {
                if (view != nullptr) {
                    view->Release();
                    view=nullptr;
                }
            }
        }
#endif
        if (shader != nullptr)
            shader->Release();""","bounded probe reject diagnostics")

p.write_text(s,encoding="utf8")
print("V354_EXACT_RESNAPSHOT_OPERATOR_SAFE_SOURCE_PASS")


h=root/"include/dsrrl/runtime/envspec_resource_runtime.hpp"
hdr=h.read_text(encoding="utf8")
anchor="namespace dsrrl::runtime {\n"
assert hdr.count(anchor)==1,"v354 envspec resource header drift"
hdr=hdr.replace(anchor,anchor+"""#if defined(DSRRL_V354_EXACT_RESOURCE_RESNAPSHOT)
// Diagnostic-only lookup in existing SHA256 certified BC6H native atlas.
bool v354_native_probe_for_view(
    ID3D11ShaderResourceView *view,
    std::uint16_t &probe) noexcept;
#endif

""",1)
h.write_text(hdr,encoding="utf8")

q=root/"src/runtime/envspec_resource_runtime.cpp"
resource=q.read_text(encoding="utf8")
anchor="} // namespace\n\nenvspec_resource_runtime::~envspec_resource_runtime()"
assert resource.count(anchor)==1,"V354 native view probe hook anchor drift"
resource=resource.replace(anchor,"""} // namespace

#if defined(DSRRL_V354_EXACT_RESOURCE_RESNAPSHOT)
bool v354_native_probe_for_view(
    ID3D11ShaderResourceView *view,
    std::uint16_t &probe) noexcept
{
    return probe_for_native_view(view,probe);
}
#endif

envspec_resource_runtime::~envspec_resource_runtime()""",1)
q.write_text(resource,encoding="utf8")
print("V354_EXACT_NATIVE_PROBE_VIEW_CENSUS_PASS")
