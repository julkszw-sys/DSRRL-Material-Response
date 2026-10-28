#include "dsrrl/runtime/v413_live_owner_probe.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
int main(){
    using namespace dsrrl::runtime::v413;
    auto *owner=static_cast<unsigned char*>(
        ::VirtualAlloc(nullptr,0x2500,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    auto *wrapper=static_cast<unsigned char*>(
        ::VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    assert(owner && wrapper);
    const std::uintptr_t manager=0x12345678u;
    const std::uintptr_t vtable=0x14012e6b60ull;
    const std::uintptr_t wp=reinterpret_cast<std::uintptr_t>(wrapper);
    std::memcpy(owner,&vtable,sizeof(vtable));
    std::memcpy(owner+0x2318,&wp,sizeof(wp));
    std::memcpy(owner+0x2320,&manager,sizeof(manager));
    std::memcpy(wrapper+0x40,&manager,sizeof(manager));
    auto s=inspect_owner(reinterpret_cast<std::uintptr_t>(owner));
    assert(s.readable && s.manager_agrees);
    assert(s.wrapper==wp && s.manager==manager && s.vtable==vtable);
    const std::uintptr_t other=0x87654321u;
    std::memcpy(owner+0x2320,&other,sizeof(other));
    s=inspect_owner(reinterpret_cast<std::uintptr_t>(owner));
    assert(s.readable && !s.manager_agrees);
    const auto old=reinterpret_cast<std::uintptr_t>(owner);
    assert(::VirtualFree(owner,0,MEM_RELEASE));
    s=inspect_owner(old);
    assert(!s.readable);
    assert(::VirtualFree(wrapper,0,MEM_RELEASE));
}
