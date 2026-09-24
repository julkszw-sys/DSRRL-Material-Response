#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/upper_lower_runtime.hpp"
#include "dsrrl/runtime/engine_hooks.hpp"
#include "dsrrl/core/renderer_core.hpp"

#include <reshade.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace dsrrl::runtime::upper_lower {
namespace {

constexpr std::uintptr_t k_rva_wrapper_type5 = 0x1C0BE0;
constexpr std::uintptr_t k_rva_wrapper_type6 = 0x1C0C10;
constexpr std::uintptr_t k_rva_blend_helper  = 0x5642F0;
constexpr std::uintptr_t k_rva_steady_packer = 0x563B80;

constexpr std::uintptr_t k_ret_blend_upper = 0x5639BA;
constexpr std::uintptr_t k_ret_blend_lower = 0x5639D5;
constexpr std::uintptr_t k_ret_sel_1 = 0x20E019;
constexpr std::uintptr_t k_ret_sel_2 = 0x20EB7F;
constexpr std::uintptr_t k_ret_sel_3 = 0x20FB9E;

constexpr std::array<std::uint8_t,17> k_wrapper_bytes = {
    0x48,0x83,0xEC,0x38,0x4D,0x8B,0xC8,0xF3,0x0F,0x11,0x5C,0x24,0x20,0x4C,0x8B,0x41,0x40
};
constexpr std::array<std::uint8_t,19> k_blend_bytes = {
    0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x81,0xEC,0xC0,0x00,0x00,0x00
};
constexpr std::array<std::uint8_t,14> k_steady_packer_bytes = {
    0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x40,0x48,0x8B,0x41,0x18
};

constexpr std::size_t k_record_stride = 0x110u;
constexpr std::size_t k_q_upper_offset = 0x60u;
constexpr std::size_t k_q_lower_offset = 0x70u;
constexpr float k_inv_pow = 1.0f / 2.2f;

struct f4 { float x=0.0f,y=0.0f,z=0.0f,w=0.0f; };
#pragma pack(push,1)
struct raw_rgbm { std::int16_t r,g,b,m; };
#pragma pack(pop)

struct snapshot {
    std::uintptr_t owner = 0;
    std::uint16_t a = 0;
    std::uint16_t b = 0;
    std::uint32_t beta_bits = 0;
    alignas(16) std::array<f4,8> payload{};
    mutable std::mutex gpu_mutex;
    mutable ID3D11Device *device = nullptr;
    mutable ID3D11Buffer *buffer = nullptr;

