#pragma once
#include "dsrrl/runtime/generated_pointlight_ptde_donors.hpp"
#include <array>
#include <cmath>
#include <cstring>

namespace dsrrl::runtime::pointlight_ptde_source {
struct signal { float begin=0, end=0; std::array<float,3> q{}; };
inline signal decode(const pointlight_donors::row &r) noexcept {
    signal s;
    std::memcpy(&s.begin,&r.begin_bits,4); std::memcpy(&s.end,&r.end_bits,4);
    const float intensity=static_cast<float>(r.intensity)*0.01f;
    s.q={r.r*intensity/255.0f,r.g*intensity/255.0f,r.b*intensity/255.0f};
    return s;
}
inline bool mix(const signal &a,const signal &b,float t,signal &out) noexcept {
    if(!std::isfinite(t)) return false;
    t=t<0?0:t>1?1:t;
    out.begin=a.begin+(b.begin-a.begin)*t; out.end=a.end+(b.end-a.end)*t;
    for(unsigned i=0;i<3;++i) out.q[i]=a.q[i]+(b.q[i]-a.q[i])*t;
    return std::isfinite(out.begin)&&std::isfinite(out.end)&&out.end>out.begin&&out.end>0;
}
// Whole original bank identity, not source category, approximate color or row alone.
inline const pointlight_donors::bank *identify(const void *rows) noexcept {
    for(const auto &bank:pointlight_donors::banks)
        if(std::memcmp(rows,bank.dsr.data(),sizeof(bank.dsr))==0) return &bank;
    return nullptr;
}
} // namespace dsrrl::runtime::pointlight_ptde_source
