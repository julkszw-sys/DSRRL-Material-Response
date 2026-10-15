#pragma once
#include "dsrrl/runtime/v297_authored_slot0_material.hpp"
namespace dsrrl::runtime::v300 {
namespace mr=dsrrl::operators::material_response;
struct row {const char*name;const char*sha;unsigned route;};
inline constexpr row k_leather[]={
{"P_Leather[DSB].mtd","0f2b9a1012b83c4dfc909c311d29a11147de8c15191bc62398ce0d5e1cc6757e",0x52000001u},
{"P_Leather[DSB]_Edge.mtd","cf968a22722667777d10429a03a2e4a87f867412949a7fe5590cb8cefaa2bfa4",0x52000002u}
};
inline bool promote(const mr::material_identity&m,const mr::decision&host,
unsigned rx,mr::decision&out) noexcept {
out=host;
if(host.active||!m.valid||!m.actual_material_exact||!m.owner_tuple_exact||
!m.material_slot_valid||m.route_index!=0u||rx<33u||rx>35u||
m.material_family_hash!=mr::mtd_semantic_hash("DifSpcBmp")) return false;
const row*match=nullptr;
for(const auto&r:k_leather)
if(m.semantic_name_hash==mr::mtd_semantic_hash(r.name)&&
v297::raw_sha_equal(m.raw_mtd_sha256,r.sha)){if(match)return false;match=&r;}
if(!match)return false;
auto q=dsrrl::runtime::v203_equipment::make_envspec36_query(m);
if(!mr::has_exact_flver_material_ownership(q))return false;
auto e=mr::classify_mtd_envspec_semantics(q);
if(!e.exact_identity_match||e.presence!=mr::ptde_envspec_presence::present||
e.router_state!=mr::mtd_envspec_router_state::present||
!e.envspc_slot_valid||e.envspc_slot!=1u)return false;
out.active=true;out.reason=mr::decision_reason::active;
out.receiver_id=rx;out.route_index=match->route;
out.certified_operations=mr::diffuse_material_domain_linear|mr::specular_factor_c101;
out.c100={{0.5f,0.5f,0.5f}};out.c101=1.5f;out.ptde_specular_power=4.0f;
out.ptde_specular_power_verified=true;out.envspec=mr::ptde_envspec_presence::present;
return true;
}
inline bool source(const mr::material_identity&m)noexcept{
mr::decision h{},d{};return promote(m,h,33u,d);}
inline bool draw(const mr::material_identity&m,const mr::decision&d)noexcept{
if(!d.active||d.receiver_id<33u||d.receiver_id>35u)return false;
mr::decision h{},c{};if(!promote(m,h,d.receiver_id,c))return false;
if(d.route_index!=c.route_index||d.certified_operations!=c.certified_operations||
d.receiver_id!=c.receiver_id||d.envspec!=c.envspec||
!d.ptde_specular_power_verified||d.c101!=c.c101||
d.ptde_specular_power!=c.ptde_specular_power)return false;
for(unsigned i=0u;i<3u;++i)if(d.c100[i]!=c.c100[i])return false;
return true;
}
}