    ~snapshot()
    {
        if(buffer) buffer->Release();
        if(device) device->Release();
    }
};

struct producer_tls {
    bool active = false;
    std::uintptr_t owner = 0;
    const std::uint8_t *assignment = nullptr;
    bool have_upper = false;
    bool have_lower = false;
    f4 upper{};
    f4 lower{};
};

struct inline_hook {
    void *target = nullptr;
    void *trampoline = nullptr;
    void *detour = nullptr;
    std::size_t stolen = 0;
    std::array<std::uint8_t,32> original{};
    bool patched = false;
};

using wrapper_fn = void *(__fastcall *)(void *,void *,void *,float);
using blend_fn = void *(__fastcall *)(void *,const raw_rgbm *,const raw_rgbm *,float);
using steady_packer_fn = void (__fastcall *)(void *,void *,std::int32_t);

core::renderer_core *g_core = nullptr;
std::uintptr_t g_base = 0;
std::array<inline_hook,4> g_hooks{};

wrapper_fn g_wrapper5_orig = nullptr;
wrapper_fn g_wrapper6_orig = nullptr;
blend_fn g_blend_orig = nullptr;
steady_packer_fn g_steady_packer_orig = nullptr;

std::mutex g_snapshot_mutex;
std::unordered_map<std::uintptr_t,std::shared_ptr<const snapshot>> g_snapshots;
thread_local producer_tls g_prod;
thread_local std::shared_ptr<const snapshot> g_draw_snapshot;

std::atomic<bool> g_enabled{false};
std::atomic<bool> g_quarantined{false};
std::atomic<std::uint64_t> g_wrapper5{0},g_wrapper6{0},g_steady_seen{0},g_steady_pass{0};
std::atomic<std::uint64_t> g_blend_seen{0},g_blend_upper{0},g_blend_lower{0};
std::atomic<std::uint64_t> g_snapshot_publish{0},g_selector_seen{0},g_selector_match{0};
std::atomic<std::uint64_t> g_selector_miss{0},g_tuple_mismatch{0};
std::atomic<std::uint64_t> g_b13_create{0},g_b13_hit{0},g_bind{0},g_restore_fail{0};

void log_info(const std::string &s){ reshade::log::message(reshade::log::level::info,s.c_str()); }
void log_warn(const std::string &s){ reshade::log::message(reshade::log::level::warning,s.c_str()); }

template<class T>
bool safe_read(const void *p,T &out) noexcept
{
    return p && engine::safe_read_bytes(p,&out,sizeof(out));
}

bool write_bytes(void *at,const void *src,std::size_t n) noexcept
{
    DWORD old=0;
    if(!VirtualProtect(at,n,PAGE_EXECUTE_READWRITE,&old)) return false;
    std::memcpy(at,src,n);
    const bool flushed=FlushInstructionCache(GetCurrentProcess(),at,n)!=FALSE;
    DWORD ignored=0;
    VirtualProtect(at,n,old,&ignored);
    return flushed;
}

template<std::size_t N>
bool prepare_hook(inline_hook &h,std::uintptr_t rva,const std::array<std::uint8_t,N> &expected,void *detour) noexcept
{
    static_assert(N>=14 && N<=32);
    auto *target=reinterpret_cast<std::uint8_t *>(g_base+rva);
    std::array<std::uint8_t,N> got{};
    if(!engine::safe_read_bytes(target,got.data(),got.size()) || got!=expected) return false;

    void *tr=VirtualAlloc(nullptr,N+14,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!tr) return false;
    std::memcpy(tr,target,N);
    auto *tail=static_cast<std::uint8_t *>(tr)+N;
    tail[0]=0xFF; tail[1]=0x25;
    std::uint32_t zero=0; std::memcpy(tail+2,&zero,4);
    const std::uint64_t back=reinterpret_cast<std::uint64_t>(target+N);
    std::memcpy(tail+6,&back,8);
    FlushInstructionCache(GetCurrentProcess(),tr,N+14);

    h.target=target;
    h.trampoline=tr;
    h.detour=detour;
    h.stolen=N;
    std::copy(got.begin(),got.end(),h.original.begin());
    return true;
}

bool patch_hook(inline_hook &h) noexcept
{
    if(!h.target || !h.trampoline || !h.detour || h.stolen<14 || h.stolen>h.original.size())
        return false;
    std::array<std::uint8_t,32> patch{};
    patch.fill(0x90);
    patch[0]=0xFF; patch[1]=0x25;
    std::uint32_t zero=0; std::memcpy(patch.data()+2,&zero,4);
    const std::uint64_t dest=reinterpret_cast<std::uint64_t>(h.detour);
    std::memcpy(patch.data()+6,&dest,8);
    if(!write_bytes(h.target,patch.data(),h.stolen)) return false;
    h.patched=true;
    return true;
}

void restore_hook(inline_hook &h) noexcept
{
    if(h.patched){
        (void)write_bytes(h.target,h.original.data(),h.stolen);
        h.patched=false;
    }
    if(h.trampoline){
        VirtualFree(h.trampoline,0,MEM_RELEASE);
        h.trampoline=nullptr;
    }
    h.target=nullptr; h.detour=nullptr; h.stolen=0;
}

void restore_hooks() noexcept
{
    for(auto &h:g_hooks) restore_hook(h);
    g_wrapper5_orig=nullptr;
    g_wrapper6_orig=nullptr;
    g_blend_orig=nullptr;
    g_steady_packer_orig=nullptr;
}

bool inverse_q(float q,float &out) noexcept
{
    if(!std::isfinite(q) || q<0.0f) return false;
    out = q==0.0f ? 0.0f : std::pow(q,k_inv_pow);
    return std::isfinite(out);
}

bool read_selected_ptde(void *source,std::int32_t selector,f4 &upper,f4 &lower) noexcept
{
    if(!source || selector<0) return false;

    const std::uint8_t *header=nullptr;
    if(!safe_read(static_cast<const std::uint8_t *>(source)+0x18,header) || !header) return false;

    std::uint16_t count=0;
    if(!safe_read(header+0x0A,count) || static_cast<std::uint32_t>(selector)>=count) return false;

    const std::uint8_t *records=nullptr;
    if(!safe_read(static_cast<const std::uint8_t *>(source)+0x20,records) || !records) return false;
    const auto *record=records+static_cast<std::size_t>(selector)*k_record_stride;

    f4 q_upper{},q_lower{};
    if(!safe_read(record+k_q_upper_offset,q_upper) || !safe_read(record+k_q_lower_offset,q_lower))
        return false;

    f4 u{},l{};
    if(!inverse_q(q_upper.x,u.x) || !inverse_q(q_upper.y,u.y) || !inverse_q(q_upper.z,u.z) ||
       !inverse_q(q_lower.x,l.x) || !inverse_q(q_lower.y,l.y) || !inverse_q(q_lower.z,l.z))
        return false;
    upper={u.x,u.y,u.z,0.0f};
    lower={l.x,l.y,l.z,0.0f};
    return true;
}

f4 decode_rgbm(const raw_rgbm &v) noexcept
{
    const float scale=static_cast<float>(v.m)/100.0f;
    return {
        static_cast<float>(v.r)/255.0f*scale,
        static_cast<float>(v.g)/255.0f*scale,
        static_cast<float>(v.b)/255.0f*scale,
        0.0f
    };
}

f4 lerp4(const f4 &a,const f4 &b,float t) noexcept
{
    return {
        a.x+(b.x-a.x)*t,
        a.y+(b.y-a.y)*t,
        a.z+(b.z-a.z)*t,
        0.0f
    };
}

void publish_snapshot(const producer_tls &p) noexcept
{
    if(!p.active || !p.owner || !p.assignment || !p.have_upper || !p.have_lower) return;

    std::uint16_t a=0,b=0;
    std::uint32_t beta=0;
    if(!safe_read(p.assignment+8,a) || !safe_read(p.assignment+10,b) || !safe_read(p.assignment+12,beta))
        return;

    auto s=std::make_shared<snapshot>();
    s->owner=p.owner;
    s->a=a; s->b=b; s->beta_bits=beta;
    s->payload[6]=p.upper;
    s->payload[7]=p.lower;
    {
        std::lock_guard lock(g_snapshot_mutex);
        g_snapshots[p.owner]=s;
    }
    ++g_snapshot_publish;
}

void *run_wrapper(wrapper_fn original,std::atomic<std::uint64_t> &counter,
                  void *rcx,void *owner,void *assignment,float x) noexcept
{
    ++counter;
    const auto previous=g_prod;
    g_prod={true,reinterpret_cast<std::uintptr_t>(owner),
            static_cast<const std::uint8_t *>(assignment),false,false,{},{}};
    void *result=original ? original(rcx,owner,assignment,x) : nullptr;
    const auto completed=g_prod;
    g_prod=previous;
    publish_snapshot(completed);
    return result;
}

void *__fastcall hook_wrapper5(void *rcx,void *owner,void *assignment,float x) noexcept
{
    return run_wrapper(g_wrapper5_orig,g_wrapper5,rcx,owner,assignment,x);
}
void *__fastcall hook_wrapper6(void *rcx,void *owner,void *assignment,float x) noexcept
{
    return run_wrapper(g_wrapper6_orig,g_wrapper6,rcx,owner,assignment,x);
}

void __fastcall hook_steady_packer(void *source,void *dst,std::int32_t selector) noexcept
{
    ++g_steady_seen;
    if(g_steady_packer_orig) g_steady_packer_orig(source,dst,selector);

    if(!g_prod.active) return;
    f4 upper{},lower{};
    if(read_selected_ptde(source,selector,upper,lower)){
        g_prod.upper=upper;
        g_prod.lower=lower;
        g_prod.have_upper=true;
        g_prod.have_lower=true;
        ++g_steady_pass;
    }
}

void *__fastcall hook_blend(void *dst,const raw_rgbm *a,const raw_rgbm *b,float beta) noexcept
{
    ++g_blend_seen;
    const auto base=g_base;
#if defined(_MSC_VER)
    const auto ret=reinterpret_cast<std::uintptr_t>(_ReturnAddress())-base;
#else
    const auto ret=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))-base;
