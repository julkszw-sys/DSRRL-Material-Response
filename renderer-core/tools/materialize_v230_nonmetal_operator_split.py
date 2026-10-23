#!/usr/bin/env python3
"""Operator-local salvage of certified PTDE Normal after failed nonmetal EnvSpec.

Never fallback generic Material Response / Diffuse / SpecRGB when the atomic
EnvSpec owner has failed: that creates a known hybrid. Preserve all successful
atomic draws, exact P_Metal, Ps_Body protected body and HemEnvLerp unchanged.
Only on an actual denied nonmetal stable draw may the separately verified
normal_t2 resource operator be dispatched with its own D3D transaction.
"""
from pathlib import Path
ROOT=Path("renderer-core")
PHYS=Path("build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp")
MARK="V230_NONMETAL_NORMAL_SPLIT_OPERATOR_LOCAL"
NEEDLE="""#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)
            dsrrl::runtime::v287_nonmetal_trace::log(
                "atomic_prepare_fail",
                material, decision,
                static_cast<std::uint32_t>(envspec_family));
#endif
            return false;"""
REPLACEMENT="""#if defined(DSRRL_V285_ATOMIC_NONMETAL_DIAG)
            dsrrl::runtime::v287_nonmetal_trace::log(
                "atomic_prepare_fail",
                material, decision,
                static_cast<std::uint32_t>(envspec_family));
#endif
#if defined(DSRRL_V230_NONMETAL_NORMAL_OPERATOR_SPLIT)
            // Atomic EnvSpec failed. It MUST NOT become an MR+stock-EnvSpec
            // hybrid. Only the independent normal-map t2 operator is allowed.
            // P_Metal and Ps_Body remain on their previously protected paths.
            const bool independent_normal_scope =
                !hemenvlerp_bound && context != nullptr &&
                material.valid && material.owner_tuple_exact &&
                material.actual_material_exact && material.material_slot_valid &&
                material.route_index != 345u &&
                material.semantic_name_hash !=
                    dsrrl::operators::material_response::mtd_semantic_hash(
                        "P_Metal[DSB].mtd") &&
                material.semantic_name_hash !=
                    dsrrl::operators::material_response::mtd_semantic_hash(
                        "Ps_Body[DSB].mtd") &&
                decision.active && decision.receiver_id >= 33u &&
                decision.receiver_id <= 35u;
            if (independent_normal_scope) {
                dsrrl::operators::material_response::mtd_semantic_query query{};
                query.material = material;
                query.receiver_id = decision.receiver_id;
                query.ownership.flver_sha256 = material.flver_sha256;
                query.ownership.flver_identity_hash = material.flver_identity_hash;
                query.ownership.material_slot = material.material_slot;
                query.ownership.material_slot_valid = material.material_slot_valid;
                query.ownership.exact = material.owner_tuple_exact;
                dsrrl::runtime::prepared_material_resource_draw independent{};
                // Both false: NO SpecRGB and NO dependent PTDE Diffuse c100.
                const bool prepared_normal =
                    g_material_resources.prepare_draw_requests(
                        context, decision.receiver_id, query,
                        false, false, independent);
                const bool exactly_one_normal =
                    prepared_normal && independent.normal &&
                    !independent.spec_rgb && !independent.diffuse &&
                    independent.request_count == 1u &&
                    independent.requests[0].primary ==
                        dsrrl::core::operator_id::normal &&
                    independent.requests[0].srv_count == 1u &&
                    independent.requests[0].srvs[0].slot == 2u &&
                    independent.requests[0].srvs[0].srv != nullptr &&
                    !independent.requests[0].replace_pixel_shader &&
                    independent.requests[0].constant_buffer_count == 0u;
                if (exactly_one_normal) {
                    prepared.resources = independent;
                    independent = {};
                    if (dsrrl::runtime::append_island_draw_request(
                            prepared.batch, prepared.resources.requests[0]) ==
                        dsrrl::runtime::island_draw_batch_result::ready) {
                        // %MARK%: only normal-resource request, never EnvSpec
                        // MR, SpecRGB or dependent Diffuse. D3D restore is
                        // performed by the normal draw batch transaction.
                        return true;
                    }
                    release_prepared_island_batch(prepared);
                    return false;
                }
                g_material_resources.release_prepared_draw(independent);
            }
#endif
            return false;""".replace("%MARK%",MARK)
for p in (ROOT/"integrated/integrated_addon.cpp", PHYS):
    s=p.read_text("utf-8")
    pos=s.find("if (!g_pmetal_envspec.prepare(")
    tail=s.find("            return false;",pos)
    safe=pos>=0 and tail>pos and tail-pos<1500
    if not safe or MARK in s:
        raise RuntimeError(f"INDEPENDENT_OPERATOR_SPLIT_DRIFT path={p} pos={pos} tail={tail}")
    fallback=REPLACEMENT.split("#if defined(DSRRL_V230_NONMETAL_NORMAL_OPERATOR_SPLIT)",1)[1].rsplit("#endif",1)[0]
    fallback="#if defined(DSRRL_V230_NONMETAL_NORMAL_OPERATOR_SPLIT)"+fallback+"#endif"
    s=s[:tail]+fallback+"\n"+s[tail:]
    p.write_text(s,"utf-8")
    print(f"V230_NORMAL_SPLIT_PHYSICAL_PATCH_PASS {p}")
print("V230_INDEPENDENT_NORMAL_SPLIT_SOURCE_PASS no MR/SpecRGB/Diffuse downgrade, exact owner, protected P_Metal/body/Lerp")
