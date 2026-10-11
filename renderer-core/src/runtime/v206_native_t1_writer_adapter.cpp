// V206 focused provenance producer for the V204 renderer architecture.
// Reuses the existing retail TLS name hook and V204 ReShade callbacks.
// Never owns/binds a D3D11 resource, loads an asset, or infers names from MTD.
#include "dsrrl/runtime/texture_identity_transport.hpp"
#include "dsrrl/runtime/v206_native_t1_provenance.hpp"
#include <Windows.h>
#include <reshade.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

extern "C" {
void *g_dsrrl_v206_writer_target=nullptr;
void *g_dsrrl_v206_writer_resume=nullptr;
}

namespace {
std::atomic<std::uint64_t> g_writer_calls{0u};
std::atomic<std::uint64_t> g_witnesses{0u};

// Bounds check of the exact host's CPU pointer values; no blind cast,
// dereference of COM vtables, hook replay or on-disk EXE modification.
bool readable(const void *ptr,std::size_t size) noexcept {
    if (!ptr || !size)return false;
    auto at=reinterpret_cast<std::uintptr_t>(ptr);
    const auto end=at+size;
    if(end<at)return false;
    while(at<end){
        MEMORY_BASIC_INFORMATION info{};
        if(VirtualQuery(reinterpret_cast<const void *>(at),
                        &info,sizeof(info))!=sizeof(info) ||
           info.State!=MEM_COMMIT ||
           (info.Protect & PAGE_GUARD))
            return false;
        const auto access=info.Protect&0xffu;
        if(access!=PAGE_READONLY && access!=PAGE_READWRITE &&
           access!=PAGE_WRITECOPY && access!=PAGE_EXECUTE_READ &&
           access!=PAGE_EXECUTE_READWRITE &&
           access!=PAGE_EXECUTE_WRITECOPY)return false;
        const auto next=reinterpret_cast<std::uintptr_t>(
            info.BaseAddress)+info.RegionSize;
        if(next<=at)return false;
        at=next<end?next:end;
    }
    return true;
}
} // namespace

// The verified retail writer passes TexHdlResCap cache node (RBX),
// DLGR engine object (RDI), and entry id (ESI). At the observer instant,
// native +0x28/+0x30 fields may still be null. Fail-open on missing fields.
// Source-specific vtable RVA must match EXACT retail EXE SHA already checked
// by texture_identity_transport::install(), never a name prefix alone.
extern "C" void dsrrl_v206_writer_observer(
    const void *cache_entry,
    const void *engine_object,
    std::uint32_t source_id) noexcept {
    const auto call=g_writer_calls.fetch_add(1u,
        std::memory_order_relaxed)+1u;
    const auto flags=dsrrl::runtime::texture_identity_transport::status();
    if(!flags.provenance_ok || !flags.name_hook_armed ||
       !cache_entry || !engine_object || !source_id)return;
    const wchar_t *name=nullptr;
    std::size_t length=0u;
    if(!dsrrl::runtime::texture_identity_transport::snapshot_raw(
            name,length) || !name || length<6u || length>64u)
        return;
    const auto *slot=static_cast<const std::uint8_t *>(cache_entry)+8u;
    if(!readable(slot,sizeof(const wchar_t *)))return;
    const wchar_t *node_name=nullptr;
    std::memcpy(&node_name,slot,sizeof(node_name));
    if(!node_name || !readable(node_name,(length+1u)*sizeof(wchar_t)) ||
       node_name[length]!=L'\0' ||
       std::char_traits<wchar_t>::compare(node_name,name,length)!=0)
        return;
    const auto host_base=reinterpret_cast<std::uintptr_t>(
        GetModuleHandleW(nullptr));
    std::uintptr_t vtable=0u;
    if(!host_base || !readable(engine_object,sizeof(vtable)))return;
    std::memcpy(&vtable,engine_object,sizeof(vtable));
    // Vtable + exact EXE hash + exact cache-key identity are required;
    // this does NOT accept any object merely because its name ends "_s".
    if(vtable!=host_base+0x014AA4A8u)return;
    const auto *fields=static_cast<const std::uint8_t *>(engine_object)+0x28u;
    std::array<std::uintptr_t,2u> pair{};
    if(!readable(fields,sizeof(pair)))return;
    std::memcpy(pair.data(),fields,sizeof(pair));
    if(!pair[0] || !pair[1])return;
    std::array<char,65u> ascii{};
    for(std::size_t i=0u;i<length;++i){
        if(name[i]<0x21u || name[i]>0x7eu)return;
        ascii[i]=static_cast<char>(name[i]);
    }
    const bool accepted=dsrrl::runtime::v206_native_t1::instance().
        observe_writer(pair[1],pair[0],ascii.data(),length,source_id);
    if(!accepted)return;
    const auto n=g_witnesses.fetch_add(1u,
        std::memory_order_relaxed)+1u;
    if(n<=8u || (n<=8192u && (n&(n-1u))==0u)){
        char log[310]{};
        std::snprintf(log,sizeof(log),
            "[DSRRL V206 CPU WRITER] source=verified_DLTexture2D "
            "name=%s epoch_matched_at_writer=1 calls=%llu "
            "witnesses=%llu bridge_authority=0 pixel=OPEN",
            ascii.data(),
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(n));
        reshade::log::message(reshade::log::level::info,log);
    }
}
