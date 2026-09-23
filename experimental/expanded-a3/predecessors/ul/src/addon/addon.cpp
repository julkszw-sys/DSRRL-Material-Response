#include <reshade.hpp>
#include "dsrrl/generated_port_plans.hpp"
#include "dsrrl/generated_ul48_live.hpp"
#include "dsrrl/sha256.hpp"
#include "dsrrl/dxbc_checksum.hpp"

#if RESHADE_API_VERSION != 20
#error DSRRL Real PTDE U/L requires ReShade Add-on API 20
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11_1.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

using namespace reshade::api;

extern "C" void selector_hook_entry();
extern "C" { void *g_selector_trampoline = nullptr; }

namespace {
constexpr std::string_view kExeSha256 = "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";
constexpr std::string_view kBinderSha256 = "ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394";
constexpr std::string_view kFilterBinderSha256 = "4cdc36209f6e1ebff5132a3e9226eff10470121e858dc197d7daef02702d4650";
constexpr std::string_view kArtifact267Sha256 = "2c99a74b45cbc9627a9d3181063cfa6138ca1b6668a7a923d02aa3aa40c569fb";
constexpr std::uint16_t kEnabledMask = dsrrl::p1plan::OP_SAT | dsrrl::p1plan::OP_DIFFUSE_RAW | dsrrl::p1plan::OP_PNTS_LINEAR | dsrrl::p1plan::OP_GST_FIXED_SAT | dsrrl::p1plan::OP_NOSPC_ENVSPEC_OFF | dsrrl::p1plan::OP_FIXED_PHN_DIFFUSE_RAW | dsrrl::p1plan::OP_FIXED_PHN_POSTFOG_IDENTITY | dsrrl::p1plan::OP_PHN_EXTRA_SAT | dsrrl::p1plan::OP_RELAXED_PHN_EXT_SAT | dsrrl::p1plan::OP_RELAXED_PHN_EXT_DIFFUSE_RAW | dsrrl::p1plan::OP_RELAXED_GST_SFX_BROAD_SAT | dsrrl::p1plan::OP_RELAXED_GST_ALPHA_HEMENV_SAT | dsrrl::p1plan::OP_RGBA_TERMINAL_SAT;
static_assert((kEnabledMask & dsrrl::p1plan::OP_WORKFLOW_RAW) == 0);
static_assert(dsrrl::ul48live::k_plans.size() == 48);

constexpr std::uintptr_t RVA_WRAPPER_TYPE5 = 0x1C0BE0;
constexpr std::uintptr_t RVA_WRAPPER_TYPE6 = 0x1C0C10;
constexpr std::uintptr_t RVA_BLEND_HELPER  = 0x5642F0;
constexpr std::uintptr_t RVA_SINGLE_HELPER = 0x564510;
constexpr std::uintptr_t RVA_SELECTOR      = 0x22BA20;
constexpr std::uintptr_t RET_SINGLE_UPPER  = 0x563642;
constexpr std::uintptr_t RET_SINGLE_LOWER  = 0x563659;
constexpr std::uintptr_t RET_BLEND_UPPER   = 0x5639BA;
constexpr std::uintptr_t RET_BLEND_LOWER   = 0x5639D5;
constexpr std::uintptr_t RET_SEL_1         = 0x20E019;
constexpr std::uintptr_t RET_SEL_2         = 0x20EB7F;
constexpr std::uintptr_t RET_SEL_3         = 0x20FB9E;

constexpr std::array<std::uint8_t,17> BYTES_WRAPPER = {0x48,0x83,0xEC,0x38,0x4D,0x8B,0xC8,0xF3,0x0F,0x11,0x5C,0x24,0x20,0x4C,0x8B,0x41,0x40};
constexpr std::array<std::uint8_t,19> BYTES_BLEND = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x81,0xEC,0xC0,0x00,0x00,0x00};
constexpr std::array<std::uint8_t,14> BYTES_SINGLE = {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x40,0x0F,0xBF,0x42,0x06};
constexpr std::array<std::uint8_t,15> BYTES_SELECTOR = {0x40,0x53,0x48,0x83,0xEC,0x30,0x49,0x63,0xC0,0x45,0x8B,0xD1,0x48,0x8B,0xDA};

void log_info(const std::string &s) { reshade::log::message(reshade::log::level::info, s.c_str()); }
void log_warn(const std::string &s) { reshade::log::message(reshade::log::level::warning, s.c_str()); }
void log_error(const std::string &s) { reshade::log::message(reshade::log::level::error, s.c_str()); }

