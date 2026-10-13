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
 std::puts("V257_PTDE_EQUIPMENT_HEMENVLERP_CLEAN_KERNEL_PASS");
}
