#!/usr/bin/env python3
"""Owner-first all-material draw witness at the actual integrated draw dispatch.

Must run after materialize_v230_cutscene_trace.py. No authorization,
shader, resource, constant buffer or replay state is changed.
"""
from pathlib import Path
p=Path("build-v230-release/generated/dsrrl/runtime/integrated_addon_physical_cut.cpp")
s=p.read_text(encoding="utf-8")
anchor="void v230_cutscene_emit("
assert s.count(anchor)==1,f"HELM_DISPATCH_FUNCTION_DRIFT {s.count(anchor)}"

observer=r"""
// The old armor DUAL tracer was behind pmetal/material-resource selection.
// This trace is deliberately attached to BOTH real draw dispatch callsites,
// before drawing inferences from any operator-local eligibility predicate.
void v230_helmet_draw_witness(
    const dsrrl::operators::material_response::material_identity &material,
    const dsrrl::operators::material_response::decision &decision) noexcept
{
    namespace mr=dsrrl::operators::material_response;
    // Capture any receiver/material tuple, including invalid ones. Never authorize.
    const bool rejected = !material.valid ||
        !material.owner_tuple_exact || !decision.active;

    // Per-identity de-duplication, not a single early session-wide burst.
    // A visible helmet encountered later in the session can still be sampled.
    std::uint64_t fingerprint=14695981039346656037ull;
    for (auto byte:material.flver_sha256)
        fingerprint=(fingerprint^byte)*1099511628211ull;
    for (auto byte:material.raw_mtd_sha256)
        fingerprint=(fingerprint^byte)*1099511628211ull;
    fingerprint=(fingerprint^material.material_slot)*1099511628211ull;
    fingerprint=(fingerprint^decision.receiver_id)*1099511628211ull;
    fingerprint=(fingerprint^material.route_index)*1099511628211ull;
    fingerprint=(fingerprint^decision.route_index)*1099511628211ull;
    fingerprint=(fingerprint^static_cast<unsigned>(decision.reason))*1099511628211ull;
    fingerprint=(fingerprint^static_cast<unsigned>(decision.active))*1099511628211ull;
    static std::array<std::atomic<std::uint64_t>,16384> last_seen{};
    static std::atomic<std::uint32_t> total{0u};
    const auto now=static_cast<std::uint64_t>(GetTickCount64());
    auto &prev=last_seen[fingerprint%last_seen.size()];
    auto old=prev.load(std::memory_order_relaxed);
    if(now>=old && now-old<12000u) return;
    if(!prev.compare_exchange_strong(old,now,std::memory_order_relaxed))return;
    const auto count=total.fetch_add(1u,std::memory_order_relaxed)+1u;
    if(count>3500u) return;

    auto hex=[](const std::array<std::uint8_t,32> &sha,char (&out)[65]) noexcept {
        constexpr char digits[]="0123456789abcdef";
        for(std::size_t i=0;i<sha.size();++i){
            out[2*i]=digits[sha[i]>>4u];
            out[2*i+1]=digits[sha[i]&15u];
        }
        out[64]='\0';
    };
    char flver[65]{},mtd[65]{};
    hex(material.flver_sha256,flver);
    hex(material.raw_mtd_sha256,mtd);
    char b[580]{};
    std::snprintf(b,sizeof(b),
       "[DSRRL EQUIPMENT ROUTE CENSUS] n=%u candidate=%s rx=%u reason=%u route=%u decision_route=%u slot=%u slotvalid=%u valid=%u owner=%u actual=%u active=%u namehash=%016llx flver=%s mtd=%s",
       count,rejected?"rejected":"eligible",decision.receiver_id,
       static_cast<unsigned>(decision.reason),material.route_index,decision.route_index,
       material.material_slot,material.material_slot_valid?1u:0u,
       material.valid?1u:0u,material.owner_tuple_exact?1u:0u,
       material.actual_material_exact?1u:0u,decision.active?1u:0u,
       static_cast<unsigned long long>(material.semantic_name_hash),flver,mtd);
    reshade::log::message(reshade::log::level::warning,b);
}

"""
# Embedded in the existing diagnostic region with ReShade imports and ABI.
s=s.replace(anchor,observer+anchor,1)
needle="""#if defined(DSRRL_V230_CUTSCENE_TRACE)
    v230_cutscene_emit("""
assert s.count(needle)==2,f"HELM_DISPATCH_CALLSITE_DRIFT {s.count(needle)}"
s=s.replace(needle,"""#if defined(DSRRL_V230_CUTSCENE_TRACE)
    v230_helmet_draw_witness(material,decision);
    v230_cutscene_emit(""")
# Catch resource/atomic-denied draw attempts before dispatch.
# This observer is placed inside the existing V230 cutscene macro block
# and does not alter the subsequent stock DSR fallback.
denied='v230_cutscene_emit(material, decision, nullptr, cmd_list,'
assert s.count(denied)==1,f"EARLY_MATERIAL_DENIAL_CUT_DRIFT {s.count(denied)}"
s=s.replace(denied,
    'v230_helmet_draw_witness(material,decision);\n            '+denied,1)
p.write_text(s,encoding="utf-8")
print("EQUIPMENT_CENSUS_PASS both draw sites plus early atomic deny; read-only, all material identities")
