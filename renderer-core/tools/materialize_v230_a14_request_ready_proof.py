#!/usr/bin/env python3
"""A14 exact EnvDiffuse source -> request_ready witness after all guard checks.
Read-only, bounded, never claims execution or PTDE pixel success.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/pmetal_envspec_draw_runtime.cpp"
s=p.read_text("utf8")
a="    effect_latch(effect_request_ready_);"
if s.count(a)!=1: raise RuntimeError("request_ready insertion source drift")
code=r"""
#if defined(DSRRL_V230_A14_REQUEST_READY_WITNESS)
    // Only two original V13 a14 LightBank identities; no guessed scene ID.
    // Record 2 bank x 2 endpoint x 2 family x 3 receiver = 24 bitmasks.
    // This executes ONLY after material+source+probe+CB+resource+PS validation
    // and build_island_draw_mutation() success. It does not establish that
    // the later D3D11 transaction actually committed or pixel equivalence.
    constexpr std::uint64_t m14=0xc9b879a8c5e624d0ull;
    constexpr std::uint64_t s14=0xe447e0e74af8d1d3ull;
    static std::atomic<std::uint64_t> seen[24]{};
    const unsigned family_id=static_cast<unsigned>(family);
    const unsigned rx_id=static_cast<unsigned>(decision.receiver_id);
    if(family_id<2u && rx_id>=33u && rx_id<=35u) {
        for(unsigned e=0;e<2u;++e) {
            const auto sig=e ? source.bank_signature_b : source.bank_signature_a;
            const unsigned row=e ? source.row_id_b : source.row_id_a;
            const int bank=sig==m14 ? 0 : sig==s14 ? 1 : -1;
            if(bank<0 || row>=64u)continue;
            const unsigned bucket=((static_cast<unsigned>(bank)*2u+e)*2u+family_id)*3u+(rx_id-33u);
            const std::uint64_t bit=1ull<<row;
            if(seen[bucket].fetch_or(bit,std::memory_order_relaxed)&bit)continue;
            char line[920]{};
            std::snprintf(line,sizeof(line),
              "[DSRRL A14 REQUEST READY] bank=%s endpoint=%c row=%u"
              " family=%u rx=%u route=%u sem=%016llx flver=%016llx"
              " mat_valid=%u owner_exact=%u slot=%u slot_valid=%u"
              " beta=%.9g valid_source=%u"
              " ptde_envdiffuse_a=%u ptde_envdiffuse_b=%u"
              " ptde_envspec_a=%u ptde_envspec_b=%u"
              " req_shader=%u request_ready=1"
              " downstream_draw_commit=NOT_PROVEN pixel=OPEN",
              bank==0?"m14":"s14",e?'B':'A',row,
              family_id,rx_id,static_cast<unsigned>(decision.route_index),
              static_cast<unsigned long long>(material.semantic_name_hash),
              static_cast<unsigned long long>(material.flver_identity_hash),
              material.valid?1u:0u,material.owner_tuple_exact?1u:0u,
              static_cast<unsigned>(material.material_slot),
              material.material_slot_valid?1u:0u,
              static_cast<double>(source.beta),source.envdiffuse_linear_valid?1u:0u,
              prepared.envdiffuse_resources.ptde_a!=nullptr?1u:0u,
              prepared.envdiffuse_resources.ptde_b!=nullptr?1u:0u,
              prepared.env_resources.ptde_a!=nullptr?1u:0u,
              prepared.env_resources.ptde_b!=nullptr?1u:0u,
              prepared.shader!=nullptr?1u:0u);
            reshade::log::message(reshade::log::level::warning,line);
        }
    }
#endif
"""
p.write_text(s.replace(a,a+"\n"+code),"utf8")
print("A14_REQUEST_READY_SOURCE_CONTRACT_PASS")