std::uintptr_t g_exe_base = 0;
std::atomic<bool> g_provenance_ok{false};
std::atomic<bool> g_quarantined{false};
std::atomic<std::uint64_t> g_present_count{0};
std::atomic<std::uint64_t> g_create_events{0},g_intercepts{0},g_attested{0},g_mismatch{0},g_failopen{0};
std::atomic<std::uint64_t> g_hook_wrapper5{0},g_hook_wrapper6{0},g_single_upper{0},g_single_lower{0},g_blend_upper{0},g_blend_lower{0},g_snapshots_published{0};
std::atomic<std::uint64_t> g_selector_calls{0},g_selector_known{0},g_selector_match{0},g_selector_no_snapshot{0},g_selector_tuple_mismatch{0},g_selector_unknown{0};
std::atomic<std::uint64_t> g_ul48_create_pass{0},g_ul48_create_fail{0},g_target_binds{0},g_draw_eligible{0},g_replay{0},g_replay_indexed{0},g_replay_instanced{0},g_b13_create_fail{0},g_restore_fail{0};

std::filesystem::path process_path() {
    std::wstring b(32768,L'\0'); const DWORD n=GetModuleFileNameW(nullptr,b.data(),static_cast<DWORD>(b.size()));
    if(n==0 || n>=b.size()) return {};
    b.resize(n);
    return b;
}
std::filesystem::path binder_path(){ const auto p=process_path(); return p.empty()?std::filesystem::path{}:p.parent_path()/L"shader"/L"FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx"; }
std::filesystem::path filter_binder_path(){ const auto p=process_path(); return p.empty()?std::filesystem::path{}:p.parent_path()/L"shader"/L"FRPG_Filter_DX11.shaderbnd.dcx"; }

bool verify_provenance(){
    try{
        const auto ep=process_path(); if(ep.empty()) return false;
        const auto eh=dsrrl::to_hex(dsrrl::sha256_file(ep)); if(eh!=kExeSha256){log_error("DSRRL REAL U/L: EXE SHA mismatch; disabled.");return false;}
        const auto bp=binder_path(); if(bp.empty()||!std::filesystem::exists(bp)) return false;
        if(dsrrl::to_hex(dsrrl::sha256_file(bp))!=kBinderSha256){log_error("DSRRL REAL U/L: FlverPBL binder SHA mismatch; disabled.");return false;}
        const auto fp=filter_binder_path(); if(fp.empty()||!std::filesystem::exists(fp)) return false;
        if(dsrrl::to_hex(dsrrl::sha256_file(fp))!=kFilterBinderSha256){log_error("DSRRL REAL U/L: Filter binder SHA mismatch; disabled.");return false;}
        log_info("DSRRL REAL U/L: exact EXE + vanilla FlverPBL + vanilla Filter provenance PASS."); return true;
    }catch(...){log_error("DSRRL REAL U/L: provenance exception; disabled.");return false;}
}

bool safe_copy(void *dst,const void *src,std::size_t n) noexcept {
#if defined(_MSC_VER)
    __try { std::memcpy(dst,src,n); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
#else
    if(!dst||!src) return false;
    std::memcpy(dst,src,n);
    return true;
#endif
}
template<class T> bool safe_read(const void *p,T &out) noexcept { return p && safe_copy(&out,p,sizeof(T)); }

#if defined(_MSC_VER)
#define DSRRL_RETURN_ADDRESS() _ReturnAddress()
#else
#define DSRRL_RETURN_ADDRESS() __builtin_return_address(0)
#endif

#pragma pack(push,1)
struct raw_rgbm { std::int16_t r,g,b,m; };
#pragma pack(pop)
struct f4 { float x=0,y=0,z=0,w=0; };
f4 decode_rgbm(const raw_rgbm &v) noexcept {
    const float s=static_cast<float>(v.m)/100.0f;
    return {static_cast<float>(v.r)/255.0f*s,static_cast<float>(v.g)/255.0f*s,static_cast<float>(v.b)/255.0f*s,0.0f};
}
f4 lerp4(const f4&a,const f4&b,float t) noexcept { return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,0.0f}; }

struct snapshot {
    std::uintptr_t owner=0; std::uint16_t a=0,b=0; std::uint32_t beta_bits=0;
    alignas(16) std::array<f4,8> payload{};
    mutable std::mutex gpu_mutex; mutable ID3D11Device *device=nullptr; mutable ID3D11Buffer *buffer=nullptr;
    ~snapshot(){ if(buffer) buffer->Release(); if(device) device->Release(); }
};
std::mutex g_snapshot_mutex;
std::unordered_map<std::uintptr_t,std::shared_ptr<const snapshot>> g_snapshots;
thread_local std::shared_ptr<const snapshot> g_draw_snapshot;

