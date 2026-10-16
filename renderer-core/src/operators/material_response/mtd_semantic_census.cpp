#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"
#include "dsrrl/operators/material_response/generated_envspec_router_v1.hpp"
#include "dsrrl/operators/material_response/generated_ptde_flver_texture_semantics_v1.hpp"
#include "dsrrl/operators/material_response/generated_flver_pairwise_semantics_v1.hpp"
#include "dsrrl/operators/material_response/generated_dsr_flver_owner_tuples_v1.hpp"
#include "dsrrl/operators/material_response/generated_mtd_spx_negative_v1.hpp"
#include <cstddef>
namespace dsrrl::operators::material_response { namespace {
constexpr std::uint32_t op_bit(mtd_semantic_operator op) noexcept { return 1u<<static_cast<std::uint8_t>(op); }
constexpr std::uint32_t k_full24_use_mask=op_bit(mtd_semantic_operator::material_response)|op_bit(mtd_semantic_operator::spec_rgb)|op_bit(mtd_semantic_operator::hemenv)|op_bit(mtd_semantic_operator::texture_resource_consumers);
constexpr std::uint32_t k_full24_no_use_mask=op_bit(mtd_semantic_operator::pointlight);
struct override_seed{const char*name;const char*sha256;std::uint32_t receiver0,receiver1,receiver2,use_mask,no_use_mask;};
inline constexpr override_seed k_overrides[]={{"Ps_Body[DSB].mtd","af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95",33u,34u,35u,0u,op_bit(mtd_semantic_operator::subsurface)},{"P_Metal[DSB].mtd","ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b",33u,34u,35u,op_bit(mtd_semantic_operator::env_spec),0u}};
struct exact_binding_seed{const char*name;const char*sha256;const char*material_family;std::uint32_t receiver0,receiver1,receiver2;mtd_gate_policy gate_policy;};
inline constexpr exact_binding_seed k_exact_bindings[]={{"P_Metal[DSB]_Spec.mtd","c8504bfa64c84d5035bc56ab63e43d319ef69af08c5044e0260b639ab1b78ffc","DifSpcBmp",33u,34u,35u,mtd_gate_policy::direct_exact},{"P_DullLeather[DSB]_Edge_Spec.mtd","9d24347d2bfa90df062f39a281fd0ae07f160203f1dc8c5d114ac218a6daa4c2","DifSpcBmp",33u,34u,35u,mtd_gate_policy::direct_exact},{"P_DullLeather[DSB]_Spec.mtd","4def97828f95a75b64641fa9f64ba1fc58e909e69479c0d4460aaaf9cd466ee5","DifSpcBmp",33u,34u,35u,mtd_gate_policy::direct_exact},{"P_Leather[DSB]_Alp_Spec.mtd","deeb55a8e4d50767aeaba98ba9bfa97b0fb404f901249f5ca13fa762610debda","DifSpcBmp",33u,34u,35u,mtd_gate_policy::direct_exact},{"P_Leather[DS].mtd","53819ead337c1ecd8593d8535c1fc1fdde589f00eb8ea015a8ae347da71073ea","DifSpcBmp",33u,34u,35u,mtd_gate_policy::direct_exact},{"P_Leather[DSB].mtd","0f2b9a1012b83c4dfc909c311d29a11147de8c15191bc62398ce0d5e1cc6757e","DifSpcBmp",33u,34u,35u,mtd_gate_policy::ptde_companion_required},{"C_DullLeather[DSB].mtd","613a3296e7906b2dea5c0cd48bda37c8628b850bad9988688a40595c4c49eaad","DifSpcBmp",33u,34u,35u,mtd_gate_policy::ptde_companion_required},{"C_Wet[DSB].mtd","d3885a4c00b844cdb1a09b571cb037343e5bad31249e7ec80ed5e7da195b9e6f","DifSpcBmp",33u,34u,35u,mtd_gate_policy::ptde_companion_required}};
constexpr std::uint32_t k_exact_binding_use_mask=op_bit(mtd_semantic_operator::material_response)|op_bit(mtd_semantic_operator::hemenv);constexpr std::uint32_t k_exact_binding_no_use_mask=op_bit(mtd_semantic_operator::pointlight);
int hex_value(char c) noexcept{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return 10+c-'a';if(c>='A'&&c<='F')return 10+c-'A';return -1;}bool digest_matches_hex(const core::sha256_digest&d,const char*h) noexcept{if(!h)return false;for(std::size_t i=0;i<d.size();++i){int hi=hex_value(h[i*2]),lo=hex_value(h[i*2+1]);if(hi<0||lo<0||d[i]!=static_cast<std::uint8_t>((hi<<4)|lo))return false;}return h[64]=='\0';}bool receiver_matches(std::uint32_t r,std::uint32_t a,std::uint32_t b,std::uint32_t c) noexcept{return r==a||r==b||r==c;}mtd_semantic_state state_from_masks(mtd_semantic_operator op,std::uint32_t u,std::uint32_t n) noexcept{auto b=op_bit(op);if(u&b)return mtd_semantic_state::use;if(n&b)return mtd_semantic_state::no_use;return mtd_semantic_state::unknown;}bool exact_identity(const material_identity&i,const char*n,const char*s) noexcept{return i.valid&&i.semantic_name_hash&&i.semantic_name_hash==mtd_semantic_hash(n)&&digest_matches_hex(i.raw_mtd_sha256,s);}
const generated::mtd_spx_negative_record *find_mtd_spx_negative(const material_identity&i) noexcept{
if(!i.valid||!i.semantic_name_hash)return nullptr;
for(const auto&s:generated::k_mtd_spx_negative_v1)
if(i.semantic_name_hash==s.semantic_name_hash&&digest_matches_hex(i.raw_mtd_sha256,s.raw_dsr_mtd_sha256))return &s;
return nullptr;
}
bool mtd_spx_pairwise_rejects(const material_identity&i,mtd_semantic_operator op) noexcept{
const auto*s=find_mtd_spx_negative(i);if(!s)return false;
std::uint8_t bit=0u;
if(op==mtd_semantic_operator::spec_rgb)bit=generated::mtd_spx_feature_specular;
else if(op==mtd_semantic_operator::normal_bump)bit=generated::mtd_spx_feature_bump;
else return false;
return (s->dsr_only_added_mask&bit)!=0u;
}
mtd_envspec_semantics envspec_from_router_record(const generated::envspec_router_record&s) noexcept{mtd_envspec_semantics o;o.exact_identity_match=true;o.envspc_slot_valid=s.envspc_slot<4u;o.envspc_slot=s.envspc_slot;o.suppress_dsr_only_safe=s.explicit_none_safe;switch(s.state){case generated::envspec_router_state::present:o.presence=ptde_envspec_presence::present;o.router_state=mtd_envspec_router_state::present;break;case generated::envspec_router_state::explicit_none:o.presence=ptde_envspec_presence::absent;o.router_state=mtd_envspec_router_state::explicit_none;break;case generated::envspec_router_state::nospc_host:o.presence=ptde_envspec_presence::absent;o.router_state=mtd_envspec_router_state::nospc_host;o.suppress_dsr_only_safe=false;break;default:o.presence=ptde_envspec_presence::unknown;o.router_state=mtd_envspec_router_state::unknown;o.suppress_dsr_only_safe=false;}return o;}bool envspec_router_semantics_equal(const generated::envspec_router_record&a,const generated::envspec_router_record&b) noexcept{return a.state==b.state&&a.envspc_slot==b.envspc_slot&&a.explicit_none_safe==b.explicit_none_safe;}
const generated::ptde_flver_texture_semantic_record *find_ptde_texture_by_name_hash(std::uint64_t h) noexcept{if(!h)return nullptr;for(const auto&s:generated::k_ptde_flver_texture_semantics_v1)if(s.semantic_name_hash==h)return &s;return nullptr;}
const generated::ptde_flver_texture_semantic_record *find_ptde_texture_by_legacy_hash(std::uint64_t h) noexcept{if(!h)return nullptr;for(const auto&s:generated::k_ptde_flver_texture_semantics_v1)if(s.legacy_name_hash_utf16_lower==h)return &s;return nullptr;}
bool exact_host_identity_for_texture(const material_identity&i,const generated::ptde_flver_texture_semantic_record&s) noexcept{if(!i.valid||!i.semantic_name_hash||i.semantic_name_hash!=s.semantic_name_hash)return false;for(const auto&host:generated::k_envspec_router_v1)if(host.semantic_name_hash==s.semantic_name_hash&&host.raw_mtd_sha256==i.raw_mtd_sha256)return true;return false;}
bool exact_host_identity_for_texture_legacy(std::uint64_t h,const core::sha256_digest&sha,const generated::ptde_flver_texture_semantic_record&s) noexcept{if(!h||h!=s.legacy_name_hash_utf16_lower)return false;for(const auto&host:generated::k_envspec_router_v1)if(host.legacy_name_hash_utf16_lower==h&&host.raw_mtd_sha256==sha)return true;return false;}
}
std::uint64_t mtd_semantic_hash(const char*t) noexcept{constexpr std::uint64_t o=14695981039346656037ull,p=1099511628211ull;if(!t)return 0;std::uint64_t h=o;for(;*t;++t){h^=static_cast<std::uint8_t>(*t);h*=p;}return h;}
bool has_exact_flver_material_ownership(const mtd_semantic_query&q) noexcept{
if(!q.ownership.exact||!q.ownership.material_slot_valid||!q.material.valid||!q.material.semantic_name_hash)return false;
return generated::dsr_flver_owner_tuple_authenticated(q.ownership.flver_sha256,q.ownership.material_slot,q.material.semantic_name_hash);
}
// Only semantics whose newly recovered evidence is explicitly resource/slot scoped
// are gated here. Existing independently certified exact-MTD routes (EnvSpec,
// Subsurface, SpecRGB) retain their current gates until their FLVER row payload is
// reproducibly imported; this avoids turning a construction discovery into a
// runtime regression.
bool operator_requires_flver_material_ownership(mtd_semantic_operator op) noexcept{return op==mtd_semantic_operator::diffuse||op==mtd_semantic_operator::normal_bump;}

#if defined(DSRRL_V320_PTDE_ALL_EXACT_ENVSPEC_SLOTS)
// Original PTDE MTD BND3 g_EnvSpcSlotNo, compared against vanilla DSR
// raw-MTD SHA256. 21 verified active/present mismatches, no wildcard.
// This function runs ONLY after the original exact MTD hash+SHA router hit.
// Preserve explicit_none, nospc_host, unknown and all nonmatching materials.
static mtd_envspec_semantics ptde_exact_envspec_slot_v320(
    const mtd_semantic_query &q,
    const generated::envspec_router_record &s) noexcept {
    auto out = envspec_from_router_record(s);
    if (out.presence != ptde_envspec_presence::present ||
        !out.exact_identity_match || !out.envspc_slot_valid)
        return out;
    // Hash is the SHA-attested exact MTD semantic name, not a material family.
    switch (q.material.semantic_name_hash) {
        case 0x11e72450c49be97aull: // A10_Wet[DSB]_Alp.mtd | DSR 3 -> PTDE 0
            if (s.envspc_slot == 3u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "4c311ce6f9eacc52368d638218d548cc1c308187176577de32a49c5e287663c1")) out.envspc_slot=0u;
            break;
        case 0xa029373f0e23bc7bull: // A15_Chain_tex_scroll.mtd | DSR 1 -> PTDE 3
            if (s.envspc_slot == 1u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "fa2877419a2340209fb8ff9b18f3ad9aed10a6168bc9cc86c0a177d53fc5891d")) out.envspc_slot=3u;
            break;
        case 0x6f69b9f8fa1da974ull: // A17_Alarm[DSB]_DetB.mtd | DSR 2 -> PTDE 3
            if (s.envspc_slot == 2u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "ed021b35829728e012f39501a24ce7eb65c08cea763b69eac10920ed5b9fac3a")) out.envspc_slot=3u;
            break;
        case 0xd15fc87c3118fbc0ull: // C_2231_Body[DSB].mtd | DSR 0 -> PTDE 2
            if (s.envspc_slot == 0u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "f0a73db801b91298531f7ebfcfc30b6d738af3d9d3337e13ea26151b27f4397b")) out.envspc_slot=2u;
            break;
        case 0xd9dd372226a5f07eull: // C_2320_Metal[DSB].mtd | DSR 2 -> PTDE 0
            if (s.envspc_slot == 2u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "03d46faee57fd70c3af82939802bb1c2115aa62f80003346512276d1b9bd5e0a")) out.envspc_slot=0u;
            break;
        case 0xbf35b49c796975a8ull: // C_3500_Slime[DSB].mtd | DSR 2 -> PTDE 3
            if (s.envspc_slot == 2u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "3a9d511ca4c1452d617b055a55dfe4b8f76761df4529e8db2b35f27f7a31a095")) out.envspc_slot=3u;
            break;
        case 0x321c23f48240775cull: // C_5260_Leather[DSB].mtd | DSR 0 -> PTDE 3
            if (s.envspc_slot == 0u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "0c27e030221fad13efe963cd162ec40f48f2ce7952fcb150a131cca22dc483f8")) out.envspc_slot=3u;
            break;
        case 0xe792ac9ec044628bull: // C_5290_Body[DSB][M].mtd | DSR 3 -> PTDE 1
            if (s.envspc_slot == 3u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "791145a213c2791140934d2c78bd2d6582af0458c2e8fc7c4165dde11cab0e84")) out.envspc_slot=1u;
            break;
        case 0xba68ee604cd2ce87ull: // C_5290_Wing[DSB]_Alp.mtd | DSR 1 -> PTDE 0
            if (s.envspc_slot == 1u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "b3553ca598b25799aa132fa369956dd70bd38bb65681dd68b36fc6421ce92618")) out.envspc_slot=0u;
            break;
        case 0x2626b379ca38d7bfull: // C_DullLeather[DSB]_Edge.mtd | DSR 0 -> PTDE 1
            if (s.envspc_slot == 0u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "e2d819b90e8201a93e67e84e1179e9c3bf3f57988d5f86b44c5e024d05dee1a1")) out.envspc_slot=1u;
            break;
        case 0x0d31f8d7d59d3d9aull: // C_Leather[DSB]_Edge.mtd | DSR 1 -> PTDE 2
            if (s.envspc_slot == 1u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "25ffd8635203147f291c8f8af945dbad1898a1b5dfdcf31bb4136083fbda4d39")) out.envspc_slot=2u;
            break;
        case 0xbf9481216b3dac0cull: // C_Metal[DSB]_Edge.mtd | DSR 2 -> PTDE 0
            if (s.envspc_slot == 2u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "f797c63ac4f476ab7ebb7bc150ecc2a9c61851999f5051ceb864b62c95d924d9")) out.envspc_slot=0u;
            break;
        case 0xc51d6e900d2da68eull: // C_RoughCloth[DSB]_Edge.mtd | DSR 0 -> PTDE 3
            if (s.envspc_slot == 0u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "0a29732f36dc7ba939bd0114c02af09e5b72a11745c76f90e9ed4991752994bd")) out.envspc_slot=3u;
            break;
        case 0xc97ad01bcb92ba5full: // C_Wet[DSB]_Edge.mtd | DSR 3 -> PTDE 0
            if (s.envspc_slot == 3u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "94fb44042dee45d9b022b51ee5a030382ad7d4b7b10d7ec44e213f06fa1f377a")) out.envspc_slot=0u;
            break;
        case 0x8662271c7340fcb8ull: // M_1Wood[DS][M].mtd | DSR 0 -> PTDE 1
            if (s.envspc_slot == 0u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "b83732268f7332a3888b20e8eb16a2fb03bd4fb46bab25365254ff778228bdf7")) out.envspc_slot=1u;
            break;
        case 0x1fbe36236f5451aeull: // P_Wet[DSB]_Edge.mtd | DSR 3 -> PTDE 0
            if (s.envspc_slot == 3u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "a7f73fd656797fea6eeb59131a5bd5c25198ad5c654524dadf76798c57aebb86")) out.envspc_slot=0u;
            break;
        case 0xf20160463b8cbc9bull: // Ps_Body[DSB]_Alp.mtd | DSR 0 -> PTDE 2
            if (s.envspc_slot == 0u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "024f15bad3c22cd7476c82229b429eb81c3ea01626c18f8887e98f802f390aab")) out.envspc_slot=2u;
            break;
        case 0x46c08fa8a2090615ull: // S_A17_Crystal[DSB].mtd | DSR 0 -> PTDE 2
            if (s.envspc_slot == 0u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "88c09eaa91590bd1c46ba2efc86edbf8fbac2ffb1f62371f0481f63178b3d31f")) out.envspc_slot=2u;
            break;
        case 0x0a1c9f00f399ee37ull: // S_Wet[DS].mtd | DSR 2 -> PTDE 0
            if (s.envspc_slot == 2u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "c0de14b722e6fb40b601f18a9268d7176d34aebd4754cc29305b14cf2e0a357a")) out.envspc_slot=0u;
            break;
        case 0xe3d219814a0bd57eull: // P_DullLeather[DSB]_Edge.mtd | DSR 0 -> PTDE 1
            if (s.envspc_slot == 0u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "44e1188bafa99d49349883dcc2052c13365f196e341ca5e7c788cbad0a4e058d")) out.envspc_slot=1u;
            break;
        case 0xa854f3b1d7200e39ull: // P_Metal[DSB]_Edge.mtd | DSR 2 -> PTDE 0
            if (s.envspc_slot == 2u && digest_matches_hex(q.material.raw_mtd_sha256,
                    "8b730ce8655401c5cae694ba5535efb088d9d67db2a157d8f142cb5629293485")) out.envspc_slot=0u;
            break;
        default: break;
    }
    return out;
}
#endif
mtd_envspec_semantics classify_mtd_envspec_semantics(const mtd_semantic_query&q) noexcept{mtd_envspec_semantics o;if(!q.material.valid||!q.material.semantic_name_hash)return o;for(const auto&s:generated::k_envspec_router_v1)if(q.material.semantic_name_hash==s.semantic_name_hash&&q.material.raw_mtd_sha256==s.raw_mtd_sha256) {
#if defined(DSRRL_V320_PTDE_ALL_EXACT_ENVSPEC_SLOTS)
return ptde_exact_envspec_slot_v320(q,s);
#else
return envspec_from_router_record(s);
#endif
}return o;}
mtd_envspec_semantics classify_mtd_envspec_semantics_legacy(std::uint64_t h,const core::sha256_digest&sha) noexcept{mtd_envspec_semantics o;const generated::envspec_router_record*c=nullptr;for(const auto&s:generated::k_envspec_router_v1){if(s.raw_mtd_sha256!=sha)continue;if(s.legacy_name_hash_utf16_lower==h)return envspec_from_router_record(s);if(c==nullptr)c=&s;else if(!envspec_router_semantics_equal(*c,s))return o;}return c?envspec_from_router_record(*c):o;}
ptde_flver_texture_semantics classify_ptde_flver_texture_semantics(const material_identity&m) noexcept{ptde_flver_texture_semantics o;const auto*s=find_ptde_texture_by_name_hash(m.semantic_name_hash);if(!s||!exact_host_identity_for_texture(m,*s))return o;o.positive_mask=s->positive_mask;o.exact_host_identity_match=true;o.source_complete=generated::k_ptde_flver_texture_semantics_source_complete;return o;}
ptde_flver_texture_semantics classify_ptde_flver_texture_semantics_legacy(std::uint64_t h,const core::sha256_digest&sha) noexcept{ptde_flver_texture_semantics o;if(const auto*s=find_ptde_texture_by_legacy_hash(h);s&&exact_host_identity_for_texture_legacy(h,sha,*s)){o.positive_mask=s->positive_mask;o.exact_host_identity_match=true;o.source_complete=generated::k_ptde_flver_texture_semantics_source_complete;return o;}const generated::ptde_flver_texture_semantic_record*c=nullptr;for(const auto&host:generated::k_envspec_router_v1){if(host.raw_mtd_sha256!=sha)continue;const auto*cap=find_ptde_texture_by_legacy_hash(host.legacy_name_hash_utf16_lower);if(!cap)return {};if(c==nullptr)c=cap;else if(c->positive_mask!=cap->positive_mask)return {};}if(c){o.positive_mask=c->positive_mask;o.exact_host_identity_match=true;o.source_complete=generated::k_ptde_flver_texture_semantics_source_complete;}return o;}
bool ptde_flver_texture_semantic_present(const ptde_flver_texture_semantics&s,ptde_texture_semantic semantic) noexcept{const auto bit=static_cast<std::uint8_t>(1u<<static_cast<std::uint8_t>(semantic));return s.exact_host_identity_match&&(s.positive_mask&bit)!=0u;}
mtd_semantic_decision classify_mtd_semantic(const mtd_semantic_query&q,mtd_semantic_operator op) noexcept{if(!q.material.valid||!q.receiver_id)return {};if(mtd_spx_pairwise_rejects(q.material,op))return{mtd_semantic_state::no_use,mtd_semantic_source::mtd_spx_pairwise_negative_exact,mtd_gate_policy::exact_material,true};if(operator_requires_flver_material_ownership(op)&&!has_exact_flver_material_ownership(q))return {};if(op==mtd_semantic_operator::diffuse||op==mtd_semantic_operator::normal_bump){const auto tex=classify_ptde_flver_texture_semantics(q.material);const auto sem=op==mtd_semantic_operator::diffuse?ptde_texture_semantic::diffuse:ptde_texture_semantic::bump;if(ptde_flver_texture_semantic_present(tex,sem)){const bool pairwise_stable=op==mtd_semantic_operator::diffuse?generated::flver_pairwise_diffuse_stable_after_ptde_positive(q.material.semantic_name_hash):generated::flver_pairwise_bump_stable_after_ptde_positive(q.material.semantic_name_hash);if(pairwise_stable)return{mtd_semantic_state::use,mtd_semantic_source::flver_pairwise_stable_exact,mtd_gate_policy::exact_material,true};}}if(op==mtd_semantic_operator::env_spec){auto e=classify_mtd_envspec_semantics(q);if(e.exact_identity_match){auto st=e.presence==ptde_envspec_presence::present?mtd_semantic_state::use:e.presence==ptde_envspec_presence::absent?mtd_semantic_state::no_use:mtd_semantic_state::unknown;return{st,mtd_semantic_source::envspec_router_exact,mtd_gate_policy::exact_material,true};}}for(const auto&s:k_overrides){if(!receiver_matches(q.receiver_id,s.receiver0,s.receiver1,s.receiver2)||!exact_identity(q.material,s.name,s.sha256))continue;auto st=state_from_masks(op,s.use_mask,s.no_use_mask);if(st!=mtd_semantic_state::unknown)return{st,mtd_semantic_source::exact_override,mtd_gate_policy::exact_material,true};break;}for(const auto&s:k_exact_bindings){if(!receiver_matches(q.receiver_id,s.receiver0,s.receiver1,s.receiver2)||!exact_identity(q.material,s.name,s.sha256)||!q.material.material_family_hash||q.material.material_family_hash!=mtd_semantic_hash(s.material_family))continue;return{state_from_masks(op,k_exact_binding_use_mask,k_exact_binding_no_use_mask),mtd_semantic_source::exact_binding_extension,s.gate_policy,true};}for(const auto&s:generated::k_material_routes_v1){if(q.material.route_index!=s.route_index||!q.material.semantic_name_hash||q.material.semantic_name_hash!=mtd_semantic_hash(s.mtd_name)||!digest_matches_hex(q.material.raw_mtd_sha256,s.sha256)||!q.material.material_family_hash||q.material.material_family_hash!=mtd_semantic_hash(s.material_family)||!receiver_matches(q.receiver_id,s.receiver0,s.receiver1,s.receiver2))continue;return{state_from_masks(op,k_full24_use_mask,k_full24_no_use_mask),mtd_semantic_source::full24_exact_cohort,mtd_gate_policy::exact_material,true};}return {};}
ptde_envspec_presence mtd_envspec_presence(const mtd_semantic_query&q) noexcept{auto e=classify_mtd_envspec_semantics(q);if(e.exact_identity_match)return e.presence;auto r=classify_mtd_semantic(q,mtd_semantic_operator::env_spec);if(r.state==mtd_semantic_state::use)return ptde_envspec_presence::present;if(r.state==mtd_semantic_state::no_use)return ptde_envspec_presence::absent;return ptde_envspec_presence::unknown;}
} // namespace dsrrl::operators::material_response