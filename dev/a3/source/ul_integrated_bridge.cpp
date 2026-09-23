#include "ul_integrated_bridge.hpp"

#include <array>
#include <atomic>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

extern "C" void selector_hook_entry();
extern "C" { void *g_selector_trampoline = nullptr; }

namespace dsrrl::a3::ul {
namespace {

constexpr std::uintptr_t RVA_WRAPPER_TYPE5 = 0x1C0BE0;
constexpr std::uintptr_t RVA_WRAPPER_TYPE6 = 0x1C0C10;
constexpr std::uintptr_t RVA_BLEND_HELPER  = 0x5642F0;
constexpr std::uintptr_t RVA_SINGLE_HELPER = 0x564510;
constexpr std::uintptr_t RVA_SELECTOR      = 0x22BA20;

constexpr std::uintptr_t RET_SINGLE_UPPER = 0x563642;
constexpr std::uintptr_t RET_SINGLE_LOWER = 0x563659;
constexpr std::uintptr_t RET_BLEND_UPPER  = 0x5639BA;
constexpr std::uintptr_t RET_BLEND_LOWER  = 0x5639D5;
constexpr std::uintptr_t RET_SEL_1 = 0x20E019;
constexpr std::uintptr_t RET_SEL_2 = 0x20EB7F;
constexpr std::uintptr_t RET_SEL_3 = 0x20FB9E;

constexpr std::array<std::uint8_t,17> BYTES_WRAPPER = {
    0x48,0x83,0xEC,0x38,0x4D,0x8B,0xC8,0xF3,0x0F,0x11,0x5C,0x24,0x20,0x4C,0x8B,0x41,0x40
};
constexpr std::array<std::uint8_t,19> BYTES_BLEND = {
    0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x81,0xEC,0xC0,0x00,0x00,0x00
};
constexpr std::array<std::uint8_t,14> BYTES_SINGLE = {
    0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x40,0x0F,0xBF,0x42,0x06
};
constexpr std::array<std::uint8_t,15> BYTES_SELECTOR = {
    0x40,0x53,0x48,0x83,0xEC,0x30,0x49,0x63,0xC0,0x45,0x8B,0xD1,0x48,0x8B,0xDA
};

std::uintptr_t g_exe_base = 0;
std::atomic<bool> g_quarantined{false};
std::atomic<std::uint64_t> g_snapshots_published{0};
std::atomic<std::uint64_t> g_selector_matches{0};
std::atomic<std::uint64_t> g_prepare_pass{0};
std::atomic<std::uint64_t> g_prepare_failopen{0};
std::atomic<std::uint64_t> g_restore_fail{0};

bool safe_copy(void *dst,const void *src,std::size_t n) noexcept {
#if defined(_MSC_VER)
    __try {
        std::memcpy(dst,src,n);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    if(!dst || !src) return false;
    std::memcpy(dst,src,n);
    return true;
#endif
}
template<class T>
bool safe_read(const void *p,T &out) noexcept {
    return p && safe_copy(&out,p,sizeof(T));
}

#if defined(_MSC_VER)
#define DSRRL_RETURN_ADDRESS() _ReturnAddress()
#else
#define DSRRL_RETURN_ADDRESS() __builtin_return_address(0)
#endif

#pragma pack(push,1)
struct raw_rgbm {
    std::int16_t r,g,b,m;
};
#pragma pack(pop)

struct f4 {
    float x=0,y=0,z=0,w=0;
};

f4 decode_rgbm(const raw_rgbm &v) noexcept {
    const float s=static_cast<float>(v.m)/100.0f;
    return {
        static_cast<float>(v.r)/255.0f*s,
        static_cast<float>(v.g)/255.0f*s,
        static_cast<float>(v.b)/255.0f*s,
        0.0f
    };
}
f4 lerp4(const f4 &a,const f4 &b,float t) noexcept {
    return {
        a.x+(b.x-a.x)*t,
        a.y+(b.y-a.y)*t,
        a.z+(b.z-a.z)*t,
        0.0f
    };
}

struct snapshot {
    std::uintptr_t owner=0;
    std::uint16_t a=0,b=0;
    std::uint32_t beta_bits=0;
    alignas(16) std::array<f4,8> payload{};
    mutable std::mutex gpu_mutex;
    mutable ID3D11Device *device=nullptr;
    mutable ID3D11Buffer *buffer=nullptr;

    ~snapshot() {
        if(buffer) buffer->Release();
        if(device) device->Release();
    }
};

std::mutex g_snapshot_mutex;
std::unordered_map<std::uintptr_t,std::shared_ptr<const snapshot>> g_snapshots;
thread_local std::shared_ptr<const snapshot> g_draw_snapshot;

struct producer_tls {
    bool active=false;
    std::uintptr_t owner=0;
    const std::uint8_t *assignment=nullptr;
    bool have_upper=false,have_lower=false;
    f4 upper{},lower{};
};
thread_local producer_tls g_prod;

using wrapper_fn = void *(__fastcall *)(void *,void *,void *,float);
using blend_fn = void *(__fastcall *)(void *,const raw_rgbm *,const raw_rgbm *,float);
using single_fn = void *(__fastcall *)(void *,const raw_rgbm *,float);

wrapper_fn g_wrapper5_orig=nullptr;
wrapper_fn g_wrapper6_orig=nullptr;
blend_fn g_blend_orig=nullptr;
single_fn g_single_orig=nullptr;

void publish_snapshot(const producer_tls &p) {
    if(!p.active || !p.owner || !p.assignment || !p.have_upper || !p.have_lower)
        return;

    std::uint16_t a=0,b=0;
    std::uint32_t beta=0;
    if(!safe_read(p.assignment+8,a) ||
       !safe_read(p.assignment+10,b) ||
       !safe_read(p.assignment+12,beta))
        return;

    auto s=std::make_shared<snapshot>();
    s->owner=p.owner;
    s->a=a;
    s->b=b;
    s->beta_bits=beta;
    s->payload[6]=p.upper;
    s->payload[7]=p.lower;

    {
        std::lock_guard<std::mutex> lock(g_snapshot_mutex);
        g_snapshots[p.owner]=s;
    }
    g_snapshots_published.fetch_add(1,std::memory_order_relaxed);
}

void *run_wrapper(
    wrapper_fn orig,
    void *rcx,
    void *owner,
    void *assignment,
    float x)
{
    const auto prev=g_prod;
    g_prod={
        true,
        reinterpret_cast<std::uintptr_t>(owner),
        static_cast<const std::uint8_t*>(assignment),
        false,false,{},{},
    };
    void *result=orig(rcx,owner,assignment,x);
    const auto done=g_prod;
    g_prod=prev;
    publish_snapshot(done);
    return result;
}

void *__fastcall hook_wrapper5(void *a,void *owner,void *assignment,float x) {
    return run_wrapper(g_wrapper5_orig,a,owner,assignment,x);
}
void *__fastcall hook_wrapper6(void *a,void *owner,void *assignment,float x) {
    return run_wrapper(g_wrapper6_orig,a,owner,assignment,x);
}

void *__fastcall hook_single(void *dst,const raw_rgbm *v,float w) {
    if(g_prod.active && v) {
        raw_rgbm raw{};
        if(safe_read(v,raw)) {
            const auto r=
                reinterpret_cast<std::uintptr_t>(DSRRL_RETURN_ADDRESS())-g_exe_base;
            if(r==RET_SINGLE_UPPER) {
                g_prod.upper=decode_rgbm(raw);
                g_prod.have_upper=true;
            } else if(r==RET_SINGLE_LOWER) {
                g_prod.lower=decode_rgbm(raw);
                g_prod.have_lower=true;
            }
        }
    }
    return g_single_orig(dst,v,w);
}

void *__fastcall hook_blend(
    void *dst,
    const raw_rgbm *a,
    const raw_rgbm *b,
    float beta)
{
    if(g_prod.active && a && b) {
        raw_rgbm ra{},rb{};
        if(safe_read(a,ra) && safe_read(b,rb)) {
            const f4 v=lerp4(decode_rgbm(ra),decode_rgbm(rb),beta);
            const auto r=
                reinterpret_cast<std::uintptr_t>(DSRRL_RETURN_ADDRESS())-g_exe_base;
            if(r==RET_BLEND_UPPER) {
                g_prod.upper=v;
                g_prod.have_upper=true;
            } else if(r==RET_BLEND_LOWER) {
                g_prod.lower=v;
                g_prod.have_lower=true;
            }
        }
    }
    return g_blend_orig(dst,a,b,beta);
}

struct inline_hook {
    void *target=nullptr;
    void *trampoline=nullptr;
    std::size_t stolen=0;
    std::array<std::uint8_t,32> original{};
};
std::array<inline_hook,5> g_hooks{};

bool write_abs_jump(void *at,void *to,std::size_t span) {
    if(!at || !to || span<14 || span>32)
        return false;

    std::array<std::uint8_t,32> patch{};
    patch.fill(0x90);
    patch[0]=0xFF;
    patch[1]=0x25;
    const std::uint64_t q=reinterpret_cast<std::uint64_t>(to);
    std::memcpy(patch.data()+6,&q,8);

    DWORD old=0;
    if(!VirtualProtect(at,span,PAGE_EXECUTE_READWRITE,&old))
        return false;
    std::memcpy(at,patch.data(),span);
    FlushInstructionCache(GetCurrentProcess(),at,span);
    DWORD dummy=0;
    VirtualProtect(at,span,old,&dummy);
    return true;
}

template<std::size_t N>
bool install_hook(
    inline_hook &h,
    std::uintptr_t rva,
    const std::array<std::uint8_t,N> &expected,
    void *detour)
{
    static_assert(N>=14 && N<=32);
    auto *target=reinterpret_cast<std::uint8_t*>(g_exe_base+rva);

    std::array<std::uint8_t,N> got{};
    if(!safe_copy(got.data(),target,N) || got!=expected)
        return false;

    void *tr=VirtualAlloc(
        nullptr,
        N+14,
        MEM_COMMIT|MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    if(!tr)
        return false;

    std::memcpy(tr,target,N);
    auto *tail=static_cast<std::uint8_t*>(tr)+N;
    tail[0]=0xFF;
    tail[1]=0x25;
    std::uint32_t zero=0;
    std::memcpy(tail+2,&zero,4);
    const std::uint64_t back=reinterpret_cast<std::uint64_t>(target+N);
    std::memcpy(tail+6,&back,8);
    FlushInstructionCache(GetCurrentProcess(),tr,N+14);

    h.target=target;
    h.trampoline=tr;
    h.stolen=N;
    std::copy(got.begin(),got.end(),h.original.begin());

    if(rva==RVA_SELECTOR)
        g_selector_trampoline=tr;

    if(!write_abs_jump(target,detour,N)) {
        if(rva==RVA_SELECTOR)
            g_selector_trampoline=nullptr;
        VirtualFree(tr,0,MEM_RELEASE);
        h={};
        return false;
    }
    return true;
}

void restore_hooks() noexcept {
    for(auto &h:g_hooks) {
        if(!h.target)
            continue;
        DWORD old=0;
        if(VirtualProtect(h.target,h.stolen,PAGE_EXECUTE_READWRITE,&old)) {
            std::memcpy(h.target,h.original.data(),h.stolen);
            FlushInstructionCache(GetCurrentProcess(),h.target,h.stolen);
            DWORD dummy=0;
            VirtualProtect(h.target,h.stolen,old,&dummy);
        }
        if(h.trampoline)
            VirtualFree(h.trampoline,0,MEM_RELEASE);
        h={};
    }
    g_selector_trampoline=nullptr;
}

bool install_producer_hooks() {
    if(!install_hook(
        g_hooks[0],RVA_WRAPPER_TYPE5,BYTES_WRAPPER,
        reinterpret_cast<void*>(&hook_wrapper5)))
        return false;
    g_wrapper5_orig=reinterpret_cast<wrapper_fn>(g_hooks[0].trampoline);

    if(!install_hook(
        g_hooks[1],RVA_WRAPPER_TYPE6,BYTES_WRAPPER,
        reinterpret_cast<void*>(&hook_wrapper6)))
        return false;
    g_wrapper6_orig=reinterpret_cast<wrapper_fn>(g_hooks[1].trampoline);

    if(!install_hook(
        g_hooks[2],RVA_BLEND_HELPER,BYTES_BLEND,
        reinterpret_cast<void*>(&hook_blend)))
        return false;
    g_blend_orig=reinterpret_cast<blend_fn>(g_hooks[2].trampoline);

    if(!install_hook(
        g_hooks[3],RVA_SINGLE_HELPER,BYTES_SINGLE,
        reinterpret_cast<void*>(&hook_single)))
        return false;
    g_single_orig=reinterpret_cast<single_fn>(g_hooks[3].trampoline);

    if(!install_hook(
        g_hooks[4],RVA_SELECTOR,BYTES_SELECTOR,
        reinterpret_cast<void*>(&selector_hook_entry)))
        return false;
    g_selector_trampoline=g_hooks[4].trampoline;
    return true;
}

struct cb_capture {
    ID3D11Buffer *base=nullptr;
    ID3D11Buffer *window=nullptr;
    UINT first=0;
    UINT num=0;
    bool explicit_window=false;
    bool coherent=true;
};

cb_capture capture_b13(
    ID3D11DeviceContext *ctx,
    ID3D11DeviceContext1 *ctx1)
{
    cb_capture c{};
    ctx->PSGetConstantBuffers(13,1,&c.base);
    if(ctx1) {
        ctx1->PSGetConstantBuffers1(
            13,1,&c.window,&c.first,&c.num);
        c.coherent=(c.base==c.window);
        c.explicit_window=
            c.window &&
            c.num>=16 &&
            (c.first%16)==0 &&
            (c.num%16)==0;
    }
    return c;
}

void release_capture(cb_capture &c) noexcept {
    if(c.base) c.base->Release();
    if(c.window) c.window->Release();
    c={};
}

void restore_b13(
    ID3D11DeviceContext *ctx,
    ID3D11DeviceContext1 *ctx1,
    const cb_capture &c)
{
    if(ctx1 && c.explicit_window) {
        ID3D11Buffer *b=c.window;
        UINT f=c.first,n=c.num;
        ctx1->PSSetConstantBuffers1(13,1,&b,&f,&n);
    } else {
        ID3D11Buffer *b=c.base;
        ctx->PSSetConstantBuffers(13,1,&b);
    }
}

bool verify_b13_restore(
    ID3D11DeviceContext *ctx,
    ID3D11DeviceContext1 *ctx1,
    const cb_capture &c)
{
    ID3D11Buffer *base=nullptr;
    ctx->PSGetConstantBuffers(13,1,&base);
    bool ok=(base==c.base);
    if(base) base->Release();

    if(ctx1) {
        ID3D11Buffer *win=nullptr;
        UINT first=0,num=0;
        ctx1->PSGetConstantBuffers1(
            13,1,&win,&first,&num);
        ok=
            ok &&
            win==c.window &&
            (!c.explicit_window ||
             (first==c.first && num==c.num));
        if(win) win->Release();
    }
    return ok;
}

ID3D11Buffer *realize_b13(
    const std::shared_ptr<const snapshot> &s,
    ID3D11Device *device)
{
    if(!s || !device)
        return nullptr;

    std::lock_guard<std::mutex> lock(s->gpu_mutex);
    if(s->buffer) {
        if(s->device!=device)
            return nullptr;
        s->buffer->AddRef();
        return s->buffer;
    }

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth=128;
    desc.Usage=D3D11_USAGE_IMMUTABLE;
    desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem=s->payload.data();

    ID3D11Buffer *buffer=nullptr;
    if(FAILED(device->CreateBuffer(&desc,&init,&buffer)) || !buffer)
        return nullptr;

    s->device=device;
    device->AddRef();
    s->buffer=buffer;
    buffer->AddRef();
    return buffer;
}

void clear_token(draw_token &token) noexcept {
    token={};
}

} // namespace

extern "C" void selector_observer(
    void *owner,
    void *ret,
    void *r14,
    void *r15)
{
    const auto caller=
        reinterpret_cast<std::uintptr_t>(ret)-g_exe_base;
    const std::uint8_t *desc=nullptr;

    if(caller==RET_SEL_1 || caller==RET_SEL_3)
        desc=static_cast<const std::uint8_t*>(r15);
    else if(caller==RET_SEL_2)
        desc=static_cast<const std::uint8_t*>(r14);
    else {
        g_draw_snapshot.reset();
        return;
    }

    if(!owner || !desc) {
        g_draw_snapshot.reset();
        return;
    }

    std::uint16_t a=0,b=0;
    std::uint32_t beta=0;
    if(!safe_read(desc+0x4C,a) ||
       !safe_read(desc+0x4E,b) ||
       !safe_read(desc+0x50,beta)) {
        g_draw_snapshot.reset();
        return;
    }

    std::shared_ptr<const snapshot> s;
    {
        std::lock_guard<std::mutex> lock(g_snapshot_mutex);
        const auto it=g_snapshots.find(
            reinterpret_cast<std::uintptr_t>(owner));
        if(it!=g_snapshots.end())
            s=it->second;
    }

    if(!s ||
       s->a!=a ||
       s->b!=b ||
       s->beta_bits!=beta) {
        g_draw_snapshot.reset();
        return;
    }

    g_selector_matches.fetch_add(1,std::memory_order_relaxed);
    g_draw_snapshot=std::move(s);
}

bool initialize_producer(std::uintptr_t exe_base) noexcept {
    if(!exe_base || g_exe_base)
        return false;

    g_exe_base=exe_base;
    g_quarantined.store(false,std::memory_order_release);

    if(!install_producer_hooks()) {
        restore_hooks();
        g_exe_base=0;
        return false;
    }
    return true;
}

void shutdown_producer() noexcept {
    restore_hooks();
    {
        std::lock_guard<std::mutex> lock(g_snapshot_mutex);
        g_snapshots.clear();
    }
    g_draw_snapshot.reset();
    g_exe_base=0;
}

bool snapshot_ready() noexcept {
    return !g_quarantined.load(std::memory_order_acquire) &&
           static_cast<bool>(g_draw_snapshot);
}

bool prepare_draw(
    ID3D11DeviceContext *ctx,
    ID3D11PixelShader *replacement_ps,
    draw_token &token) noexcept
{
    clear_token(token);

    if(g_quarantined.load(std::memory_order_acquire) ||
       !ctx ||
       !replacement_ps ||
       !g_draw_snapshot) {
        g_prepare_failopen.fetch_add(1,std::memory_order_relaxed);
        g_draw_snapshot.reset();
        return false;
    }

    ID3D11Device *device=nullptr;
    ctx->GetDevice(&device);
    if(!device) {
        g_prepare_failopen.fetch_add(1,std::memory_order_relaxed);
        g_draw_snapshot.reset();
        return false;
    }

    ID3D11Buffer *b13=realize_b13(g_draw_snapshot,device);
    device->Release();
    if(!b13) {
        g_prepare_failopen.fetch_add(1,std::memory_order_relaxed);
        g_draw_snapshot.reset();
        return false;
    }

    ID3D11DeviceContext1 *ctx1=nullptr;
    ctx->QueryInterface(
        __uuidof(ID3D11DeviceContext1),
        reinterpret_cast<void**>(&ctx1));

    cb_capture old=capture_b13(ctx,ctx1);
    if(!old.coherent) {
        release_capture(old);
        if(ctx1) ctx1->Release();
        b13->Release();
        g_prepare_failopen.fetch_add(1,std::memory_order_relaxed);
        g_draw_snapshot.reset();
        return false;
    }

    ID3D11PixelShader *old_ps=nullptr;
    ctx->PSGetShader(&old_ps,nullptr,nullptr);

    replacement_ps->AddRef();
    ctx->PSSetShader(replacement_ps,nullptr,0);

    ID3D11Buffer *owned=b13;
    ctx->PSSetConstantBuffers(13,1,&owned);

    token.ctx=ctx;
    token.ctx1=ctx1;
    token.old_ps=old_ps;
    token.replacement_ps=replacement_ps;
    token.old_b13_base=old.base;
    token.old_b13_window=old.window;
    token.old_first=old.first;
    token.old_num=old.num;
    token.explicit_window=old.explicit_window;
    token.injected_b13=b13;
    token.active=true;

    // Ownership of old capture refs moves into the token.
    old.base=nullptr;
    old.window=nullptr;

    g_prepare_pass.fetch_add(1,std::memory_order_relaxed);
    return true;
}

void finish_draw(draw_token &token) noexcept {
    if(!token.active || !token.ctx) {
        g_draw_snapshot.reset();
        clear_token(token);
        return;
    }

    cb_capture old{};
    old.base=token.old_b13_base;
    old.window=token.old_b13_window;
    old.first=token.old_first;
    old.num=token.old_num;
    old.explicit_window=token.explicit_window;
    old.coherent=true;

    restore_b13(token.ctx,token.ctx1,old);
    token.ctx->PSSetShader(token.old_ps,nullptr,0);

    if(!verify_b13_restore(token.ctx,token.ctx1,old)) {
        g_restore_fail.fetch_add(1,std::memory_order_relaxed);
        g_quarantined.store(true,std::memory_order_release);
    }

    if(token.old_ps) token.old_ps->Release();
    release_capture(old);
    if(token.ctx1) token.ctx1->Release();
    if(token.injected_b13) token.injected_b13->Release();
    if(token.replacement_ps) token.replacement_ps->Release();

    g_draw_snapshot.reset();
    clear_token(token);
}

bool quarantined() noexcept {
    return g_quarantined.load(std::memory_order_acquire);
}
std::uint64_t snapshots_published() noexcept {
    return g_snapshots_published.load(std::memory_order_relaxed);
}
std::uint64_t selector_matches() noexcept {
    return g_selector_matches.load(std::memory_order_relaxed);
}
std::uint64_t prepare_passes() noexcept {
    return g_prepare_pass.load(std::memory_order_relaxed);
}
std::uint64_t prepare_failopens() noexcept {
    return g_prepare_failopen.load(std::memory_order_relaxed);
}
std::uint64_t restore_failures() noexcept {
    return g_restore_fail.load(std::memory_order_relaxed);
}

} // namespace dsrrl::a3::ul
