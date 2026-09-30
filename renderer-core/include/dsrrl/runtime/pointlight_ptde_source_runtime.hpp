#pragma once
#include "dsrrl/runtime/pointlight_ptde_source.hpp"
#include "dsrrl/runtime/pmetal_selector_policy.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <algorithm>
#include <cstdint>

namespace dsrrl::runtime::pointlight_ptde_source {
inline bool readable(std::uintptr_t address,std::size_t size) noexcept {
    if(!address || address+size<address) return false;
    const auto end=address+size;
    while(address<end) {
        MEMORY_BASIC_INFORMATION m{};
        if(VirtualQuery(reinterpret_cast<void*>(address),&m,sizeof(m))!=sizeof(m)||
           m.State!=MEM_COMMIT||(m.Protect&PAGE_GUARD)) return false;
        const auto p=m.Protect&255u;
        if(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY&&
           p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY) return false;
        const auto next=reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;
        if(next<=address) return false;
        address=std::min(next,end);
    }
    return true;
}
template<class T> inline bool read(std::uintptr_t address,T &out) noexcept {
    if(!readable(address,sizeof(T))) return false;
    std::memcpy(&out,reinterpret_cast<void*>(address),sizeof(T)); return true;
}
inline bool donor(std::uintptr_t source,std::int32_t selector,signal &out) noexcept {
    if(selector<0) return false;
    std::uintptr_t param=0;
    if(!read(source+0x18u,param)||!readable(param,0x330u)) return false;
    const auto *p=reinterpret_cast<const std::uint8_t*>(param);
    std::uint16_t count=0; std::uint32_t first=0;
    std::memcpy(&count,p+0xau,2); std::memcpy(&first,p+0x34u,4);
    if(count!=64u||first<0x330u||first>0x10000u) return false;
    for(std::uint32_t i=0;i<64u;++i) {
        std::uint32_t id=0,off=0;
        std::memcpy(&id,p+0x30u+12u*i,4); std::memcpy(&off,p+0x34u+12u*i,4);
        if(id!=i||off!=first+16u*i) return false;
    }
    const auto index=static_cast<std::uint32_t>(selector)&255u;
    if(index>=64u||!readable(param+first,1024u)) return false;
    const auto *bank=identify(p+first);
    if(!bank) return false;
    out=decode(bank->ptde[index]); return true;
}
inline bool selected_source(std::uintptr_t manager,std::int16_t selector,std::uintptr_t &source) noexcept {
    const auto area=selector<0?0xffffffffu:(static_cast<unsigned>(selector)>>8u)&127u;
    auto lookup=[&](unsigned a) noexcept {
        std::uintptr_t table=0;
        return read(manager+0x20u+a*0x1b0u,table)&&table&&read(table+9u*0x10u+8u,source);
    };
    source=0;
    if(area<=11u&&!lookup(area)) return false;
    if(!source&&!lookup(11u)) return false;
    return source!=0;
}
// Caller must have attested the retail EXE. No hook, host write, retained pointer,
// source-category guess, or global latest-source publication is used here.
inline bool capture(void *node,std::uintptr_t base,std::array<float,8> &raw) noexcept {
    const auto n=reinterpret_cast<std::uintptr_t>(node);
    std::uintptr_t vt=0,fn=0,owner=0;
    if(!base||!read(n,vt)||!read(vt+0x60u,fn)||!read(n+0x50u,owner)||!owner) return false;
    signal a,b,result;
    if(fn==base+0x55bc00u) {
        std::int32_t selector=-1;
        if(!read(n+0x58u,selector)||!donor(owner,selector,a)||!mix(a,a,0,result)) return false;
    } else if(fn==base+0x55d0b0u) {
        std::int16_t sa=-1,sb=-1; float beta=0;
        if(!read(n+0x58u,sa)||!read(n+0x5au,sb)||!read(n+0x5cu,beta)) return false;
        const auto pair=pmetal_selector_policy::select(sa,sb,beta);
        if(!pair.valid) return false;
        std::uintptr_t source_a=0,source_b=0;
        if(!selected_source(owner,pair.a,source_a)||!donor(source_a,pair.a,a)) return false;
        b=a;
        if(pair.beta!=0&&(!selected_source(owner,pair.b,source_b)||!donor(source_b,pair.b,b))) return false;
        if(!mix(a,b,pair.beta,result)) return false;
    } else return false;
    // Preserve host world position. Only the operator-local payload changes.
    raw[3]=1.0f/(result.end-result.begin);
    raw[4]=result.q[0];raw[5]=result.q[1];raw[6]=result.q[2];raw[7]=result.end;
    return std::isfinite(raw[3]);
}
} // namespace dsrrl::runtime::pointlight_ptde_source