#endif
    if(g_prod.active && a && b && (ret==k_ret_blend_upper || ret==k_ret_blend_lower)){
        raw_rgbm ra{},rb{};
        if(safe_read(a,ra) && safe_read(b,rb)){
            const auto value=lerp4(decode_rgbm(ra),decode_rgbm(rb),beta);
            if(ret==k_ret_blend_upper){
                g_prod.upper=value; g_prod.have_upper=true; ++g_blend_upper;
            }else{
                g_prod.lower=value; g_prod.have_lower=true; ++g_blend_lower;
            }
        }
    }
    return g_blend_orig ? g_blend_orig(dst,a,b,beta) : nullptr;
}

bool install_hooks() noexcept
{
    if(!g_base) return false;
    if(!prepare_hook(g_hooks[0],k_rva_wrapper_type5,k_wrapper_bytes,
                     reinterpret_cast<void *>(&hook_wrapper5)) ||
       !prepare_hook(g_hooks[1],k_rva_wrapper_type6,k_wrapper_bytes,
                     reinterpret_cast<void *>(&hook_wrapper6)) ||
       !prepare_hook(g_hooks[2],k_rva_blend_helper,k_blend_bytes,
                     reinterpret_cast<void *>(&hook_blend)) ||
       !prepare_hook(g_hooks[3],k_rva_steady_packer,k_steady_packer_bytes,
                     reinterpret_cast<void *>(&hook_steady_packer))){
        restore_hooks();
        return false;
    }

    g_wrapper5_orig=reinterpret_cast<wrapper_fn>(g_hooks[0].trampoline);
    g_wrapper6_orig=reinterpret_cast<wrapper_fn>(g_hooks[1].trampoline);
    g_blend_orig=reinterpret_cast<blend_fn>(g_hooks[2].trampoline);
    g_steady_packer_orig=reinterpret_cast<steady_packer_fn>(g_hooks[3].trampoline);

    for(auto &h:g_hooks){
        if(!patch_hook(h)){
            restore_hooks();
            return false;
        }
    }
    return true;
}

