#!/usr/bin/env python3
"""Add bounded read-only per-draw resource/semantic denial telemetry.

Runs AFTER materialize_v230_armor_v13_exact_supplement.py.
Source1 target is the independently owner-attested FLVER 3a82... RX34 slot2.
Armor samples are MTD C_Metal route229 slot0, or one of 34 certified V13
stock t1 identities. Never changes any operator action, resource or shader.
"""
from pathlib import Path
p=Path('renderer-core/src/runtime/material_resource_draw_runtime.cpp')
s=p.read_text(encoding='utf-8')
def patch(old,new):
 global s
 n=s.count(old)
 if n!=1:raise SystemExit(f'DUAL_FRONTIER_SOURCE_DRIFT {n} {old[:110]!r}')
 s=s.replace(old,new,1)

patch('''    const bool v13_owner =
''','''#if defined(DSRRL_V230_DUAL_FRONTIER)
    // Owner-first. The source1 FLVER fingerprint was confirmed in live RX34
    // trace; no assumption is made about unrelated scene/equipment draws.
    const bool diag_source1 =
        receiver_id == 34u &&
        query.material.route_index == 345u &&
        query.material.material_slot_valid &&
        query.material.material_slot == 2u &&
        operators::legacy_plan::hashing::matches_hex(
            query.ownership.flver_sha256,
            "3a82e4f6e45f13a05ac40418ba6ad12ff9acae9cf01e19937bd5c5f45ff2636c") &&
        operators::legacy_plan::hashing::matches_hex(
            query.material.raw_mtd_sha256,
            "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b");
    const bool diag_helmet =
        query.material.route_index == 229u &&
        query.material.material_slot_valid &&
        query.material.material_slot == 0u &&
        query.material.semantic_name_hash ==
            operators::material_response::mtd_semantic_hash("C_Metal[DSB].mtd");
    bool diag_v13_candidate=false;
    for (const auto &pair : armor_v13::p)
        if (pair.s == h1) { diag_v13_candidate=true; break; }
    static std::atomic<std::uint32_t> s_source1_logs{0u};
    static std::atomic<std::uint32_t> s_helm_logs{0u};
    static std::atomic<std::uint32_t> s_v13_logs{0u};
    const auto report = [&](const char *phase, unsigned reason, unsigned action) noexcept {
        if (!diag_source1 && !diag_helmet && !diag_v13_candidate) return;
        auto &counter=diag_source1 ? s_source1_logs :
            diag_helmet ? s_helm_logs : s_v13_logs;
        const auto n=counter.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n>90u)return;
        const auto spec_sem=operators::material_response::classify_mtd_semantic(
            query,operators::material_response::mtd_semantic_operator::spec_rgb);
        const auto diffuse_sem=operators::material_response::classify_mtd_semantic(
            query,operators::material_response::mtd_semantic_operator::diffuse);
        const auto normal_sem=operators::material_response::classify_mtd_semantic(
            query,operators::material_response::mtd_semantic_operator::normal_bump);
        char msg[680]{};
        std::snprintf(msg,sizeof(msg),
          "[DSRRL DUAL FRONTIER] kind=%s phase=%s n=%u rx=%u route=%u slot=%u own=%u own_census=%u active=%u full_mr=%u spec_consumer=%u feature_spec=%u h0=%016llx h1=%016llx h2=%016llx v13pair=%u v13triple=%u t1sidecar=%u dsidecar=%u nsidecar=%u sem_spec=%u sem_diff=%u sem_norm=%u spec_reason=%u spec_action=%u prepared_spec=%u prepared_diff=%u prepared_norm=%u",
          diag_source1?"source1":diag_helmet?"helmet":"v13_other",phase,n,
          static_cast<unsigned>(receiver_id),
          static_cast<unsigned>(query.material.route_index),
          static_cast<unsigned>(query.material.material_slot),
          query.material.owner_tuple_exact?1u:0u,
          operators::material_response::has_exact_flver_material_ownership(query)?1u:0u,
          exact_material?1u:0u,
          full_material_response_ready?1u:0u,
          spec_rgb_consumer_ready?1u:0u,
          core_.features().enabled(core::operator_id::spec_rgb)?1u:0u,
          static_cast<unsigned long long>(h0),
          static_cast<unsigned long long>(h1),
          static_cast<unsigned long long>(h2),
          armor_v13::pair_ok(h1,h0)?1u:0u,
          armor_v13::triple_ok(h0,h1,h2)?1u:0u,
          companions[1]?1u:0u,companions[0]?1u:0u,companions[2]?1u:0u,
          static_cast<unsigned>(spec_sem.state),
          static_cast<unsigned>(diffuse_sem.state),
          static_cast<unsigned>(normal_sem.state),reason,action,
          prepared.spec_rgb?1u:0u,prepared.diffuse?1u:0u,prepared.normal?1u:0u);
        reshade::log::message(reshade::log::level::warning,msg);
    };
    report("lookup",255u,255u);
#else
    const auto report=[](const char *,unsigned,unsigned) noexcept {};
#endif

    const bool v13_owner =
''')
patch('''#endif

        if (decision.action ==
                operators::resource_bridges::
                    spec_rgb_action::''','''#endif
        report("spec_decision",static_cast<unsigned>(decision.reason),
               static_cast<unsigned>(decision.action));
        if (decision.action ==
                operators::resource_bridges::
                    spec_rgb_action::''')
patch('''    const bool bmp_receiver =
        receiver_id >= 24u &&''','''    report("post_spec",254u,254u);
    const bool bmp_receiver =
        receiver_id >= 24u &&''')
patch('''    const bool normal_tuple =
        h0 != 0u &&''','''    report("post_diffuse",253u,253u);
    const bool normal_tuple =
        h0 != 0u &&''')
patch('''    for (auto *&companion : companions)
        release_view(companion);''','''    report("post_normal",252u,252u);
    for (auto *&companion : companions)
        release_view(companion);''')
p.write_text(s,encoding='utf-8')
print('DUAL_FRONTIER_SOURCE_PATCH_PASS exact owner source1/helmet + V13 t1; no operator state changes')
