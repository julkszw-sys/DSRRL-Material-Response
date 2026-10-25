#pragma once
// V386: same-thread material selector / R43 Source2 comparison.
// Never rewrites the protected R43 producer or any engine selector.
#include <Windows.h>
#include <cstdint>
#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"
namespace dsrrl::runtime {
struct v386_cut {
  operators::material_response::material_identity material{};
  std::uint16_t selector=0;
  DWORD thread=0;
  ULONGLONG tick=0;
  bool valid=false;
};
inline v386_cut &v386_current() noexcept {
  static thread_local v386_cut cut{};
  return cut;
}
inline void v386_publish(
    const operators::material_response::material_identity &m,
    std::uint16_t a,std::uint16_t b,float beta) noexcept {
  auto &c=v386_current();
  c.valid=false;
  if(!m.valid||!m.owner_tuple_exact||!m.material_slot_valid||
     !m.actual_material_exact||a!=b||beta!=0.0f ||
     (a!=0x0400u&&a!=0x0405u))return;
  c.material=m;c.selector=a;c.thread=GetCurrentThreadId();
  c.tick=GetTickCount64();c.valid=true;
}
inline bool v386_identical(
   const operators::material_response::material_identity &a,
   const operators::material_response::material_identity &b) noexcept {
  return a.valid&&b.valid&&a.owner_tuple_exact&&b.owner_tuple_exact&&
    a.material_slot_valid&&b.material_slot_valid&&
    a.actual_material_exact&&b.actual_material_exact&&
    a.flver_sha256==b.flver_sha256&&a.raw_mtd_sha256==b.raw_mtd_sha256&&
    a.route_index==b.route_index&&a.material_slot==b.material_slot&&
    a.semantic_name_hash==b.semantic_name_hash&&
    a.material_family_hash==b.material_family_hash;
}
inline bool v386_mismatch(
    const operators::material_response::material_identity &material,
    const pmetal_envspec_source &src,
    unsigned &expected,unsigned &age) noexcept {
  const auto &c=v386_current();
  if(!c.valid||c.thread!=GetCurrentThreadId()||
     !v386_identical(c.material,material))return false;
  const auto now=GetTickCount64();
  if(now<c.tick||now-c.tick>32u)return false;
  if(src.beta!=0.0f||src.row_id_a!=src.row_id_b)return false;
  expected=c.selector==0x0405u?5u:0u;
  age=static_cast<unsigned>(now-c.tick);
  return src.row_id_a!=expected;
}
}