void on_destroy_device(reshade::api::device *device)
{
    if(!device || device->get_api()!=reshade::api::device_api::d3d11)
        return;

    // b13 buffers are device-owned. Never retain COM references or a selected
    // snapshot across a D3D11 device teardown/recreation boundary; fail open
    // until the producer publishes a fresh semantic snapshot.
    g_draw_snapshot.reset();
    std::lock_guard lock(g_snapshot_mutex);
    g_snapshots.clear();
}

ID3D11Buffer *realize_b13(const std::shared_ptr<const snapshot> &s,ID3D11Device *device) noexcept
{
    if(!s || !device) return nullptr;
    std::lock_guard lock(s->gpu_mutex);
    if(s->buffer){
        if(s->device!=device) return nullptr;
        s->buffer->AddRef();
        ++g_b13_hit;
        return s->buffer;
    }

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth=128;
    desc.Usage=D3D11_USAGE_IMMUTABLE;
    desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem=s->payload.data();

    ID3D11Buffer *buffer=nullptr;
    if(FAILED(device->CreateBuffer(&desc,&init,&buffer)) || !buffer) return nullptr;
    s->device=device; device->AddRef();
    s->buffer=buffer;
    buffer->AddRef();
    ++g_b13_create;
    return buffer;
}

void release_state(draw_state &s) noexcept
{
    if(s.old_base) s.old_base->Release();
    if(s.old_window) s.old_window->Release();
    if(s.replacement) s.replacement->Release();
    s={};
}

} // namespace

bool register_runtime(core::renderer_core &core) noexcept
{
    g_core=&core;
    g_base=engine::image_base();
    g_quarantined.store(false);
    g_enabled.store(false);

    if(!g_base || !install_hooks()){
        log_warn("DSRRL Runtime U/L: exact producer hook preflight failed; U/L fail-open.");
        restore_hooks();
        g_core=nullptr;
        return false;
    }

    reshade::register_event<reshade::addon_event::destroy_device>(
        on_destroy_device);
    g_enabled.store(true);
    log_info("DSRRL Runtime U/L: steady 0x563B80 + blend 0x5642F0 producer capture armed; shared selector only.");
    return true;
}

void unregister_runtime() noexcept
{
    g_enabled.store(false);
    reshade::unregister_event<reshade::addon_event::destroy_device>(
        on_destroy_device);
    restore_hooks();
    consume_draw_selection();
    {
        std::lock_guard lock(g_snapshot_mutex);
        g_snapshots.clear();
    }
    g_core=nullptr;
    g_base=0;
}

