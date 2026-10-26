#pragma once
// Read-only STABLE EnvDiffuse source-to-request witness.
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <reshade.hpp>
namespace dsrrl::runtime {
inline void v387_stable_cut(
 const operators::material_response::material_identity &m,
 const operators::material_response::decision &d,
 const pmetal_envspec_source &s,
 const prepared_pmetal_envspec_draw &p,
 bool independent) noexcept {
#if defined(DSRRL_V230_RUNTIME_TELEMETRY)
 struct slot {std::uint64_t key=0; ULONGLONG tick=0;};
 static thread_local std::array<slot,128> seen{};
 if(!p.ready || !p.envdiffuse_resources.ready || s.beta!=0.0f ||
    d.receiver_id<33u || d.receiver_id>35u)return;
 // V397: diagnostic-only bounded sampling. Original per-FLVER/time
 // dedup still churned 64,845 records in a two-minute V396 log.
 // Preserve first-24 source observations and a logarithmic pulse,
 // independent of material/receiver activity or rendering decisions.
 static std::atomic<std::uint64_t> seen_calls{0u};
 const auto nth=seen_calls.fetch_add(1u,std::memory_order_relaxed)+1u;
 if(nth>24u && (nth & (nth-1u))!=0u)return;
 std::uint64_t key=m.flver_identity_hash ^ m.semantic_name_hash ^
  s.bank_signature_a ^ (std::uint64_t(s.row_id_a)<<32u) ^
  (std::uint64_t(m.material_slot)<<24u) ^
  (std::uint64_t(d.receiver_id)<<16u) ^
  std::uint64_t(p.envdiffuse_resources.probe_a);
 auto &e=seen[static_cast<std::size_t>(key%seen.size())];
 const auto now=GetTickCount64();
 if(e.key==key && now>=e.tick && now-e.tick<3000ull)return;
 e={key,now};
 char flver_sha[65]{};
 constexpr char hd[]="0123456789abcdef";
 for(unsigned i=0u;i<32u;i++) {
  flver_sha[i*2u]=hd[m.flver_sha256[i]>>4u];
  flver_sha[i*2u+1u]=hd[m.flver_sha256[i]&15u];
 }
 char line[800]{};
 std::snprintf(line,sizeof(line),
 "[DSRRL V387 STABLE CUT] rx=%u route=%u slot=%u flver=%016llx "
 "sem=%016llx flver_sha=%s kind=%u bank=%016llx row=%u serial=%llu "
 "c86=%.8g,%.8g,%.8g probe=%u path=%s shader=%p "
 "b12=%p t11=%p t12=%p request=READY pixel=OPEN",
 d.receiver_id,m.route_index,m.material_slot,
 static_cast<unsigned long long>(m.flver_identity_hash),
 static_cast<unsigned long long>(m.semantic_name_hash),flver_sha,
 static_cast<unsigned>(s.consumer_source_kind),
 static_cast<unsigned long long>(s.bank_signature_a),s.row_id_a,
 static_cast<unsigned long long>(s.serial),
 static_cast<double>(s.envdiffuse_a[0]),
 static_cast<double>(s.envdiffuse_a[1]),
 static_cast<double>(s.envdiffuse_a[2]),
 p.envdiffuse_resources.probe_a,
 independent?"INDEPENDENT":"COMBINED",
 static_cast<void*>(p.shader),static_cast<void*>(p.b12),
 static_cast<void*>(p.envdiffuse_resources.ptde_a),
 static_cast<void*>(p.env_resources.ptde_a));
 reshade::log::message(reshade::log::level::warning,line);
#else
 (void)m;(void)d;(void)s;(void)p;(void)independent;
#endif
}
}
