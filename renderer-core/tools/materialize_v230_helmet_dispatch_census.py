#!/usr/bin/env python3
"""Owner-first Standard Helm witness at the actual integrated draw dispatch.

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
    if (!material.valid) return;
    const bool route_candidate =
        material.route_index == 229u || material.route_index == 5u ||
        decision.route_index == 229u || decision.route_index == 5u;
    const bool semantic_candidate =
        material.semantic_name_hash == mr::mtd_semantic_hash("C_Metal[DSB].mtd") ||
        material.semantic_name_hash == mr::mtd_semantic_hash("P_Metal[DSB]_Edge.mtd");
    if (!route_candidate && !semantic_candidate) return;

    // Per-identity de-duplication, not a single early session-wide burst.
    // A visible helmet encountered later in the session can still be sampled.
    std::uint64_t fingerprint=14695981039346656037ull;
    for (auto byte:material.flver_sha256)
        fingerprint=(fingerprint^byte)*1099511628211ull;
    for (auto byte:material.raw_mtd_sha256)
        fingerprint=(fingerprint^byte)*1099511628211ull;
    fingerprint=(fingerprint^material.material_slot)*1099511628211ull;
    fingerprint=(fingerprint^decision.receiver_id)*1099511628211ull;
    static std::array<std::atomic<std::uint64_t>,4096> last_seen{};
    static std::atomic<std::uint32_t> total{0u};
    const auto now=static_cast<std::uint64_t>(GetTickCount64());
    auto &prev=last_seen[fingerprint%last_seen.size()];
    auto old=prev.load(std::memory_order_relaxed);
    if(now>=old && now-old<4000u) return;
    if(!prev.compare_exchange_strong(old,now,std::memory_order_relaxed))return;
    const auto count=total.fetch_add(1u,std::memory_order_relaxed)+1u;
    if(count>600u) return;

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
    char b[510]{};
    std::snprintf(b,sizeof(b),
       "[DSRRL HELM DRAW CUT] n=%u rx=%u route=%u decision_route=%u slot=%u valid=%u owner=%u actual=%u active=%u namehash=%016llx flver=%s mtd=%s",
       count,decision.receiver_id,material.route_index,decision.route_index,
       material.material_slot,material.valid?1u:0u,
       material.owner_tuple_exact?1u:0u,material.actual_material_exact?1u:0u,
       decision.active?1u:0u,
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
p.write_text(s,encoding="utf-8")
print("HELM_DRAW_CUT_PASS dispatch-coupled male/female material census, two paths, no operator changes")