struct producer_tls {
    bool active=false; std::uintptr_t owner=0; const std::uint8_t *assignment=nullptr;
    bool have_upper=false,have_lower=false; f4 upper{},lower{};
};
thread_local producer_tls g_prod;

using wrapper_fn = void *(__fastcall *)(void *,void *,void *,float);
using blend_fn = void *(__fastcall *)(void *,const raw_rgbm *,const raw_rgbm *,float);
using single_fn = void *(__fastcall *)(void *,const raw_rgbm *,float);
wrapper_fn g_wrapper5_orig=nullptr,g_wrapper6_orig=nullptr;
blend_fn g_blend_orig=nullptr;
single_fn g_single_orig=nullptr;

void publish_snapshot(const producer_tls &p){
    if(!p.active||!p.owner||!p.assignment||!p.have_upper||!p.have_lower) return;
    std::uint16_t a=0,b=0; std::uint32_t beta=0;
    if(!safe_read(p.assignment+8,a)||!safe_read(p.assignment+10,b)||!safe_read(p.assignment+12,beta)) return;
    auto s=std::make_shared<snapshot>(); s->owner=p.owner;s->a=a;s->b=b;s->beta_bits=beta;s->payload[6]=p.upper;s->payload[7]=p.lower;
    {std::lock_guard<std::mutex> lock(g_snapshot_mutex);g_snapshots[p.owner]=s;}
    g_snapshots_published.fetch_add(1,std::memory_order_relaxed);
}

void *run_wrapper(wrapper_fn orig,std::atomic<std::uint64_t>&counter,void *rcx,void *owner,void *assignment,float x){
    counter.fetch_add(1,std::memory_order_relaxed); const auto prev=g_prod;
    g_prod={true,reinterpret_cast<std::uintptr_t>(owner),static_cast<const std::uint8_t*>(assignment),false,false,{},{}};
    void *result=orig(rcx,owner,assignment,x); const auto done=g_prod; g_prod=prev; publish_snapshot(done);
    return result;
}
void *__fastcall hook_wrapper5(void *a,void *owner,void *assignment,float x){return run_wrapper(g_wrapper5_orig,g_hook_wrapper5,a,owner,assignment,x);}
void *__fastcall hook_wrapper6(void *a,void *owner,void *assignment,float x){return run_wrapper(g_wrapper6_orig,g_hook_wrapper6,a,owner,assignment,x);}

void *__fastcall hook_single(void *dst,const raw_rgbm *v,float w){
    if(g_prod.active && v){ raw_rgbm raw{}; if(safe_read(v,raw)){
        const auto r=reinterpret_cast<std::uintptr_t>(DSRRL_RETURN_ADDRESS())-g_exe_base; if(r==RET_SINGLE_UPPER){g_prod.upper=decode_rgbm(raw);g_prod.have_upper=true;g_single_upper.fetch_add(1);} else if(r==RET_SINGLE_LOWER){g_prod.lower=decode_rgbm(raw);g_prod.have_lower=true;g_single_lower.fetch_add(1);}
    }} return g_single_orig(dst,v,w);
}
void *__fastcall hook_blend(void *dst,const raw_rgbm *a,const raw_rgbm *b,float beta){
    if(g_prod.active && a && b){ raw_rgbm ra{},rb{}; if(safe_read(a,ra)&&safe_read(b,rb)){
        const f4 v=lerp4(decode_rgbm(ra),decode_rgbm(rb),beta); const auto r=reinterpret_cast<std::uintptr_t>(DSRRL_RETURN_ADDRESS())-g_exe_base;
        if(r==RET_BLEND_UPPER){g_prod.upper=v;g_prod.have_upper=true;g_blend_upper.fetch_add(1);} else if(r==RET_BLEND_LOWER){g_prod.lower=v;g_prod.have_lower=true;g_blend_lower.fetch_add(1);}
    }} return g_blend_orig(dst,a,b,beta);
}

struct inline_hook { void *target=nullptr; void *trampoline=nullptr; std::size_t stolen=0; std::array<std::uint8_t,32> original{}; };
std::array<inline_hook,5> g_hooks{};

