#pragma once
// V268: independent PTDE c84/c85 pre-Fog GPU source transport.
// Builds exactly the 8*float4 b12 fixture ABI in
// v268_native_ptde_rx_environment_fixture.hlsl.
//
// NO derived/shared DSR beta and NO guessed PTDE constants permitted.
// A failed owner/source check MUST NOT partially overwrite the buffer.
// This is opt-in data-plane construction: no draw binding or shader replacement.
#include "../v257/ptde_equipment_hemenvlerp_core.hpp"
#include <array>
#include <cmath>
#include <cstddef>

namespace dsrrl::v268 {
using float4 = std::array<float,4>;
struct source {
  // The V257 gate requires exact observed-vs-expected physical full-SHA
  // FLVER/MTD/shader/texture/sidecar ownership, receiver and material slot.
  dsrrl::v257::admission exact_owner{};

  // Distinct PTDE semantic producers. A common DSR source.beta is NOT proof.
  bool ptde_c84_producer_verified=false;
  bool ptde_c85_producer_verified=false;
  bool ptde_upper_lower_producer_verified=false;
  bool ptde_material_producer_verified=false;
  bool native_rx_signature_verified=false;
  bool ptde_env_probe_a_b_verified=false;
  // V269: DSR local normal map may be used only when the actual receiver
  // consumes a SINGLE native t2 normal map with s2 and normal-scale source.
  // Multiple/blended normals and PTDE-rebaked assets remain fail-open.
  bool dsr_t2_normal_resource_verified=false;
  bool dsr_s2_sampler_verified=false;
  bool single_normal_material_operator_verified=false;
  bool dsr_normal_scale_producer_verified=false;

  // float4 values from actual producer, in PTDE representation/domain.
  float4 c84{},c85{},c86{},c87{},c100{},c101{},c156{},upper_lower{};
};
struct gpu_cb12 {
  // Exact V268 register order, eight 16-byte aligned float4 constants.
  // b12[0] c84, [1] c85, [2] c86, [3] c87,
  // [4] c100, [5] c101, [6] c156, [7] UpperLower
  std::array<float4,8> reg{};
};
inline bool finite(const float4 &v) noexcept {
  for(const float x:v) if(!std::isfinite(x)) return false;
  return true;
}
inline bool build_cb12(const source &s,gpu_cb12 &dst) noexcept {
  if (!dsrrl::v257::accepts(s.exact_owner) ||
      !s.ptde_c84_producer_verified ||
      !s.ptde_c85_producer_verified ||
      !s.ptde_upper_lower_producer_verified ||
      !s.ptde_material_producer_verified ||
      !s.native_rx_signature_verified ||
      !s.ptde_env_probe_a_b_verified ||
      !s.dsr_t2_normal_resource_verified ||
      !s.dsr_s2_sampler_verified ||
      !s.single_normal_material_operator_verified ||
      !s.dsr_normal_scale_producer_verified)
    return false;
  const gpu_cb12 candidate{{{
      s.c84,s.c85,s.c86,s.c87,
      s.c100,s.c101,s.c156,s.upper_lower
  }}};
  for (const auto &v:candidate.reg) if(!finite(v)) return false;
  // No coercion c85.w=c84.w: PTDE uses two independent controls.
  dst=candidate;
  return true;
}
static_assert(sizeof(gpu_cb12)==8u*16u,
              "V268 b12 hardware constant payload must be exactly 128 bytes");
} // namespace dsrrl::v268
