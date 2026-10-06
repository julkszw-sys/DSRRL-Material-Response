#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/dof_authored_state_runtime.hpp"

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/dof_process_memory.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace dsrrl::runtime::dof {
namespace {

struct decoded_state {
    float far_begin = 0.0f;
    float far_end = 0.0f;
    float far_mul = 0.0f;
    float near_begin = 0.0f;
    float near_end = 0.0f;
    float near_mul = 0.0f;
    float dispersion_sq = 0.0f;
};

struct live_dof_row_v1 {
    float far_begin = 0.0f;
    float far_end = 0.0f;
    std::uint8_t far_mul = 0u;
    std::array<std::uint8_t,3> far_mul_pad{};
    float near_begin = 0.0f;
    float near_end = 0.0f;
    std::uint8_t near_mul = 0u;
    std::array<std::uint8_t,3> near_mul_pad{};
    float dispersion_sq = 0.0f;
};
static_assert(sizeof(live_dof_row_v1) == 28u);

// Narrow routing authority only. No PTDE DoF payload lives in the binary.
constexpr std::array<std::uint64_t,10> k_ptde_homologous_dofbank_signatures = {{
    0x96ff83e97066616fULL, // m10_DofBank.param
    0x4369cfd16580100bULL, // m11_DofBank.param
    0xcf06079b3d769e22ULL, // m12_DofBank.param
    0x8495ff3b666052a9ULL, // m13_DofBank.param
    0x5cefc2fa3d4e63d2ULL, // m14_DofBank.param
    0x23e4ac6187d0387bULL, // m15_1_DofBank.param
    0xc8c2fbb1f61b4915ULL, // m15_DofBank.param
    0x5b8ca0a616a3898bULL, // m16_DofBank.param
    0x6e971c759010210dULL, // m17_DofBank.param
    0x600fa9b5f6e6446dULL, // m18_DofBank.param
}};

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
    bool authorized = false;
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

bool bank_authorized(void *source,const std::uint8_t *&param) noexcept
{
    param=nullptr;
    if(!read_param_base(source,param))
        return false;

    for(const auto &cached:g_bank_cache)
        if(cached.param==param)
            return cached.authorized;

    std::uint64_t signature=0u;
    bool authorized=false;
    if(structural_signature(param,signature)){
        for(const auto candidate:k_ptde_homologous_dofbank_signatures){
            if(candidate==signature){
                authorized=true;
                break;
            }
        }
    }

    g_bank_cache[g_bank_cache_cursor]={param,authorized};
    g_bank_cache_cursor=(g_bank_cache_cursor+1u)%g_bank_cache.size();
    return authorized;
}

bool read_selected_live_row(
    void *source,
    std::uint32_t selector,
    decoded_state &out) noexcept
{
    const std::uint8_t *param=nullptr;
    if(selector>=64u || !bank_authorized(source,param) || !param)
        return false;

    std::uint32_t data_offset=0u;
    bool found=false;

    {
        const auto *entry=
            param+0x30u+static_cast<std::size_t>(selector)*12u;
        std::uint32_t id=0u;
        if(process_memory::safe_read_bytes(entry,&id,sizeof(id)) &&
           process_memory::safe_read_bytes(
               entry+4u,&data_offset,sizeof(data_offset)) &&
           id==selector)
            found=true;
    }

    if(!found){
        for(std::uint32_t i=0u;i<64u;++i){
            const auto *entry=
                param+0x30u+static_cast<std::size_t>(i)*12u;
            std::uint32_t id=0u,offset=0u;
            if(!process_memory::safe_read_bytes(entry,&id,sizeof(id)) ||
               !process_memory::safe_read_bytes(
                   entry+4u,&offset,sizeof(offset)))
                return false;
            if(id==selector){
                data_offset=offset;
                found=true;
                break;
            }
        }
    }

    if(!found || data_offset<0x330u || data_offset>0x100000u)
        return false;

    live_dof_row_v1 row{};
    if(!process_memory::safe_read_bytes(
           param+data_offset,
           &row,
           sizeof(row)))
        return false;

    out={
        row.far_begin,
        row.far_end,
        static_cast<float>(row.far_mul),
        row.near_begin,
        row.near_end,
        static_cast<float>(row.near_mul),
        row.dispersion_sq
    };

    return
        std::isfinite(out.far_begin) &&
        std::isfinite(out.far_end) &&
        std::isfinite(out.far_mul) &&
        std::isfinite(out.near_begin) &&
        std::isfinite(out.near_end) &&
        std::isfinite(out.near_mul) &&
        std::isfinite(out.dispersion_sq);
}

bool blend_live_rows(
    const decoded_state &a,
    const decoded_state &b,
    float beta,
    decoded_state &out) noexcept
{
    if(!std::isfinite(beta))
        return false;

    const auto lerp=[beta](float x,float y) noexcept {
        return x+(y-x)*beta;
    };

    out={
        lerp(a.far_begin,b.far_begin),
        lerp(a.far_end,b.far_end),
        lerp(a.far_mul,b.far_mul),
        lerp(a.near_begin,b.near_begin),
        lerp(a.near_end,b.near_end),
        lerp(a.near_mul,b.near_mul),
        lerp(a.dispersion_sq,b.dispersion_sq)
    };

    return
        std::isfinite(out.far_begin) &&
        std::isfinite(out.far_end) &&
        std::isfinite(out.far_mul) &&
        std::isfinite(out.near_begin) &&
        std::isfinite(out.near_end) &&
        std::isfinite(out.near_mul) &&
        std::isfinite(out.dispersion_sq);
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

    // Stock DSR owns source and row selection. Read the exact live DrawParam
    // records selected by DSR; do not remap them through an embedded donor table.
    decoded_state a{},b{};
    if(!read_selected_live_row(source_a,selector_a,a) ||
       !read_selected_live_row(source_b,selector_b,b)){
        ++g_misses;
        return;
    }

    ++g_matches;

    if(!g_core ||
       !g_core->features().enabled(core::operator_id::post_dof_ptde))
        return;

    decoded_state state{};
    if(!blend_live_rows(a,b,beta,state)){
        ++g_misses;
        return;
    }

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