bool write_abs_jump(void *at,void *to,std::size_t span){
    if(!at||!to||span<14||span>32) return false;
    std::array<std::uint8_t,32> p{};
    p.fill(0x90); p[0]=0xFF;p[1]=0x25;
    const std::uint64_t q=reinterpret_cast<std::uint64_t>(to); std::memcpy(p.data()+6,&q,8);
    DWORD old=0;if(!VirtualProtect(at,span,PAGE_EXECUTE_READWRITE,&old))return false;std::memcpy(at,p.data(),span);FlushInstructionCache(GetCurrentProcess(),at,span);DWORD dummy=0;VirtualProtect(at,span,old,&dummy);return true;
}
template<std::size_t N> bool install_hook(inline_hook &h,std::uintptr_t rva,const std::array<std::uint8_t,N>&expected,void *detour){
    static_assert(N>=14 && N<=32); auto *target=reinterpret_cast<std::uint8_t*>(g_exe_base+rva); std::array<std::uint8_t,N> got{}; if(!safe_copy(got.data(),target,N)||got!=expected)return false;
    void *tr=VirtualAlloc(nullptr,N+14,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tr)return false;std::memcpy(tr,target,N);
    auto *tail=static_cast<std::uint8_t*>(tr)+N;tail[0]=0xFF;tail[1]=0x25;std::uint32_t z=0;std::memcpy(tail+2,&z,4);const std::uint64_t back=reinterpret_cast<std::uint64_t>(target+N);std::memcpy(tail+6,&back,8);FlushInstructionCache(GetCurrentProcess(),tr,N+14);
    h.target=target;h.trampoline=tr;h.stolen=N;std::copy(got.begin(),got.end(),h.original.begin());
    if(rva==RVA_SELECTOR) g_selector_trampoline=tr;
    if(!write_abs_jump(target,detour,N)){if(rva==RVA_SELECTOR) g_selector_trampoline=nullptr;VirtualFree(tr,0,MEM_RELEASE);h={};return false;}
    return true;
}
void restore_hooks(){ for(auto &h:g_hooks){if(!h.target)continue;DWORD old=0;if(VirtualProtect(h.target,h.stolen,PAGE_EXECUTE_READWRITE,&old)){std::memcpy(h.target,h.original.data(),h.stolen);FlushInstructionCache(GetCurrentProcess(),h.target,h.stolen);DWORD d=0;VirtualProtect(h.target,h.stolen,old,&d);}if(h.trampoline)VirtualFree(h.trampoline,0,MEM_RELEASE);h={};}g_selector_trampoline=nullptr; }

bool install_producer_hooks(){
    if(!install_hook(g_hooks[0],RVA_WRAPPER_TYPE5,BYTES_WRAPPER,reinterpret_cast<void*>(&hook_wrapper5))) return false;
    g_wrapper5_orig=reinterpret_cast<wrapper_fn>(g_hooks[0].trampoline);
    if(!install_hook(g_hooks[1],RVA_WRAPPER_TYPE6,BYTES_WRAPPER,reinterpret_cast<void*>(&hook_wrapper6))) return false;
    g_wrapper6_orig=reinterpret_cast<wrapper_fn>(g_hooks[1].trampoline);
    if(!install_hook(g_hooks[2],RVA_BLEND_HELPER,BYTES_BLEND,reinterpret_cast<void*>(&hook_blend))) return false;
    g_blend_orig=reinterpret_cast<blend_fn>(g_hooks[2].trampoline);
    if(!install_hook(g_hooks[3],RVA_SINGLE_HELPER,BYTES_SINGLE,reinterpret_cast<void*>(&hook_single))) return false;
    g_single_orig=reinterpret_cast<single_fn>(g_hooks[3].trampoline);
    if(!install_hook(g_hooks[4],RVA_SELECTOR,BYTES_SELECTOR,reinterpret_cast<void*>(&selector_hook_entry))) return false;
    g_selector_trampoline=g_hooks[4].trampoline;
    return true;
}

extern "C" void selector_observer_impl(void *owner,void *ret,void *r14,void *r15){
    g_selector_calls.fetch_add(1,std::memory_order_relaxed);const auto rr=reinterpret_cast<std::uintptr_t>(ret)-g_exe_base;const std::uint8_t *desc=nullptr;
    if(rr==RET_SEL_1||rr==RET_SEL_3)desc=static_cast<const std::uint8_t*>(r15);else if(rr==RET_SEL_2)desc=static_cast<const std::uint8_t*>(r14);else{g_selector_unknown.fetch_add(1);g_draw_snapshot.reset();return;}g_selector_known.fetch_add(1);
    if(!owner||!desc){g_draw_snapshot.reset();return;}std::uint16_t a=0,b=0;std::uint32_t beta=0;if(!safe_read(desc+0x4C,a)||!safe_read(desc+0x4E,b)||!safe_read(desc+0x50,beta)){g_draw_snapshot.reset();return;}
    std::shared_ptr<const snapshot> s;{std::lock_guard<std::mutex> lock(g_snapshot_mutex);auto it=g_snapshots.find(reinterpret_cast<std::uintptr_t>(owner));if(it!=g_snapshots.end())s=it->second;}
    if(!s){g_selector_no_snapshot.fetch_add(1);g_draw_snapshot.reset();return;}if(s->a!=a||s->b!=b||s->beta_bits!=beta){g_selector_tuple_mismatch.fetch_add(1);g_draw_snapshot.reset();return;}g_selector_match.fetch_add(1);g_draw_snapshot=std::move(s);
}