void selector_event(void *,void *owner,void *ret,void *r14,void *r15,std::int32_t) noexcept
{
    ++g_selector_seen;
    g_draw_snapshot.reset();
    if(!g_enabled.load() || g_quarantined.load() || !owner || !ret) return;

    const auto rva=reinterpret_cast<std::uintptr_t>(ret)-g_base;
    const std::uint8_t *desc=nullptr;
    if(rva==k_ret_sel_1 || rva==k_ret_sel_3)
        desc=static_cast<const std::uint8_t *>(r15);
    else if(rva==k_ret_sel_2)
        desc=static_cast<const std::uint8_t *>(r14);
    else
        return;

    if(!desc){++g_selector_miss;return;}
    std::uint16_t a=0,b=0;
    std::uint32_t beta=0;
    if(!safe_read(desc+0x4C,a) || !safe_read(desc+0x4E,b) || !safe_read(desc+0x50,beta)){
        ++g_selector_miss; return;
    }

    std::shared_ptr<const snapshot> s;
    {
        std::lock_guard lock(g_snapshot_mutex);
        const auto it=g_snapshots.find(reinterpret_cast<std::uintptr_t>(owner));
        if(it!=g_snapshots.end()) s=it->second;
    }
    if(!s){++g_selector_miss;return;}
    if(s->a!=a || s->b!=b || s->beta_bits!=beta){
        ++g_tuple_mismatch; return;
    }
    g_draw_snapshot=std::move(s);
    ++g_selector_match;
}

bool selected_snapshot_ready() noexcept
{
    return g_enabled.load() && !g_quarantined.load() && static_cast<bool>(g_draw_snapshot);
}

bool bind_draw(ID3D11DeviceContext *context,draw_state &state) noexcept
{
    state={};
    if(!context || !selected_snapshot_ready()) return false;

    ID3D11Device *device=nullptr;
    context->GetDevice(&device);
    if(!device) return false;

    state.replacement=realize_b13(g_draw_snapshot,device);
    device->Release();
    if(!state.replacement) return false;

    context->PSGetConstantBuffers(13,1,&state.old_base);

    ID3D11DeviceContext1 *context1=nullptr;
    if(SUCCEEDED(context->QueryInterface(__uuidof(ID3D11DeviceContext1),
                                         reinterpret_cast<void **>(&context1))) && context1){
        context1->PSGetConstantBuffers1(13,1,&state.old_window,&state.first,&state.count);
        state.coherent=state.old_base==state.old_window;
        state.explicit_window=state.old_window && state.count>=16 &&
                              (state.first%16)==0 && (state.count%16)==0;
        context1->Release();
    }

    if(!state.coherent){
        release_state(state);
        return false;
    }

    ID3D11Buffer *owned=state.replacement;
    context->PSSetConstantBuffers(13,1,&owned);
    state.bound=true;
    ++g_bind;
    return true;
}

bool restore_draw(ID3D11DeviceContext *context,draw_state &state) noexcept
{
    if(!context){
        release_state(state);
        return false;
    }

    bool ok=true;
    if(state.bound){
        ID3D11DeviceContext1 *context1=nullptr;
        if(SUCCEEDED(context->QueryInterface(__uuidof(ID3D11DeviceContext1),
                                             reinterpret_cast<void **>(&context1))) && context1){
            if(state.explicit_window){
                ID3D11Buffer *b=state.old_window;
                UINT first=state.first,count=state.count;
                context1->PSSetConstantBuffers1(13,1,&b,&first,&count);
            }else{
                ID3D11Buffer *b=state.old_base;
                context->PSSetConstantBuffers(13,1,&b);
            }

            ID3D11Buffer *base=nullptr,*window=nullptr;
            UINT first=0,count=0;
            context->PSGetConstantBuffers(13,1,&base);
            context1->PSGetConstantBuffers1(13,1,&window,&first,&count);
            ok=base==state.old_base && window==state.old_window &&
               (!state.explicit_window || (first==state.first && count==state.count));
            if(base) base->Release();
            if(window) window->Release();
            context1->Release();
        }else{
            ID3D11Buffer *b=state.old_base;
            context->PSSetConstantBuffers(13,1,&b);
            ID3D11Buffer *check=nullptr;
            context->PSGetConstantBuffers(13,1,&check);
            ok=check==state.old_base;
            if(check) check->Release();
        }
    }

    if(!ok){
        ++g_restore_fail;
        g_quarantined.store(true);
        log_warn("DSRRL Runtime U/L: CB13 restore mismatch; U/L quarantined.");
    }
    release_state(state);
    return ok;
}

void consume_draw_selection() noexcept
{
    g_draw_snapshot.reset();
}

} // namespace dsrrl::runtime::upper_lower
