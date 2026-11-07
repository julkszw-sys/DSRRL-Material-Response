#include "dsrrl/runtime/v450_native_join.hpp"
#include <cassert>
#include <iostream>
using namespace dsrrl::runtime;
int main(){
 v447::source_ticket p{};p.sequence=905;p.entity=0x2189d9f0;p.drawparam=p.entity+0x108;
 p.packet=0x7fb1fb30;p.context=0x21919d80;p.model=0x0e897fc0;
 p.native_a=p.native_b=1310;p.row_a=p.row_b=30;
 v448::builder_witness w{};w.entity=p.entity;w.dp=p.drawparam;w.packet=p.packet;
 w.context=p.context;w.engine_model=p.model;w.a=w.b=1310;w.frame_rbp=0x7fb1fb60;
 w.builder_family=1;w.valid=true;
 using V=v450::verdict;
 int count=0;
 auto check=[&](V v){assert(v450::evaluate(p,w,905,0x21919d80,30).state==v);++count;};
 check(V::exact_native_packet_generation_open);
 {auto q=p;p.sequence=0;check(V::no_source);p=q;}
 {auto q=w;w.valid=false;check(V::no_issued_builder);w=q;}
 {auto q=w;w.builder_family=2;check(V::no_issued_builder);w=q;}
 {auto q=p;p.sequence=906;check(V::source_sequence_mismatch);p=q;}
 {auto q=w;w.context++;check(V::context_mismatch);w=q;}
 {auto q=w;w.entity++;check(V::entity_mismatch);w=q;}
 {auto q=w;w.dp++;check(V::drawparam_mismatch);w=q;}
 {auto q=w;w.packet++;check(V::packet_mismatch);w=q;}
 {auto q=w;w.engine_model++;check(V::model_mismatch);w=q;}
 {auto q=w;w.a=1311;check(V::native_ab_mismatch);w=q;}
 {auto q=p;p.beta_bits=1;check(V::native_ab_mismatch);p=q;}
 {auto q=p;p.row_a=31;check(V::no_source);p=q;}
 {auto q=p;p.row_a=p.row_b=31;check(V::no_source);p=q;}
 {auto q=p;p.entity=0x218d25d0;p.drawparam=p.entity+0x108;p.native_a=p.native_b=1311;p.row_a=p.row_b=31;auto b=w;w.entity=p.entity;w.dp=p.drawparam;w.a=w.b=1311;check(V::exact_native_packet_generation_open);p=q;w=b;}
 {auto q=p;p.entity=0x218d25d0;p.drawparam=p.entity+0x108;check(V::entity_mismatch);p=q;}
 assert(v450::evaluate(p,w,905,p.context,31).state==V::row_mismatch);++count;
 p.actor_generation=0;check(V::exact_native_packet_generation_open);
 std::cout<<"V450 actor packet join PASS "<<count<<" tests\n";
}