struct pending_tx { const shader_desc *descriptor=nullptr;std::size_t plan_index=0;std::size_t expected_size=0;std::string_view expected_sha{};int ul_host=-1; };
std::array<std::shared_ptr<const std::vector<std::uint8_t>>,dsrrl::p1plan::k_plan_count> g_cache{};
std::mutex g_cache_mutex;thread_local std::vector<pending_tx> g_pending;
std::mutex g_pipeline_mutex;std::unordered_map<std::uint64_t,int> g_ul_pipelines;
thread_local int g_bound_ul_host=-1;thread_local const command_list *g_bound_cmd=nullptr;

struct device_ul_state { ID3D11Device *device=nullptr;std::array<ID3D11PixelShader*,48> ps{}; } g_dev;
std::mutex g_dev_mutex;
void release_device_state(){std::lock_guard<std::mutex> lock(g_dev_mutex);for(auto *&p:g_dev.ps){if(p){p->Release();p=nullptr;}}if(g_dev.device){g_dev.device->Release();g_dev.device=nullptr;}}

shader_desc *find_mutable_ps(std::uint32_t n,const pipeline_subobject *sub) noexcept {if(!sub)return nullptr;for(std::uint32_t i=0;i<n;++i)if(sub[i].type==pipeline_subobject_type::pixel_shader&&sub[i].count==1&&sub[i].data)return const_cast<shader_desc*>(static_cast<const shader_desc*>(sub[i].data));return nullptr;}
const shader_desc *find_ps(std::uint32_t n,const pipeline_subobject *sub) noexcept {if(!sub)return nullptr;for(std::uint32_t i=0;i<n;++i)if(sub[i].type==pipeline_subobject_type::pixel_shader&&sub[i].count==1&&sub[i].data)return static_cast<const shader_desc*>(sub[i].data);return nullptr;}
void evict_stale(const shader_desc *d){g_pending.erase(std::remove_if(g_pending.begin(),g_pending.end(),[d](const pending_tx&x){return x.descriptor==d;}),g_pending.end());}
pending_tx *find_pending(const shader_desc*d){for(auto it=g_pending.rbegin();it!=g_pending.rend();++it)if(it->descriptor==d)return &*it;return nullptr;}
void erase_pending(const shader_desc*d){for(auto it=g_pending.end();it!=g_pending.begin();){--it;if(it->descriptor==d){g_pending.erase(it);return;}}}
std::shared_ptr<const std::vector<std::uint8_t>> build_p22(const dsrrl::p1plan::plan &p,const shader_desc&ps){if(ps.code_size!=p.code_size||!ps.code)return{};auto v=std::make_shared<std::vector<std::uint8_t>>(ps.code_size);std::memcpy(v->data(),ps.code,ps.code_size);for(std::size_t i=0;i<p.op_count;++i){const auto&o=p.ops[i];if(o.byte_offset+4>v->size())return{};std::uint32_t cur=0;std::memcpy(&cur,v->data()+o.byte_offset,4);if(cur!=o.old_word)return{};std::memcpy(v->data()+o.byte_offset,&o.new_word,4);}if(!dsrrl::fix_dxbc_checksum(*v))return{};if(dsrrl::to_hex(dsrrl::sha256({reinterpret_cast<const std::byte*>(v->data()),v->size()}))!=p.replacement_sha256)return{};return v;}

