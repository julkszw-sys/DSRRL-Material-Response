#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/dof_authored_state_runtime.hpp"

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/dof_process_memory.hpp"
#include "dsrrl/operators/dof/ptde_dofbank_embedded.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl::runtime::dof {
namespace {

using dsrrl::operators::dof::ptde_bank::decoded_state;

constexpr std::uintptr_t k_rva_dofbank_blend = 0x5627E0u;
constexpr std::uintptr_t k_rva_security_cookie = 0x1AAB820u;
constexpr std::size_t k_stolen = 14u;

constexpr std::array<std::uint8_t,k_stolen> k_expected = {
    0x48,0x83,0xEC,0x38,
    0x48,0x8B,0x05,0x35,0x90,0x54,0x01,
    0x48,0x33,0xC4
};

using producer_fn = void(__fastcall *)(
    float *out,
    void *source_a,
    std::uint32_t selector_a,
    void *source_b,
    std::uint32_t selector_b,
    float beta);

core::renderer_core *g_core = nullptr;
std::uintptr_t g_base = 0;
void *g_target = nullptr;
void *g_trampoline = nullptr;
producer_fn g_original = nullptr;
std::array<std::uint8_t,k_stolen> g_original_bytes{};
bool g_patched = false;

std::atomic<std::uint64_t> g_calls{0};
std::atomic<std::uint64_t> g_matches{0};
std::atomic<std::uint64_t> g_misses{0};
std::atomic<std::uint64_t> g_applied{0};

struct cached_bank {
    const std::uint8_t *param = nullptr;
    int slot = -1;
};
thread_local std::array<cached_bank,4> g_bank_cache{};
thread_local std::size_t g_bank_cache_cursor = 0u;

bool write_bytes(void *at,const void *src,std::size_t n) noexcept
{
    DWORD old=0;
    if(!VirtualProtect(at,n,PAGE_EXECUTE_READWRITE,&old))
        return false;
    std::memcpy(at,src,n);
    const bool flushed=
        FlushInstructionCache(GetCurrentProcess(),at,n)!=FALSE;
    DWORD ignored=0;
    VirtualProtect(at,n,old,&ignored);
    return flushed;
}

bool read_param_base(void *source,const std::uint8_t *&param) noexcept
{
    param=nullptr;
    if(!source)
        return false;

    std::int32_t type=0;
    if(!process_memory::safe_read_bytes(
           static_cast<const std::uint8_t *>(source)+0x28u,
           &type,sizeof(type)) ||
       type!=5)
        return false;

    if(!process_memory::safe_read_bytes(
           static_cast<const std::uint8_t *>(source)+0x38u,
           &param,sizeof(param)) ||
       !param)
        return false;

    std::array<char,8> tag{};
    if(!process_memory::safe_read_bytes(param+0x0Cu,tag.data(),tag.size()) ||
       std::memcmp(tag.data(),"DOF_BANK",8)!=0)
        return false;

    std::uint16_t count=0;
    return
        process_memory::safe_read_bytes(param+0x0Au,&count,sizeof(count)) &&
        count==64u;
}

std::uint64_t fnv_byte(std::uint64_t h,std::uint8_t value) noexcept
{
    h^=value;
    return h*0x100000001b3ULL;
}

bool structural_signature(
    const std::uint8_t *param,
    std::uint64_t &signature) noexcept
{
    if(!param)
        return false;

    std::uint16_t count=0;
    if(!process_memory::safe_read_bytes(param+0x0Au,&count,sizeof(count)) ||
       count!=64u)
        return false;

    std::uint64_t h=0xcbf29ce484222325ULL;
    h=fnv_byte(h,static_cast<std::uint8_t>(count));
    h=fnv_byte(h,static_cast<std::uint8_t>(count>>8));

    for(std::uint32_t i=0;i<count;++i){
        const auto *entry=param+0x30u+static_cast<std::size_t>(i)*12u;
        std::uint32_t id=0,name_offset=0;
        if(!process_memory::safe_read_bytes(entry,&id,sizeof(id)) ||
           !process_memory::safe_read_bytes(entry+8u,&name_offset,sizeof(name_offset)))
            return false;

        for(unsigned shift=0;shift<32u;shift+=8u)
            h=fnv_byte(h,static_cast<std::uint8_t>(id>>shift));

        if(name_offset!=0u){
            if(name_offset>0x100000u)
                return false;
            bool terminated=false;
            for(std::uint32_t j=0;j<512u;++j){
                std::uint8_t c=0;
                if(!process_memory::safe_read_bytes(param+name_offset+j,&c,1u))
                    return false;
                if(c==0u){
                    terminated=true;
                    break;
                }
                h=fnv_byte(h,c);
            }
            if(!terminated)
                return false;
        }

        h=fnv_byte(h,0u);
    }

    signature=h;
    return true;
}

int resolve_area_slot(void *source) noexcept
{
    const std::uint8_t *param=nullptr;
    if(!read_param_base(source,param))
        return -1;

    for(const auto &cached:g_bank_cache)
        if(cached.param==param)
            return cached.slot;

    std::uint64_t signature=0;
    const int slot=
        structural_signature(param,signature) ?
        dsrrl::operators::dof::ptde_bank::area_slot_from_signature(signature) :
        -1;

    g_bank_cache[g_bank_cache_cursor]={param,slot};
    g_bank_cache_cursor=(g_bank_cache_cursor+1u)%g_bank_cache.size();
    return slot;
}

void copy_state(float *out,const decoded_state &state) noexcept
{
    const std::array<float,7> values={
        state.far_begin,
        state.far_end,
        state.far_mul,
        state.near_begin,
        state.near_end,
        state.near_mul,
        state.dispersion_sq
    };
    std::memcpy(out,values.data(),sizeof(values));
}

void __fastcall hook_producer(
    float *out,
    void *source_a,
    std::uint32_t selector_a,
    void *source_b,
    std::uint32_t selector_b,
    float beta) noexcept
{
    ++g_calls;

    if(g_original)
        g_original(out,source_a,selector_a,source_b,selector_b,beta);

    if(!out)
        return;

    const int area_a=resolve_area_slot(source_a);
    const int area_b=resolve_area_slot(source_b);
    if(area_a<0 || area_b<0 || selector_a>=64u || selector_b>=64u){
        ++g_misses;
        return;
    }

    ++g_matches;

    // Authored PTDE state is embedded and route-verified at this point, but
    // visible mutation remains tied to the DoF island feature gate. The
    // current manifest keeps post_dof_ptde hard-blocked until the resource
    // graph/output cut is closed, so this hook is observational today.
    if(!g_core ||
       !g_core->features().enabled(core::operator_id::post_dof_ptde))
        return;

    decoded_state state{};
    if(!dsrrl::operators::dof::ptde_bank::blend(
           static_cast<std::size_t>(area_a),selector_a,
           static_cast<std::size_t>(area_b),selector_b,
           beta,state))
        return;

    copy_state(out,state);
    ++g_applied;
}

bool build_trampoline() noexcept
{
    auto *mem=static_cast<std::uint8_t *>(
        VirtualAlloc(nullptr,64u,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if(!mem)
        return false;

    std::size_t p=0u;
    const std::array<std::uint8_t,4> sub_rsp={0x48,0x83,0xEC,0x38};
    std::memcpy(mem+p,sub_rsp.data(),sub_rsp.size()); p+=sub_rsp.size();

    // Relocate the stolen RIP-relative security-cookie load explicitly:
    // mov rax, <cookie address>; mov rax, [rax]; xor rax, rsp.
    mem[p++]=0x48; mem[p++]=0xB8;
    const std::uint64_t cookie=
        static_cast<std::uint64_t>(g_base+k_rva_security_cookie);
    std::memcpy(mem+p,&cookie,sizeof(cookie)); p+=sizeof(cookie);
    mem[p++]=0x48; mem[p++]=0x8B; mem[p++]=0x00;
    mem[p++]=0x48; mem[p++]=0x33; mem[p++]=0xC4;

    // Absolute indirect jump to the first non-stolen instruction.
    mem[p++]=0xFF; mem[p++]=0x25;
    const std::uint32_t zero=0u;
    std::memcpy(mem+p,&zero,sizeof(zero)); p+=sizeof(zero);
    const std::uint64_t resume=
        static_cast<std::uint64_t>(g_base+k_rva_dofbank_blend+k_stolen);
    std::memcpy(mem+p,&resume,sizeof(resume)); p+=sizeof(resume);

    FlushInstructionCache(GetCurrentProcess(),mem,p);
    g_trampoline=mem;
    g_original=reinterpret_cast<producer_fn>(g_trampoline);
    return true;
}

bool patch_entry() noexcept
{
    auto *target=
        reinterpret_cast<std::uint8_t *>(g_base+k_rva_dofbank_blend);
    if(!process_memory::safe_read_bytes(
           target,g_original_bytes.data(),g_original_bytes.size()) ||
       g_original_bytes!=k_expected)
        return false;

    if(!build_trampoline())
        return false;

    std::array<std::uint8_t,k_stolen> patch{};
    patch[0]=0xFF; patch[1]=0x25;
    const std::uint32_t zero=0u;
    std::memcpy(patch.data()+2u,&zero,sizeof(zero));
    const std::uint64_t dest=
        reinterpret_cast<std::uint64_t>(&hook_producer);
    std::memcpy(patch.data()+6u,&dest,sizeof(dest));

    g_target=target;
    g_patched=true;
    return write_bytes(g_target,patch.data(),patch.size());
}

void restore() noexcept
{
    if(g_patched && g_target)
        write_bytes(g_target,g_original_bytes.data(),g_original_bytes.size());
    g_patched=false;
    g_target=nullptr;
    g_original=nullptr;

    if(g_trampoline){
        VirtualFree(g_trampoline,0,MEM_RELEASE);
        g_trampoline=nullptr;
    }
}

} // namespace

bool register_authored_state_runtime(core::renderer_core &core) noexcept
{
    g_core=&core;
    g_base=process_memory::image_base();
    g_calls.store(0);
    g_matches.store(0);
    g_misses.store(0);
    g_applied.store(0);

    if(!g_base || !patch_entry()){
        restore();
        g_core=nullptr;
        g_base=0;
        return false;
    }
    return true;
}

void unregister_authored_state_runtime() noexcept
{
    restore();
    g_core=nullptr;
    g_base=0;
}

authored_state_telemetry authored_state_status() noexcept
{
    return {
        g_calls.load(),
        g_matches.load(),
        g_misses.load(),
        g_applied.load(),
        g_patched && g_original!=nullptr
    };
}

} // namespace dsrrl::runtime::dof
