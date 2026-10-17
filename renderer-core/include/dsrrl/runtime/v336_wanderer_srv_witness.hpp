#pragma once
// V336 read-only SRV source and PTDE PackedGI cube witness.
// Invoked only on fully prepared R8 Lerp draws. No GPU state changes.
#include "dsrrl/runtime/envspec_resource_runtime.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include <reshade.hpp>
#include <array>
#include <atomic>
#include <cstdio>

namespace dsrrl::runtime {
inline void v336_wanderer_srv_witness(
  const operators::material_response::material_identity &mat,
  const operators::material_response::decision &decision,
  envspec_resource_runtime &resources,
  const prepared_envspec_resources &env,
  bool env_shadow,bool material_shadow,
  float beta,std::uint64_t serial) noexcept
{
  namespace hashing=operators::legacy_plan::hashing;
  if(!mat.valid || !mat.owner_tuple_exact ||
     !mat.material_slot_valid ||
     !hashing::matches_hex(mat.flver_sha256,
       "1398eec9e1545fb52688c691d1ff3caa042de3750bccc6b267165a136c3ca845") ||
     (decision.receiver_id!=33u && decision.receiver_id!=34u) ||
     !(beta>0.0f && beta<1.0f) || !env.ready) return;
  const bool good=mat.material_slot==4u &&
    hashing::matches_hex(mat.raw_mtd_sha256,
     "0f2b9a1012b83c4dfc909c311d29a11147de8c15191bc62398ce0d5e1cc6757e");
  const bool bad=mat.material_slot==9u &&
    hashing::matches_hex(mat.raw_mtd_sha256,
     "7ea7d6ce9bb7148db62e8c27ee45edefb9547018d0b723dd4753d4c37e29f293");
  if(!good && !bad)return;
  static std::atomic<std::uint32_t> logged{0u};
  const auto bit=1u << ((good?0u:1u)+(decision.receiver_id==34u?2u:0u));
  if(logged.fetch_or(bit,std::memory_order_relaxed)&bit)return;
  std::array<std::uint8_t,32> a{},b{};
  const bool ok_a=resources.audit_packed_cube_sha256(env.probe_a,env.slot,a);
  const bool ok_b=resources.audit_packed_cube_sha256(env.probe_b,env.slot,b);
  char hex_a[65]{},hex_b[65]{};
  for(std::uint32_t i=0;i<32u;++i){
    std::snprintf(hex_a+i*2u,3u,"%02x",static_cast<unsigned>(a[i]));
    std::snprintf(hex_b+i*2u,3u,"%02x",static_cast<unsigned>(b[i]));
  }
  char msg[700]{};
  std::snprintf(msg,sizeof(msg),
   "[DSRRL V336 SRV WITNESS] rx=%u mesh=%u beta=%.9g serial=%llu "
   "env_source=%s material_source=%s probe_a=%u probe_b=%u "
   "env_slot=%u key_a=%u key_b=%u packed_sha_a=%s packed_sha_b=%s ready=%u,%u",
   decision.receiver_id,mat.material_slot,static_cast<double>(beta),
   static_cast<unsigned long long>(serial),
   env_shadow?"shadow":"context",material_shadow?"shadow":"context",
   env.probe_a,env.probe_b,env.slot,
   static_cast<unsigned>(env.probe_a)*4u+env.slot,
   static_cast<unsigned>(env.probe_b)*4u+env.slot,
   hex_a,hex_b,ok_a?1u:0u,ok_b?1u:0u);
  reshade::log::message(reshade::log::level::info,msg);
}
} // namespace dsrrl::runtime