bool on_create_pipeline(device *d,pipeline_layout,std::uint32_t n,const pipeline_subobject *sub){
    g_create_events.fetch_add(1);if(g_quarantined.load()||!g_provenance_ok.load()||!d||d->get_api()!=device_api::d3d11)return false;auto *ps=find_mutable_ps(n,sub);if(!ps||!ps->code||!dsrrl::p1plan::candidate_size(ps->code_size))return false;evict_stale(ps);
    const auto h=dsrrl::to_hex(dsrrl::sha256({reinterpret_cast<const std::byte*>(ps->code),ps->code_size}));const auto *p=dsrrl::p1plan::find(ps->code_size,h);if(!p)return false;if((p->mask&kEnabledMask)!=p->mask){g_failopen.fetch_add(1);return false;}const std::size_t idx=static_cast<std::size_t>(p-dsrrl::p1plan::k_plans.data());std::shared_ptr<const std::vector<std::uint8_t>> stable;{std::lock_guard<std::mutex> lock(g_cache_mutex);stable=g_cache[idx];if(!stable){stable=build_p22(*p,*ps);if(stable)g_cache[idx]=stable;}}if(!stable){g_failopen.fetch_add(1);return false;}
    const int ul=dsrrl::ul48live::find_original(h);ps->code=stable->data();ps->code_size=stable->size();g_pending.push_back({ps,idx,stable->size(),p->replacement_sha256,ul});g_intercepts.fetch_add(1);return true;
}
void on_init_pipeline(device *d,pipeline_layout,std::uint32_t n,const pipeline_subobject *sub,pipeline p){if(!d||d->get_api()!=device_api::d3d11||!p.handle)return;const auto *ps=find_ps(n,sub);if(!ps||!ps->code)return;auto *tx=find_pending(ps);if(!tx)return;bool ok=false;if(ps->code_size==tx->expected_size){const auto h=dsrrl::to_hex(dsrrl::sha256({reinterpret_cast<const std::byte*>(ps->code),ps->code_size}));ok=h==tx->expected_sha;}if(ok){g_attested.fetch_add(1);if(tx->ul_host>=0){std::lock_guard<std::mutex> lock(g_pipeline_mutex);g_ul_pipelines[p.handle]=tx->ul_host;}}else{g_mismatch.fetch_add(1);g_quarantined.store(true);log_error("DSRRL REAL U/L: P2.2 create->init attestation mismatch; quarantined.");}erase_pending(ps);}
void on_bind_pipeline(command_list *cmd,pipeline_stage stages,pipeline p){if(!cmd||((static_cast<std::uint32_t>(stages)&static_cast<std::uint32_t>(pipeline_stage::pixel_shader))==0))return;int idx=-1;{std::lock_guard<std::mutex> lock(g_pipeline_mutex);auto it=g_ul_pipelines.find(p.handle);if(it!=g_ul_pipelines.end())idx=it->second;}g_bound_ul_host=idx;g_bound_cmd=idx>=0?cmd:nullptr;if(idx>=0)g_target_binds.fetch_add(1);else g_draw_snapshot.reset();}

struct cb_capture { ID3D11Buffer *base=nullptr,*win=nullptr;UINT first=0,num=0;bool explicit_window=false,coherent=true; };
cb_capture capture_cb(ID3D11DeviceContext *ctx,ID3D11DeviceContext1 *ctx1,UINT slot){cb_capture c{};ctx->PSGetConstantBuffers(slot,1,&c.base);if(ctx1){ctx1->PSGetConstantBuffers1(slot,1,&c.win,&c.first,&c.num);c.coherent=c.base==c.win;c.explicit_window=c.win&&c.num>=16&&(c.first%16)==0&&(c.num%16)==0;}return c;}
void release_cb(cb_capture&c){if(c.base)c.base->Release();if(c.win)c.win->Release();c={};}
void restore_cb(ID3D11DeviceContext*ctx,ID3D11DeviceContext1*ctx1,UINT slot,const cb_capture&c){if(ctx1&&c.explicit_window){ID3D11Buffer*b=c.win;UINT f=c.first,n=c.num;ctx1->PSSetConstantBuffers1(slot,1,&b,&f,&n);}else{ID3D11Buffer*b=c.base;ctx->PSSetConstantBuffers(slot,1,&b);}}
bool verify_restore(ID3D11DeviceContext*ctx,ID3D11DeviceContext1*ctx1,const cb_capture&c){ID3D11Buffer*b=nullptr;ctx->PSGetConstantBuffers(13,1,&b);bool ok=b==c.base;if(b)b->Release();if(ctx1){ID3D11Buffer*w=nullptr;UINT f=0,n=0;ctx1->PSGetConstantBuffers1(13,1,&w,&f,&n);ok=ok&&w==c.win&&(!c.explicit_window||(f==c.first&&n==c.num));if(w)w->Release();}return ok;}
ID3D11Buffer *realize_b13(const std::shared_ptr<const snapshot>&s,ID3D11Device*dev){if(!s||!dev)return nullptr;std::lock_guard<std::mutex> lock(s->gpu_mutex);if(s->buffer){if(s->device!=dev)return nullptr;s->buffer->AddRef();return s->buffer;}D3D11_BUFFER_DESC bd{};bd.ByteWidth=128;bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;D3D11_SUBRESOURCE_DATA init{};init.pSysMem=s->payload.data();ID3D11Buffer*b=nullptr;if(FAILED(dev->CreateBuffer(&bd,&init,&b))||!b)return nullptr;s->device=dev;dev->AddRef();s->buffer=b;b->AddRef();return b;}

