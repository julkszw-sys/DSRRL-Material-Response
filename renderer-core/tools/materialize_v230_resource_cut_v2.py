#!/usr/bin/env python3
"""V230 cut-local diagnostic: identity transport, sidecar state and early helmet owner.

Source-only overlay, applied AFTER armor V13 and DUAL frontier materializers.
No shader, resource binding, material semantic or existing deny/allow change.
"""
from pathlib import Path

root=Path("renderer-core/src/runtime")

def patch(path,old,new):
    p=root/path
    s=p.read_text(encoding="utf-8")
    n=s.count(old)
    if n!=1:
        raise RuntimeError(f"RESOURCE_CUT_DRIFT: {path}: {n}: {old[:100]!r}")
    p.write_text(s.replace(old,new,1),encoding="utf-8")

# Observe exact HELM route candidates at the entry of the EnvSpec draw
# runtime, BEFORE its route/feature checks; does not widen eligibility.
p="pmetal_envspec_draw_runtime.cpp"
patch(p,'''    prepared = {};
    effect_latch(effect_entry_seen_);''',
'''    prepared = {};
#if defined(DSRRL_V230_RESOURCE_CUT_V2)
    if ((material.route_index==229u || material.route_index==5u) &&
        material.material_slot_valid && material.material_slot==0u) {
        static std::atomic<std::uint32_t> helm_samples{0u};
        const auto n=helm_samples.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n<=32u) {
            char b[440]{};
            std::snprintf(b,sizeof(b),
                "[DSRRL HELM ENTRY CUT] n=%u rx=%u route=%u slot=%u valid=%u owner=%u actual=%u flver8=%02x%02x%02x%02x mtd8=%02x%02x%02x%02x sem=%016llx",
                n,decision.receiver_id,material.route_index,material.material_slot,
                material.valid?1u:0u,material.owner_tuple_exact?1u:0u,
                material.actual_material_exact?1u:0u,
                material.flver_sha256[0],material.flver_sha256[1],
                material.flver_sha256[2],material.flver_sha256[3],
                material.raw_mtd_sha256[0],material.raw_mtd_sha256[1],
                material.raw_mtd_sha256[2],material.raw_mtd_sha256[3],
                static_cast<unsigned long long>(material.semantic_name_hash));
            reshade::log::message(reshade::log::level::warning,b);
        }
    }
#endif
    effect_latch(effect_entry_seen_);''')

# This observer runs inside the real texture-name creation callback and
# records whether HD_M/HD_F_9360 textures are ever named, and whether the
# classified sidecar was loaded. It never authorizes a texture by name alone.
p="material_resource_draw_runtime.cpp"
patch(p,'''    if (!spec_member &&
        !diffuse_member &&
        !normal_member)
        return;''',
'''#if defined(DSRRL_V230_RESOURCE_CUT_V2)
    const bool helmet_texture =
        (logical_name_length == 9u ||
         logical_name_length == 11u) &&
        (_wcsnicmp(logical_name_raw,L"hd_m_9360",9u)==0 ||
         _wcsnicmp(logical_name_raw,L"hd_f_9360",9u)==0);
    if (helmet_texture) {
        static std::atomic<std::uint32_t> texture_samples{0u};
        const auto n=texture_samples.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n<=32u) {
            char b[360]{};
            std::snprintf(b,sizeof(b),
                "[DSRRL HELM TEX CUT] n=%u f=%u suffix=%u hash=%016llx spec_allow=%u diffuse_allow=%u normal_allow=%u",
                n,_wcsnicmp(logical_name_raw,L"hd_f_9360",9u)==0?1u:0u,
                logical_name_length==11u
                    ? (logical_name_raw[10]==L's'||logical_name_raw[10]==L'S'?1u:2u)
                    : 0u,
                static_cast<unsigned long long>(logical_hash),
                spec_member?1u:0u,diffuse_member?1u:0u,normal_member?1u:0u);
            reshade::log::message(reshade::log::level::warning,b);
        }
    }
#endif
    if (!spec_member &&
        !diffuse_member &&
        !normal_member)
        return;''')
