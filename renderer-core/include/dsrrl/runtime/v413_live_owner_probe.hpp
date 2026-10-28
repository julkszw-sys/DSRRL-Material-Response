#pragma once
// V413: read-only native owner tuple inspection, diagnostic use only.
// ReadProcessMemory can fail on freed/unmapped memory without dereference.
// This is NOT a constructor/destructor generation or a CPU/GPU lifetime proof.
#include <Windows.h>
#include <cstdint>
namespace dsrrl::runtime::v413 {
struct native_context_tuple {
    std::uintptr_t vtable=0,wrapper=0,manager=0,owner_manager=0;
    bool readable=false,manager_agrees=false;
};
inline bool guarded_ptr(std::uintptr_t address,std::uintptr_t &out) noexcept {
    out=0;
    if(!address)return false;
    SIZE_T count=0;
    return ::ReadProcessMemory(::GetCurrentProcess(),
        reinterpret_cast<LPCVOID>(address),&out,sizeof(out),&count)!=0 &&
        count==sizeof(out);
}
inline native_context_tuple inspect_owner(std::uintptr_t owner) noexcept {
    native_context_tuple r{};
    if(!owner || owner>UINTPTR_MAX-0x2320u)return r;
    if(!guarded_ptr(owner,r.vtable) ||
       !guarded_ptr(owner+0x2318u,r.wrapper) ||
       !guarded_ptr(owner+0x2320u,r.owner_manager) ||
       !r.wrapper || r.wrapper>UINTPTR_MAX-0x40u ||
       !guarded_ptr(r.wrapper+0x40u,r.manager))
        return r;
    r.readable=true;
    r.manager_agrees=r.manager==r.owner_manager && r.manager!=0;
    return r;
}
}
