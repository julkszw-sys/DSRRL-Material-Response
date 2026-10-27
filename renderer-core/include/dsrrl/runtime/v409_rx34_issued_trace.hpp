#pragma once
#include "dsrrl/runtime/v409_rx34_transition_tracker.hpp"
#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include <Windows.h>
#include <reshade.hpp>
#include <cstddef>
#include <cstdio>
#include <cstring>
namespace dsrrl::runtime::v409 {
inline void issued(
 const operators::material_response::material_identity &m,
 const operators::material_response::decision &d,
 const prepared_pmetal_envspec_draw &p) noexcept {
 if(d.receiver_id!=34u || !p.ready || !p.stable_envdiffuse_trace_valid ||
    !telemetry::effect_enabled()) return;
 static source_transition_tracker tracker{};
 identity key{};
 key.owner=m.model_owner_instance;
 key.shader=reinterpret_cast<std::uintptr_t>(p.shader);
 key.semantic=m.semantic_name_hash;
 key.route=m.route_index;key.material_slot=m.material_slot;
 key.receiver=d.receiver_id;key.flver=m.flver_sha256;key.mtd=m.raw_mtd_sha256;
 source_snapshot s{};
 s.kind=p.source_kind;s.independent=p.stable_envdiffuse_independent;
 s.bank=p.stable_envdiffuse_bank;s.row=p.stable_envdiffuse_row;
 s.probe=p.stable_envdiffuse_probe;
 s.serial=p.stable_envdiffuse_source_serial;
 s.native_sequence=p.stable_envdiffuse_native_producer_sequence;
 for(std::size_t i=0;i<3u;++i)
   std::memcpy(&s.rgb_bits[i],&p.stable_envdiffuse_c86[i],sizeof(float));
 const observation o=tracker.observe(key,s);
 const bool first=o.kind==observation_kind::first && o.sequence<=64u;
 const bool changed=o.kind==observation_kind::transition &&
   (o.sequence<=256u || o.sequence%4096u==0u);
 const bool full=o.kind==observation_kind::capacity_miss &&
   (o.sequence<=16u || o.sequence%4096u==0u);
 if(!first && !changed && !full)return;
 constexpr char hex[]="0123456789abcdef";
 char flver[65]{},mtd[65]{};
 for(std::size_t i=0;i<32u;++i) {
    flver[i*2u]=hex[key.flver[i]>>4u];
    flver[i*2u+1u]=hex[key.flver[i]&15u];
    mtd[i*2u]=hex[key.mtd[i]>>4u];
    mtd[i*2u+1u]=hex[key.mtd[i]&15u];
 }
 float prior_rgb[3]{};
 for(std::size_t i=0;i<3u;++i)
    std::memcpy(&prior_rgb[i],&o.previous.rgb_bits[i],sizeof(float));
 char msg[1350]{};
 std::snprintf(msg,sizeof(msg),
  "[DSRRL V409 RX34 GPU TRANSITION] event=%s n=%llu "
  "owner=%p rx=34 route=%u slot=%u sem=%016llx shader=%p "
  "flver_sha=%s mtd_sha=%s "
  "kind_before=%u row_before=%u bank_before=%016llx "
  "native_seq_before=%llu prior_c86=%.9g,%.9g,%.9g "
  "prior_probe=%u prior_path=%s kind=%u row=%u bank=%016llx "
  "c86=%.9g,%.9g,%.9g probe=%u path=%s "
  "serial=%llu native_producer_seq=%llu tid=%lu "
  "issued=1 draw_order=OPEN pixel=OPEN",
  changed?"changed":first?"first":"capacity_miss",
  static_cast<unsigned long long>(o.sequence),
  reinterpret_cast<void*>(key.owner),key.route,key.material_slot,
  static_cast<unsigned long long>(key.semantic),
  reinterpret_cast<void*>(key.shader),flver,mtd,
  static_cast<unsigned>(o.previous.kind),o.previous.row,
  static_cast<unsigned long long>(o.previous.bank),
  static_cast<unsigned long long>(o.previous.native_sequence),
  static_cast<double>(prior_rgb[0]),static_cast<double>(prior_rgb[1]),
  static_cast<double>(prior_rgb[2]),
  static_cast<unsigned>(o.previous.probe),
  o.previous.independent?"INDEPENDENT":"COMBINED",
  static_cast<unsigned>(s.kind),s.row,
  static_cast<unsigned long long>(s.bank),
  static_cast<double>(p.stable_envdiffuse_c86[0]),
  static_cast<double>(p.stable_envdiffuse_c86[1]),
  static_cast<double>(p.stable_envdiffuse_c86[2]),
  static_cast<unsigned>(s.probe),
  s.independent?"INDEPENDENT":"COMBINED",
  static_cast<unsigned long long>(s.serial),
  static_cast<unsigned long long>(s.native_sequence),
  static_cast<unsigned long>(GetCurrentThreadId()));
 reshade::log::message(reshade::log::level::warning,msg);
}
}