patch(p,'''    // std::wstring move is noexcept: do not introduce allocations or''',
'''#if defined(DSRRL_V230_RESOURCE_CUT_V2)
    if (helmet_texture) {
        static std::atomic<std::uint32_t> ready_samples{0u};
        const auto n=ready_samples.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n<=32u) {
            char b[220]{};
            std::snprintf(b,sizeof(b),
                "[DSRRL HELM DDS CUT] n=%u hash=%016llx spec=%u diff=%u norm=%u",
                n,static_cast<unsigned long long>(logical_hash),
                set.specular?1u:0u,set.diffuse?1u:0u,set.normal?1u:0u);
            reshade::log::message(reshade::log::level::warning,b);
        }
    }
#endif
    // std::wstring move is noexcept: do not introduce allocations or''')
patch(p,'''    const auto h2 = hashes[2];
#if defined(DSRRL_V230_DUAL_FRONTIER)''',
'''    const auto h2 = hashes[2];
#if defined(DSRRL_V230_RESOURCE_CUT_V2)
    const bool rc_source1 =
        receiver_id==34u && query.material.route_index==345u &&
        query.material.material_slot_valid && query.material.material_slot==2u &&
        operators::legacy_plan::hashing::matches_hex(
            query.ownership.flver_sha256,
            "3a82e4f6e45f13a05ac40418ba6ad12ff9acae9cf01e19937bd5c5f45ff2636c") &&
        operators::legacy_plan::hashing::matches_hex(
            query.material.raw_mtd_sha256,
            "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b");
    const bool rc_helm = (query.material.route_index==229u ||
        query.material.route_index==5u) &&
        query.material.material_slot_valid &&
        query.material.material_slot==0u;
    const bool rc_v13 = armor_v13::pair_ok(h1,h0);
    if(rc_source1||rc_helm||rc_v13) {
        static std::atomic<std::uint32_t> src_count{0u},helm_count{0u},v13_count{0u};
        auto &counter=rc_source1?src_count:rc_helm?helm_count:v13_count;
        const auto n=counter.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n<=20u) {
            char b[390]{};
            std::snprintf(b,sizeof(b),
                "[DSRRL RESOURCE CUT V2] kind=%s n=%u rx=%u route=%u slot=%u stockmask=%u hashmask=%u sidemask=%u h0=%016llx h1=%016llx h2=%016llx",
                rc_source1?"source1":rc_helm?"helm":"v13",
                n,receiver_id,query.material.route_index,query.material.material_slot,
                (views[0]?1u:0u)|(views[1]?2u:0u)|(views[2]?4u:0u),
                (h0?1u:0u)|(h1?2u:0u)|(h2?4u:0u),
                (companions[0]?1u:0u)|(companions[1]?2u:0u)|(companions[2]?4u:0u),
                static_cast<unsigned long long>(h0),
                static_cast<unsigned long long>(h1),
                static_cast<unsigned long long>(h2));
            reshade::log::message(reshade::log::level::warning,b);
        }
    }
#endif

#if defined(DSRRL_V230_DUAL_FRONTIER)''')

# Record which route 345 RX34 SRV read path is taken. The shadow snapshot
# is a thread-local descriptor observer; a successful snapshot may still
# contain three null views and must never be treated as a positive sidecar.
p="pmetal_envspec_draw_runtime.cpp"
patch(p,'''    if (shadow_env_ready &&
        shadow_material_ready)
        telemetry::hot_count(''',
'''#if defined(DSRRL_V230_RESOURCE_CUT_V2)
    if (decision.receiver_id==34u &&
        material.route_index==345u &&
        material.material_slot_valid && material.material_slot==2u &&
        hashing::matches_hex(material.flver_sha256,
            "3a82e4f6e45f13a05ac40418ba6ad12ff9acae9cf01e19937bd5c5f45ff2636c")) {
        static std::atomic<std::uint32_t> src_shadow_count{0u};
        const auto n=src_shadow_count.fetch_add(1u,std::memory_order_relaxed)+1u;
        if(n<=20u) {
            char b[240]{};
            std::snprintf(b,sizeof(b),
                "[DSRRL RX34 SRV CUT] n=%u shadow_env=%u shadow_material=%u shadow_mask=%u fallback_context=%u",
                n,shadow_env_ready?1u:0u,shadow_material_ready?1u:0u,
                (shadow_material[0]?1u:0u)|(shadow_material[1]?2u:0u)|
                    (shadow_material[2]?4u:0u),
                shadow_material_ready?0u:1u);
            reshade::log::message(reshade::log::level::warning,b);
        }
    }
#endif
    if (shadow_env_ready &&
        shadow_material_ready)
        telemetry::hot_count(''')
print("RESOURCE_CUT_V2_SOURCE_PATCH_PASS no operator/state changes")