bool on_draw_indexed(command_list *cmd,std::uint32_t index_count,std::uint32_t instance_count,std::uint32_t first_index,std::int32_t vertex_offset,std::uint32_t first_instance){
    if(g_quarantined.load()||g_bound_ul_host<0||!cmd||g_bound_cmd!=cmd||!g_draw_snapshot) return false;
    g_draw_eligible.fetch_add(1);
    if(first_index!=0||vertex_offset!=0||first_instance!=0){g_failopen.fetch_add(1);g_draw_snapshot.reset();return false;}
    std::uint32_t native_instances=0;if(!safe_read(reinterpret_cast<const void*>(g_draw_snapshot->owner+0x24BC),native_instances)){g_failopen.fetch_add(1);g_draw_snapshot.reset();return false;}
    const bool native_instanced=native_instances!=0;if((native_instanced&&instance_count!=native_instances)||(!native_instanced&&instance_count!=1)){g_failopen.fetch_add(1);g_draw_snapshot.reset();return false;}
    auto *ctx=reinterpret_cast<ID3D11DeviceContext*>(cmd->get_native());if(!ctx){g_draw_snapshot.reset();return false;}ID3D11Device*dev=nullptr;ctx->GetDevice(&dev);if(!dev){g_draw_snapshot.reset();return false;}
    ID3D11PixelShader *replacement=nullptr;{std::lock_guard<std::mutex> lock(g_dev_mutex);if(g_dev.device==dev&&g_bound_ul_host>=0&&g_bound_ul_host<48){replacement=g_dev.ps[static_cast<std::size_t>(g_bound_ul_host)];if(replacement)replacement->AddRef();}}
    if(!replacement){dev->Release();g_draw_snapshot.reset();return false;}ID3D11Buffer*b13=realize_b13(g_draw_snapshot,dev);dev->Release();if(!b13){g_b13_create_fail.fetch_add(1);replacement->Release();g_draw_snapshot.reset();return false;}
    ID3D11DeviceContext1*ctx1=nullptr;ctx->QueryInterface(__uuidof(ID3D11DeviceContext1),reinterpret_cast<void**>(&ctx1));cb_capture oldcb=capture_cb(ctx,ctx1,13);if(!oldcb.coherent){release_cb(oldcb);if(ctx1)ctx1->Release();b13->Release();replacement->Release();g_draw_snapshot.reset();return false;}
    ID3D11PixelShader*oldps=nullptr;ctx->PSGetShader(&oldps,nullptr,nullptr);ctx->PSSetShader(replacement,nullptr,0);ID3D11Buffer*owned=b13;ctx->PSSetConstantBuffers(13,1,&owned);
    if(native_instanced){ctx->DrawIndexedInstanced(index_count,native_instances,0,0,0);g_replay_instanced.fetch_add(1);}else{ctx->DrawIndexed(index_count,0,0);g_replay_indexed.fetch_add(1);}g_replay.fetch_add(1);
    restore_cb(ctx,ctx1,13,oldcb);ctx->PSSetShader(oldps,nullptr,0);const bool restored=verify_restore(ctx,ctx1,oldcb);if(!restored){g_restore_fail.fetch_add(1);g_quarantined.store(true);}
    if(oldps) oldps->Release();
    release_cb(oldcb);
    if(ctx1) ctx1->Release();
    b13->Release(); replacement->Release(); g_draw_snapshot.reset();
    return true;
}

void on_init_device(device*d){if(!d||d->get_api()!=device_api::d3d11)return;auto *native=reinterpret_cast<ID3D11Device*>(d->get_native());if(!native)return;std::lock_guard<std::mutex> lock(g_dev_mutex);if(g_dev.device)return;g_dev.device=native;native->AddRef();for(std::size_t i=0;i<48;++i){ID3D11PixelShader*p=nullptr;const auto&q=dsrrl::ul48live::k_plans[i];if(SUCCEEDED(native->CreatePixelShader(q.code,q.code_size,nullptr,&p))&&p){g_dev.ps[i]=p;g_ul48_create_pass.fetch_add(1);}else g_ul48_create_fail.fetch_add(1);}std::ostringstream os;os<<"DSRRL REAL U/L: native U/L replacement creation "<<g_ul48_create_pass.load()<<"/48, fail="<<g_ul48_create_fail.load();log_info(os.str());}

