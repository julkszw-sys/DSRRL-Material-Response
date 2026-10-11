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
#include <cstddef>
#include <string>
#include <cstring>
#include <cstdio>

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
enum class writer_stage : std::size_t {
    input, tls, cache_pointer, cache_name, engine_read, vtable,
    native_fields, native_pair, non_ascii, non_equipment,
    no_view, wrong_resource, ambiguous_view, no_writer_or_capacity,
    writer_conflict, published, count
};
constexpr std::array<const char *,static_cast<std::size_t>(writer_stage::count)>
    stage_labels{{
        "INPUT", "TLS_EMPTY", "CACHE_PTR_UNREADABLE", "CACHE_NAME_MISMATCH",
        "ENGINE_UNREADABLE", "VTABLE_NOT_DLTexture2D", "NATIVE_FIELDS_UNREADABLE",
        "NATIVE_PAIR_EMPTY", "NON_ASCII", "NOT_EQUIPMENT_SPEC",
        "VIEW_NOT_REGISTERED", "RESOURCE_MISMATCH", "VIEW_AMBIGUOUS",
        "WRITER_SNAPSHOT_CAPACITY", "WRITER_CONFLICT", "PUBLISHED"
    }};
std::array<std::atomic<std::uint64_t>,
           static_cast<std::size_t>(writer_stage::count)> g_stage_hits{};
void stage_log(writer_stage stage,std::uint64_t call,
               dsrrl::runtime::v206_native_t1::probe_status probe) noexcept {
    const auto index=static_cast<std::size_t>(stage);
    const auto n=g_stage_hits[index].fetch_add(
        1u,std::memory_order_relaxed)+1u;
    if(!(n<=8u || (n<=65536u && (n&(n-1u))==0u)))return;
    const auto stats=dsrrl::runtime::v206_native_t1::instance().stats();
    char line[360]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V207 WRITER STAGE] stage=%s stage_hits=%llu "
        "writer_calls=%llu registry_init=%llu/%llu "
        "registry_writers=%llu probe_code=%u "
        "equipment_authority=0 swap=0 pixel=OPEN",
        stage_labels[index],
        static_cast<unsigned long long>(n),
        static_cast<unsigned long long>(call),
        static_cast<unsigned long long>(stats.init_accepted),
        static_cast<unsigned long long>(stats.init_called),
        static_cast<unsigned long long>(stats.writer_accepted),
        static_cast<unsigned>(probe));
    reshade::log::message(reshade::log::level::info,line);
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
    using dsrrl::runtime::v206_native_t1::probe_status;
    auto &registry=dsrrl::runtime::v206_native_t1::instance();
    const auto call=g_writer_calls.fetch_add(1u,
        std::memory_order_relaxed)+1u;
    auto reject=[&](writer_stage stage,
                    probe_status probe=probe_status::view_absent) noexcept {
        stage_log(stage,call,probe);
    };
    const auto flags=dsrrl::runtime::texture_identity_transport::status();
    if(!flags.provenance_ok || !flags.name_hook_armed ||
       !cache_entry || !engine_object || !source_id)
        return reject(writer_stage::input);
    const wchar_t *name=nullptr;
    std::size_t length=0u;
    if(!dsrrl::runtime::texture_identity_transport::snapshot_raw(
            name,length) || !name || length<6u || length>64u)
        return reject(writer_stage::tls);
    const auto *slot=static_cast<const std::uint8_t *>(cache_entry)+8u;
    if(!readable(slot,sizeof(const wchar_t *)))
        return reject(writer_stage::cache_pointer);
    const wchar_t *node_name=nullptr;
    std::memcpy(&node_name,slot,sizeof(node_name));
    if(!node_name || !readable(node_name,(length+1u)*sizeof(wchar_t)) ||
       node_name[length]!=L'\0' ||
       std::char_traits<wchar_t>::compare(node_name,name,length)!=0)
        return reject(writer_stage::cache_name);
    const auto host_base=reinterpret_cast<std::uintptr_t>(
        GetModuleHandleW(nullptr));
    std::uintptr_t vtable=0u;
    if(!host_base || !readable(engine_object,sizeof(vtable)))
        return reject(writer_stage::engine_read);
    std::memcpy(&vtable,engine_object,sizeof(vtable));
    if(vtable!=host_base+0x014AA4A8u)
        return reject(writer_stage::vtable);
    const auto *fields=static_cast<const std::uint8_t *>(engine_object)+0x28u;
    std::array<std::uintptr_t,2u> pair{};
    if(!readable(fields,sizeof(pair)))
        return reject(writer_stage::native_fields);
    std::memcpy(pair.data(),fields,sizeof(pair));
    if(!pair[0] || !pair[1])
        return reject(writer_stage::native_pair);
    std::array<char,65u> ascii{};
    for(std::size_t i=0u;i<length;++i){
        if(name[i]<0x21u || name[i]>0x7eu)
            return reject(writer_stage::non_ascii);
        ascii[i]=static_cast<char>(name[i]);
    }
    if(!dsrrl::runtime::v206_native_t1::registry<>::name_is_equipment_spec(
           ascii.data(),length))
        return reject(writer_stage::non_equipment);
    using writer_result=dsrrl::runtime::v206_native_t1::registry<>::writer_result;
    const auto result=registry.observe_writer_diagnostic(
        pair[1],pair[0],ascii.data(),length,source_id);
    if(result!=writer_result::published){
        switch(result){
        case writer_result::rejected_name:
            return reject(writer_stage::non_equipment);
        case writer_result::view_absent:
            return reject(writer_stage::no_view,probe_status::view_absent);
        case writer_result::resource_mismatch:
            return reject(writer_stage::wrong_resource,
                          probe_status::resource_mismatch);
        case writer_result::view_ambiguous:
            return reject(writer_stage::ambiguous_view,
                          probe_status::view_ambiguous);
        case writer_result::snapshot_capacity:
            return reject(writer_stage::no_writer_or_capacity,
                          probe_status::writer_absent);
        case writer_result::published: break;
        }
    }
    stage_log(writer_stage::published,call,probe_status::matched);
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
