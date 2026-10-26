#pragma once
// Read-only STABLE EnvDiffuse source-to-request witness.
#include <Windows.h>
#include <array>
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
 std::uint64_t key=m.flver_identity_hash ^ m.semantic_name_hash ^
  s.bank_signature_a ^ (std::uint64_t(s.row_id_a)<<32u) ^
  (std::uint64_t(m.material_slot)<<24u) ^
  (std::uint64_t(d.receiver_id)<<16u) ^
  std::uint64_t(p.envdiffuse_resources.probe_a);
 auto &e=seen[static_cast<std::size_t>(key%seen.size())];
 const auto now=GetTickCount64();
 if(e.key==key && now>=e.tick && now-e.tick<3000ull)return;
 e={key,now};
 char line[620]{};
 std::snprintf(line,sizeof(line),
 "[DSRRL V387 STABLE CUT] rx=%u route=%u slot=%u flver=%016llx "
 "sem=%016llx kind=%u bank=%016llx row=%u serial=%llu "
 "c86=%.8g,%.8g,%.8g probe=%u path=%s shader=%p "
 "b12=%p t11=%p t12=%p request=READY pixel=OPEN",
 d.receiver_id,m.route_index,m.material_slot,
 static_cast<unsigned long long>(m.flver_identity_hash),
 static_cast<unsigned long long>(m.semantic_name_hash),
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
