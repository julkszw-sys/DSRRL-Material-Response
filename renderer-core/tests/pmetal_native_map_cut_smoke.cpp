#ifdef NDEBUG
#undef NDEBUG
#endif
#include "dsrrl/runtime/pmetal_native_map_cut.hpp"
#include <cassert>
#include <array>
#include <iostream>
using namespace dsrrl::runtime::pmetal_native_map_cut;
static std::uint32_t original_maps=0,original_unmaps=0;
static HRESULT STDMETHODCALLTYPE test_map(ID3D11DeviceContext *,
    ID3D11Resource *,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE *out) {
    ++original_maps;
    if(out) *out={};
    return S_OK;
}
static void STDMETHODCALLTYPE test_unmap(ID3D11DeviceContext *,
    ID3D11Resource *,UINT) {
    ++original_unmaps;
}
struct fake_ctx {void **vtbl;};
int main() {
    // Allocated writable vtable simulates D3D11's shared COM dispatch table.
    void **vtbl=reinterpret_cast<void **>(VirtualAlloc(
        nullptr,128*sizeof(void *),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    assert(vtbl);
    for(int i=0;i<128;++i) vtbl[i]=nullptr;
    vtbl[map_slot]=reinterpret_cast<void *>(&test_map);
    vtbl[unmap_slot]=reinterpret_cast<void *>(&test_unmap);
    fake_ctx ctx{vtbl};
    auto *native=reinterpret_cast<ID3D11DeviceContext *>(&ctx);
    auto *b0=reinterpret_cast<ID3D11Resource *>(std::uintptr_t(0xABCD0000));
    auto &ob=global_observer();
    assert(!ob.observe(reinterpret_cast<std::uintptr_t>(native),
                       reinterpret_cast<std::uintptr_t>(b0)).complete_map_unmap);
    assert(ob.install(native));
    assert(ob.install(native));
    assert(ob.hook_count()==1);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    auto map=reinterpret_cast<map_fn>(vtbl[map_slot]);
    auto unmap=reinterpret_cast<unmap_fn>(vtbl[unmap_slot]);
    assert(SUCCEEDED(map(native,b0,0u,D3D11_MAP_WRITE_DISCARD,0u,&mapped)));
    unmap(native,b0,0u);
    auto w=ob.observe(reinterpret_cast<std::uintptr_t>(native),
                      reinterpret_cast<std::uintptr_t>(b0));
    assert(original_maps==1 && original_unmaps==1);
    assert(w.hook_active && w.ever_watched);
    assert(w.map_count==1 && w.unmap_count==1 && w.write_unmap_count==1);
    assert(w.source_context_match && w.complete_map_unmap);
    assert(w.last_map_tid==GetCurrentThreadId());
    assert(w.last_unmap_ctx==reinterpret_cast<std::uintptr_t>(native));
    ob.forget(reinterpret_cast<std::uintptr_t>(b0));
    w=ob.observe(reinterpret_cast<std::uintptr_t>(native),
                 reinterpret_cast<std::uintptr_t>(b0));
    assert(w.map_count==0 && !w.complete_map_unmap);
    assert(ob.detach());
    assert(vtbl[map_slot]==reinterpret_cast<void *>(&test_map));
    assert(vtbl[unmap_slot]==reinterpret_cast<void *>(&test_unmap));
    VirtualFree(vtbl,0,MEM_RELEASE);
    std::cout<<"PASS: native Map/Unmap trampoline, exact buffer/context, forget, teardown\n";
}
