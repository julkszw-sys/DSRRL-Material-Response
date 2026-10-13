#include "ptde_equipment_hemenvlerp_core.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace dsrrl::v257;
static bool eq(float a,float b){return std::fabs(a-b)<0.0001f;}
static bool eq(rgb a,rgb b){return eq(a.r,b.r)&&eq(a.g,b.g)&&eq(a.b,b.b);}
int main(){
 pair p{{{0.5f,0.4f,0.3f,0.5f},{2,1,3}},{{0.75f,0.25f,0.5f,0.25f},{1,3,2}},0.25f};
 pair_output v{};
 assert(env_blend(p,v));
 assert(eq(v.decoded_a,{2,0.8f,1.8f}));
 assert(eq(v.decoded_b,{3,3,4}));
 assert(eq(v.blended,{2.25f,1.35f,2.35f}));
 p.beta=0;assert(env_blend(p,v)&&eq(v.blended,v.decoded_a));
 p.beta=1;assert(env_blend(p,v)&&eq(v.blended,v.decoded_b));
 environment e{p,p};e.diffuse.beta=0.75f;e.specular.beta=0.25f;
 environment_output o{};assert(env_blend(e,o));
 assert(!eq(o.diffuse.blended,o.specular.blended));
 surface_input m{{1,2,3},{0.2f,0.2f,0.2f},{2,3,4},{0.5f,0.5f,0.5f},{0.6f,0.7f,0.8f},{2,2,2},{3,3,3},{0,0,0},{1,1,1}};
 rgb s{};assert(compose_local(m,s));assert(eq(s,{4.8f,8.5f,12.8f}));
 terminal_input t{{2,4,6},{10,20,30},0,1,2};
 assert(compose_terminal(t,s)&&eq(s,{1,2,3}));
 t.c110_w=1;assert(compose_terminal(t,s)&&eq(s,{5,10,15}));
 t.c110_w=0.25f;assert(compose_terminal(t,s)&&eq(s,{2,4,6}));
 t.c135_y=0;assert(!compose_terminal(t,s));
 admission g{};assert(!accepts(g));
 auto &a=g.expected,&b=g.observed;
 a.receiver=b.receiver=33;a.slot=b.slot=4;
 a.flver_fingerprint=b.flver_fingerprint=1;
 a.mtd_fingerprint=b.mtd_fingerprint=2;
 a.shader_fingerprint=b.shader_fingerprint=3;
 a.logical_tex_hash=b.logical_tex_hash=4;
 for(unsigned i=0;i<32;++i){
   a.flver[i]=b.flver[i]=static_cast<unsigned char>(i+1);
   a.mtd[i]=b.mtd[i]=static_cast<unsigned char>(i+2);
   a.shader[i]=b.shader[i]=static_cast<unsigned char>(i+3);
   a.texture[i]=b.texture[i]=static_cast<unsigned char>(i+4);
   a.sidecar[i]=b.sidecar[i]=static_cast<unsigned char>(i+5);
 }
 g.equipment_owner_verified=g.slot_verified=true;
 g.sidecar_loaded_and_hashed=g.consumer_family_proven=true;
 g.full_material_port_proven=g.exact_post_material_inputs_proven=true;
 assert(accepts(g));
 b.sidecar[17]^=1;assert(!accepts(g));b.sidecar[17]^=1;
 b.receiver=37;assert(!accepts(g));b.receiver=33;
 g.equipment_owner_verified=false;assert(!accepts(g));
 g.equipment_owner_verified=true;
 g.exact_post_material_inputs_proven=false;assert(!accepts(g));
 std::puts("V257_PTDE_EQUIPMENT_HEMENVLERP_CLEAN_KERNEL_PASS");
}