void emit_summary(const char*why,std::uint64_t present){std::ostringstream os;os<<"DSRRL REAL PTDE U/L summary: trigger="<<why<<" present="<<present<<" artifact267="<<kArtifact267Sha256<<" P22_STOCK_HDR=1 ENVSPEC=0 create="<<g_create_events.load()<<" intercepts="<<g_intercepts.load()<<" attested="<<g_attested.load()<<" mismatch="<<g_mismatch.load()<<" quarantined="<<(g_quarantined.load()?1:0)<<" UL48_CREATE_PASS="<<g_ul48_create_pass.load()<<" UL48_CREATE_FAIL="<<g_ul48_create_fail.load()<<" WRAP5="<<g_hook_wrapper5.load()<<" WRAP6="<<g_hook_wrapper6.load()<<" SINGLE_U="<<g_single_upper.load()<<" SINGLE_L="<<g_single_lower.load()<<" BLEND_U="<<g_blend_upper.load()<<" BLEND_L="<<g_blend_lower.load()<<" SNAPSHOTS="<<g_snapshots_published.load()<<" SELECTOR="<<g_selector_calls.load()<<" SELECTOR_KNOWN="<<g_selector_known.load()<<" SELECTOR_MATCH="<<g_selector_match.load()<<" NO_SNAPSHOT="<<g_selector_no_snapshot.load()<<" TUPLE_MISMATCH="<<g_selector_tuple_mismatch.load()<<" UNKNOWN_CALLER="<<g_selector_unknown.load()<<" TARGET_BINDS="<<g_target_binds.load()<<" ELIGIBLE="<<g_draw_eligible.load()<<" REPLAY="<<g_replay.load()<<" DRAWINDEXED="<<g_replay_indexed.load()<<" INSTANCED="<<g_replay_instanced.load()<<" B13_FAIL="<<g_b13_create_fail.load()<<" RESTORE_FAIL="<<g_restore_fail.load()<<" FAILOPEN="<<g_failopen.load();log_info(os.str());}
void on_present(command_queue*,swapchain*,const rect*,const rect*,std::uint32_t,const rect*){const auto n=g_present_count.fetch_add(1)+1;if(n==300||(n>300&&n%600==0))emit_summary("present",n);}
}

extern "C" void selector_observer(void *owner,void *ret,void *r14,void *r15){selector_observer_impl(owner,ret,r14,r15);}
extern "C" {
__declspec(dllexport) const char *NAME="DSRRL REAL PTDE U/L LIVE";
__declspec(dllexport) const char *AUTHOR="DSR Restored Lighting";
__declspec(dllexport) const char *DESCRIPTION="Real PTDE-authored Upper/Lower producer bridge: exact DSR-selected raw endpoint RGBM + A/B/beta -> PTDE linear U/L -> b13[6..7] -> 48 P2.2 HemEnv/HemEnvLerp replacement consumers. EnvSpec is excluded.";
}

extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module,HMODULE reshade_module){
    if(!reshade::register_addon(addon_module,reshade_module)) return false;
    g_exe_base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(!g_exe_base||!verify_provenance()){reshade::unregister_addon(addon_module,reshade_module);return false;}
    g_provenance_ok.store(true);
    if(!install_producer_hooks()){log_error("DSRRL REAL U/L: exact producer/selector hook preflight failed; restoring and disabling.");restore_hooks();reshade::unregister_addon(addon_module,reshade_module);return false;}
    log_info("DSRRL REAL U/L armed: real renderer-selected PTDE U/L endpoint capture, owner+A/B/beta freshness, exact draw-kind native replay; synthetic U/L and EnvSpec lanes are absent.");
    reshade::register_event<reshade::addon_event::init_device>(on_init_device);reshade::register_event<reshade::addon_event::create_pipeline>(on_create_pipeline);reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);reshade::register_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);reshade::register_event<reshade::addon_event::present>(on_present);return true;
}
extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module,HMODULE reshade_module){emit_summary("unload",g_present_count.load());restore_hooks();release_device_state();{std::lock_guard<std::mutex> lock(g_snapshot_mutex);g_snapshots.clear();}reshade::unregister_addon(addon_module,reshade_module);}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(h);return TRUE;}
