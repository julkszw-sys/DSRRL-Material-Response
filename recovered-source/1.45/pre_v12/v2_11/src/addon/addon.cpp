#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <reshade.hpp>
#include "dsrrl/generated_ptde_diffuse_material_response.hpp"
#include "dsrrl/ptde_material_donor_registry.hpp"
#include "dsrrl/generated_ptde_c101_material_response.hpp"
#include "dsrrl/sha256.hpp"
#if RESHADE_API_VERSION != 20
#error DSRRL PTDE Material Response V2.11 requires ReShade Add-on API 20
#endif
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
#include <unordered_set>
#include <vector>

using namespace reshade::api;
extern "C" void selector_hook_entry();
extern "C" { void *g_selector_trampoline = nullptr; }

namespace {
constexpr std::string_view kExeSha256 = "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";
constexpr std::string_view kBinderSha256 = "ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394";
constexpr std::uintptr_t RVA_SELECTOR = 0x22BA20;
constexpr std::uintptr_t RVA_MTD_PARSE_WRAPPER = 0x295ED0;
constexpr std::uintptr_t RET_SEL_1 = 0x20E019;
constexpr std::uintptr_t RET_SEL_2 = 0x20EB7F;
constexpr std::uintptr_t RET_SEL_3 = 0x20FB9E;
constexpr std::array<std::uint8_t,15> BYTES_SELECTOR = {0x40,0x53,0x48,0x83,0xEC,0x30,0x49,0x63,0xC0,0x45,0x8B,0xD1,0x48,0x8B,0xDA};
constexpr std::array<std::uint8_t,15> BYTES_MTD_PARSE_WRAPPER = {0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF};

void log_info(const std::string &s){reshade::log::message(reshade::log::level::info,s.c_str());}
void log_warn(const std::string &s){reshade::log::message(reshade::log::level::warning,s.c_str());}
void log_error(const std::string &s){reshade::log::message(reshade::log::level::error,s.c_str());}

std::uintptr_t g_exe_base=0;
std::atomic<bool> g_enabled{false},g_quarantined{false};
std::atomic<std::uint64_t> g_mtd_seen{0},g_mtd_donor_registered{0},g_mtd_donor_unmapped{0},g_mtd_tier0{0},g_mtd_tier1{0},g_mtd_tier2{0},g_selector_seen{0},g_selector_donor{0},g_target_binds{0},g_ptde_draws{0},g_failopen{0},g_lerp_bypass{0},g_b12_create{0},g_b12_hit{0},g_c101_exact_reg{0},g_c101_sibling_reg{0},g_c101_draws{0},g_c100_only_draws{0},g_present_seq{0};

std::filesystem::path process_path(){std::wstring b(32768,L'\0');const DWORD n=GetModuleFileNameW(nullptr,b.data(),static_cast<DWORD>(b.size()));if(n==0||n>=b.size())return{};b.resize(n);return b;}
std::filesystem::path binder_path(){auto p=process_path();return p.empty()?std::filesystem::path{}:p.parent_path()/L"shader"/L"FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx";}
bool verify_provenance(){try{auto ep=process_path();auto bp=binder_path();if(ep.empty()||bp.empty()||!std::filesystem::exists(bp))return false;if(dsrrl::to_hex(dsrrl::sha256_file(ep))!=kExeSha256){log_error("PTDE Material Response V2.11: EXE SHA mismatch; disabled.");return false;}if(dsrrl::to_hex(dsrrl::sha256_file(bp))!=kBinderSha256){log_error("PTDE Material Response V2.11: FlverPBL SHA mismatch; disabled.");return false;}return true;}catch(...){return false;}}

template<class T> bool safe_read(const void *src,T &out) noexcept {
#if defined(_MSC_VER)
    __try { std::memcpy(&out,src,sizeof(T)); return true; } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
#else
    if(!src) return false;
    std::memcpy(&out,src,sizeof(T));
    return true;
#endif
}
bool safe_copy(void *dst,const void *src,std::size_t n) noexcept {
#if defined(_MSC_VER)
    __try { std::memcpy(dst,src,n); return true; } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
#else
    if(!dst||!src) return false;
    std::memcpy(dst,src,n);
    return true;
#endif
}
bool write_bytes(void *at,const void *src,std::size_t n){DWORD old=0;if(!VirtualProtect(at,n,PAGE_EXECUTE_READWRITE,&old))return false;std::memcpy(at,src,n);const bool ok=FlushInstructionCache(GetCurrentProcess(),at,n)!=FALSE;DWORD dummy=0;VirtualProtect(at,n,old,&dummy);return ok;}

struct hook {void *target=nullptr,*trampoline=nullptr,*detour=nullptr;std::size_t stolen=0;std::array<std::uint8_t,32> original{};bool patched=false;};
hook g_selector_hook{},g_mtd_hook{};
using mtd_parse_fn=void (__fastcall *)(void*,const void*,std::uint32_t,void*);
mtd_parse_fn g_mtd_parse_orig=nullptr;

template<std::size_t N> bool prepare_hook(hook &h,std::uintptr_t rva,const std::array<std::uint8_t,N>&expect,void *detour){static_assert(N>=14&&N<=32);auto *target=reinterpret_cast<std::uint8_t*>(g_exe_base+rva);std::array<std::uint8_t,N> got{};if(!safe_copy(got.data(),target,N)||got!=expect)return false;void *tr=VirtualAlloc(nullptr,N+14,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!tr)return false;std::memcpy(tr,target,N);auto *tail=static_cast<std::uint8_t*>(tr)+N;tail[0]=0xFF;tail[1]=0x25;std::uint32_t z=0;std::memcpy(tail+2,&z,4);const std::uint64_t back=reinterpret_cast<std::uint64_t>(target+N);std::memcpy(tail+6,&back,8);FlushInstructionCache(GetCurrentProcess(),tr,N+14);h.target=target;h.trampoline=tr;h.detour=detour;h.stolen=N;std::copy(got.begin(),got.end(),h.original.begin());return true;}
bool patch_hook(hook &h){if(!h.target||!h.trampoline||!h.detour)return false;std::array<std::uint8_t,32> p{};p.fill(0x90);p[0]=0xFF;p[1]=0x25;std::uint32_t z=0;std::memcpy(p.data()+2,&z,4);const std::uint64_t q=reinterpret_cast<std::uint64_t>(h.detour);std::memcpy(p.data()+6,&q,8);if(!write_bytes(h.target,p.data(),h.stolen))return false;h.patched=true;return true;}
void restore_hook(hook &h){if(h.patched){if(!write_bytes(h.target,h.original.data(),h.stolen))g_quarantined.store(true);h.patched=false;}if(h.trampoline){VirtualFree(h.trampoline,0,MEM_RELEASE);h.trampoline=nullptr;}}

std::mutex g_mat_mutex;std::unordered_map<void*,std::uint16_t> g_material_donor;
int lookup_donor(void *m){if(!m)return -1;std::lock_guard<std::mutex> l(g_mat_mutex);auto it=g_material_donor.find(m);return it==g_material_donor.end()?-1:static_cast<int>(it->second);}
void register_material(void *material,const void *raw,std::uint32_t len) noexcept {
    if(!material||!raw||!len)return;
    g_mtd_seen.fetch_add(1);
    try{
        const auto h=dsrrl::to_hex(dsrrl::sha256({reinterpret_cast<const std::byte*>(raw),len}));
        const int idx=dsrrl::materialdonor::find_sha256(h);
        std::lock_guard<std::mutex> l(g_mat_mutex);g_material_donor.erase(material);
        if(idx>=0){g_material_donor[material]=static_cast<std::uint16_t>(idx);g_mtd_donor_registered.fetch_add(1);const auto &md=dsrrl::materialdonor::k_donors[static_cast<std::size_t>(idx)];const auto tier=md.c100_tier;if(tier==0)g_mtd_tier0.fetch_add(1);else if(tier==1)g_mtd_tier1.fetch_add(1);else if(tier==2)g_mtd_tier2.fetch_add(1);if(md.has_c101){if(md.c101_tier==1)g_c101_exact_reg.fetch_add(1);else if(md.c101_tier==2)g_c101_sibling_reg.fetch_add(1);}}else g_mtd_donor_unmapped.fetch_add(1);
    }catch(...){g_failopen.fetch_add(1);}
}
void __fastcall hook_mtd_parse(void *material,const void *raw,std::uint32_t len,void *arg4) noexcept {register_material(material,raw,len);g_mtd_parse_orig(material,raw,len,arg4);}
void *resolve_selector_material(void *container,std::int32_t idx) noexcept {if(!container||idx<0||idx>0x100000)return nullptr;void *arr=nullptr;if(!safe_read(static_cast<const std::uint8_t*>(container)+0x10,arr)||!arr)return nullptr;void *m=nullptr;if(!safe_read(static_cast<const std::uint8_t*>(arr)+static_cast<std::size_t>(idx)*24u,m))return nullptr;return m;}
thread_local int g_draw_donor=-1;
extern "C" void selector_observer_impl(void *container,void *,void *ret,void *,void *,std::int32_t idx) noexcept {
    g_selector_seen.fetch_add(1);const auto rr=reinterpret_cast<std::uintptr_t>(ret)-g_exe_base;
    if(rr!=RET_SEL_1&&rr!=RET_SEL_2&&rr!=RET_SEL_3){g_draw_donor=-1;return;}
    void *m=resolve_selector_material(container,idx);g_draw_donor=lookup_donor(m);
    if(g_draw_donor>=0)g_selector_donor.fetch_add(1);
}
extern "C" void selector_observer(void *container,void *owner,void *ret,void *r14,void *r15,std::int32_t idx) noexcept {try{selector_observer_impl(container,owner,ret,r14,r15,idx);}catch(...){g_draw_donor=-1;g_failopen.fetch_add(1);}}

bool install_hooks(){if(!prepare_hook(g_selector_hook,RVA_SELECTOR,BYTES_SELECTOR,reinterpret_cast<void*>(&selector_hook_entry)))return false;g_selector_trampoline=g_selector_hook.trampoline;if(!prepare_hook(g_mtd_hook,RVA_MTD_PARSE_WRAPPER,BYTES_MTD_PARSE_WRAPPER,reinterpret_cast<void*>(&hook_mtd_parse))){restore_hook(g_selector_hook);return false;}g_mtd_parse_orig=reinterpret_cast<mtd_parse_fn>(g_mtd_hook.trampoline);if(!patch_hook(g_mtd_hook)||!patch_hook(g_selector_hook)){restore_hook(g_selector_hook);restore_hook(g_mtd_hook);return false;}return true;}
void restore_hooks(){restore_hook(g_selector_hook);restore_hook(g_mtd_hook);g_selector_trampoline=nullptr;g_mtd_parse_orig=nullptr;}

struct pending {const shader_desc *desc=nullptr;std::size_t size=0;std::string sha;int host=-1;};thread_local std::vector<pending> g_pending;
std::mutex g_pipe_mutex;std::unordered_map<std::uint64_t,int> g_pipes;thread_local int g_bound_host=-1;
const shader_desc *find_ps(std::uint32_t n,const pipeline_subobject *s) noexcept {if(!s)return nullptr;for(std::uint32_t i=0;i<n;++i)if(s[i].type==pipeline_subobject_type::pixel_shader&&s[i].count==1&&s[i].data)return static_cast<const shader_desc*>(s[i].data);return nullptr;}
bool on_create_pipeline(device *d,pipeline_layout,std::uint32_t n,const pipeline_subobject *s){if(!g_enabled.load()||g_quarantined.load()||!d||d->get_api()!=device_api::d3d11)return false;const auto *ps=find_ps(n,s);if(!ps||!ps->code||!ps->code_size)return false;const auto h=dsrrl::to_hex(dsrrl::sha256({reinterpret_cast<const std::byte*>(ps->code),ps->code_size}));const int host=dsrrl::diffusemr::find_original(h);if(host>=0)g_pending.push_back({ps,ps->code_size,h,host});return false;}
void on_init_pipeline(device *d,pipeline_layout,std::uint32_t n,const pipeline_subobject *s,pipeline p){if(!d||d->get_api()!=device_api::d3d11||!p.handle)return;const auto *ps=find_ps(n,s);if(!ps)return;for(auto it=g_pending.begin();it!=g_pending.end();++it){if(it->desc==ps){const auto h=dsrrl::to_hex(dsrrl::sha256({reinterpret_cast<const std::byte*>(ps->code),ps->code_size}));if(h==it->sha&&ps->code_size==it->size){std::lock_guard<std::mutex> l(g_pipe_mutex);g_pipes[p.handle]=it->host;}else{g_quarantined.store(true);log_error("PTDE Material Response V2.11: pipeline attestation mismatch; quarantined.");}g_pending.erase(it);break;}}}
void on_bind_pipeline(command_list *cmd,pipeline_stage stages,pipeline p){if(!cmd||((static_cast<std::uint32_t>(stages)&static_cast<std::uint32_t>(pipeline_stage::pixel_shader))==0))return;int h=-1;{std::lock_guard<std::mutex> l(g_pipe_mutex);auto it=g_pipes.find(p.handle);if(it!=g_pipes.end())h=it->second;}g_bound_host=h;if(h>=0)g_target_binds.fetch_add(1);else g_draw_donor=-1;}

struct devstate {ID3D11Device *dev=nullptr;std::array<ID3D11PixelShader*,24> diffuse{};std::array<ID3D11PixelShader*,24> full{};std::unordered_map<std::uint16_t,ID3D11Buffer*> b12;};devstate g_dev;std::mutex g_dev_mutex;
void release_device(){std::lock_guard<std::mutex> l(g_dev_mutex);for(auto *&p:g_dev.diffuse){if(p){p->Release();p=nullptr;}}for(auto *&p:g_dev.full){if(p){p->Release();p=nullptr;}}for(auto &kv:g_dev.b12)if(kv.second)kv.second->Release();g_dev.b12.clear();if(g_dev.dev){g_dev.dev->Release();g_dev.dev=nullptr;}}
void on_init_device(device *d){if(!d||d->get_api()!=device_api::d3d11)return;release_device();auto *nd=reinterpret_cast<ID3D11Device*>(d->get_native());if(!nd)return;std::lock_guard<std::mutex> l(g_dev_mutex);g_dev.dev=nd;nd->AddRef();std::size_t ok_diff=0,ok_full=0;for(const auto &q:dsrrl::diffusemr::k_plans){if(q.lerp||q.stable_index<0||!q.code||!q.code_size)continue;const auto si=static_cast<std::size_t>(q.stable_index);ID3D11PixelShader *psd=nullptr,*psf=nullptr;if(SUCCEEDED(nd->CreatePixelShader(q.code,q.code_size,nullptr,&psd))&&psd){g_dev.diffuse[si]=psd;++ok_diff;}const auto &fq=dsrrl::c101mr::k_code[si];if(fq.code&&fq.size&&SUCCEEDED(nd->CreatePixelShader(fq.code,fq.size,nullptr,&psf))&&psf){g_dev.full[si]=psf;++ok_full;}}std::ostringstream os;os<<"PTDE Material Response V2.11: diffuse shaders "<<ok_diff<<"/24 c101 shaders "<<ok_full<<"/24";log_info(os.str());if(ok_diff!=24||ok_full!=24)g_quarantined.store(true);}
ID3D11Buffer *realize_b12(ID3D11Device *dev,int donor_index){
    if(!dev||donor_index<0||static_cast<std::size_t>(donor_index)>=dsrrl::materialdonor::k_donors.size())return nullptr;
    std::lock_guard<std::mutex> l(g_dev_mutex);if(g_dev.dev!=dev)return nullptr;
    auto it=g_dev.b12.find(static_cast<std::uint16_t>(donor_index));if(it!=g_dev.b12.end()&&it->second){it->second->AddRef();g_b12_hit.fetch_add(1);return it->second;}
    const auto &d=dsrrl::materialdonor::k_donors[static_cast<std::size_t>(donor_index)];
    struct F4{float x,y,z,w;};std::array<F4,4> payload{{{d.c101_f0q[0],d.c101_f0q[1],d.c101_f0q[2],d.has_c101?1.0f:0.0f},{d.c100[0],d.c100[1],d.c100[2],1.0f},{0,0,0,0},{0,0,0,0}}};
    D3D11_BUFFER_DESC bd{};bd.ByteWidth=64;bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;D3D11_SUBRESOURCE_DATA init{};init.pSysMem=payload.data();ID3D11Buffer*b=nullptr;
    if(FAILED(dev->CreateBuffer(&bd,&init,&b))||!b)return nullptr;
    g_dev.b12.emplace(static_cast<std::uint16_t>(donor_index),b);b->AddRef();g_b12_create.fetch_add(1);return b;
}
bool donor_relevant(int donor_index){return donor_index>=0&&static_cast<std::size_t>(donor_index)<dsrrl::materialdonor::k_donors.size();}
struct cbcap{ID3D11Buffer *base=nullptr,*win=nullptr;UINT first=0,num=0;bool haswin=false,coherent=true;};
cbcap capture_cb(ID3D11DeviceContext *c,ID3D11DeviceContext1 *c1){cbcap x{};c->PSGetConstantBuffers(12,1,&x.base);if(c1){c1->PSGetConstantBuffers1(12,1,&x.win,&x.first,&x.num);x.coherent=x.base==x.win;x.haswin=x.win&&x.num>=16;}return x;}
void release_cb(cbcap &x){if(x.base)x.base->Release();if(x.win)x.win->Release();x={};}
void restore_cb(ID3D11DeviceContext*c,ID3D11DeviceContext1*c1,const cbcap&x){if(c1&&x.haswin){ID3D11Buffer*b=x.win;UINT f=x.first,n=x.num;c1->PSSetConstantBuffers1(12,1,&b,&f,&n);}else{ID3D11Buffer*b=x.base;c->PSSetConstantBuffers(12,1,&b);}}

bool on_draw_indexed(command_list *cmd,std::uint32_t index_count,std::uint32_t instance_count,std::uint32_t first_index,std::int32_t vertex_offset,std::uint32_t first_instance){
    const int donor=g_draw_donor;g_draw_donor=-1;
    const bool eligible=g_enabled.load()&&!g_quarantined.load()&&g_bound_host>=0&&g_bound_host<static_cast<int>(dsrrl::diffusemr::k_plans.size())&&donor_relevant(donor);
    if(!eligible||!cmd)return false;
    const auto &host_plan=dsrrl::diffusemr::k_plans[static_cast<std::size_t>(g_bound_host)];
    if(host_plan.lerp||host_plan.stable_index<0){g_lerp_bypass.fetch_add(1);return false;}
    auto *ctx=reinterpret_cast<ID3D11DeviceContext*>(cmd->get_native());if(!ctx){g_failopen.fetch_add(1);return false;}
    ID3D11Device *dev=nullptr;ctx->GetDevice(&dev);if(!dev)return false;
    ID3D11PixelShader *oldps=nullptr;ctx->PSGetShader(&oldps,nullptr,nullptr);ID3D11PixelShader *pmr=nullptr;const auto &md=dsrrl::materialdonor::k_donors[static_cast<std::size_t>(donor)];const bool use_c101=md.has_c101;
    {std::lock_guard<std::mutex> l(g_dev_mutex);if(g_dev.dev==dev){const auto si=static_cast<std::size_t>(host_plan.stable_index);pmr=use_c101?g_dev.full[si]:g_dev.diffuse[si];if(pmr)pmr->AddRef();}}
    ID3D11Buffer*b12=realize_b12(dev,donor);
    auto cleanup=[&](){if(oldps)oldps->Release();if(pmr)pmr->Release();if(b12)b12->Release();dev->Release();};
    if(!oldps||!pmr||!b12){cleanup();g_failopen.fetch_add(1);return false;}
    ID3D11DeviceContext1 *ctx1=nullptr;ctx->QueryInterface(__uuidof(ID3D11DeviceContext1),reinterpret_cast<void**>(&ctx1));cbcap oldcb=capture_cb(ctx,ctx1);if(!oldcb.coherent){release_cb(oldcb);if(ctx1)ctx1->Release();cleanup();g_failopen.fetch_add(1);return false;}
    ctx->PSSetShader(pmr,nullptr,0);ctx->PSSetConstantBuffers(12,1,&b12);
    if(instance_count<=1)ctx->DrawIndexed(index_count,first_index,vertex_offset);else ctx->DrawIndexedInstanced(index_count,instance_count,first_index,vertex_offset,first_instance);
    ctx->PSSetShader(oldps,nullptr,0);restore_cb(ctx,ctx1,oldcb);
    release_cb(oldcb);if(ctx1)ctx1->Release();cleanup();g_ptde_draws.fetch_add(1);if(use_c101)g_c101_draws.fetch_add(1);else g_c100_only_draws.fetch_add(1);return true;
}

void on_present(command_queue *,swapchain *,const rect *,const rect *,std::uint32_t,const rect *){const auto n=g_present_seq.fetch_add(1,std::memory_order_relaxed)+1;if((n%600)==0){std::ostringstream os;os<<"PTDE Material Response V2.11: MTD="<<g_mtd_seen.load()<<" C100_REG="<<g_mtd_donor_registered.load()<<" TIER0="<<g_mtd_tier0.load()<<" TIER1="<<g_mtd_tier1.load()<<" TIER2="<<g_mtd_tier2.load()<<" C101_EXACT_REG="<<g_c101_exact_reg.load()<<" C101_SIBLING_REG="<<g_c101_sibling_reg.load()<<" UNMAPPED="<<g_mtd_donor_unmapped.load()<<" SELECTOR="<<g_selector_seen.load()<<" DONOR_SELECTOR="<<g_selector_donor.load()<<" TARGET_BINDS="<<g_target_binds.load()<<" PTDE_DIFFUSE_DRAWS="<<g_ptde_draws.load()<<" PTDE_C101_DRAWS="<<g_c101_draws.load()<<" C100_ONLY_DRAWS="<<g_c100_only_draws.load()<<" LERP_BYPASS="<<g_lerp_bypass.load()<<" B12_CREATE="<<g_b12_create.load()<<" B12_HIT="<<g_b12_hit.load()<<" FAILOPEN="<<g_failopen.load();log_info(os.str());}}
}

extern "C" {
__declspec(dllexport) const char *NAME="DSRRL PTDE Material Response V2.11";
__declspec(dllexport) const char *AUTHOR="DSR Restored Lighting";
__declspec(dllexport) const char *DESCRIPTION="PTDE material-response V2.11 diagnostic: accepted V2.9.1 c100/diffuse-linear baseline plus per-material PTDE c101 gain encoded as c101^(1/2.2); only the canonical pre-F0 MUL_SAT saturation modifier is removed so the linear PTDE gain is not clipped at F0=1. Stock SPEC x^2.2 and complete downstream DSR PBL/EnvSpec/cubemap/roughness/LOD/BRDF remain unchanged; HemEnvLerp remains stock DSR; Tier2 remains c100-only.";
}
extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon,HMODULE reshade_module){if(!reshade::register_addon(addon,reshade_module))return false;g_exe_base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));if(!g_exe_base||!verify_provenance()||!install_hooks()){log_error("PTDE Material Response V2.11: provenance/hook install failed; unload.");restore_hooks();reshade::unregister_addon(addon,reshade_module);return false;}g_enabled.store(true);reshade::register_event<reshade::addon_event::init_device>(on_init_device);reshade::register_event<reshade::addon_event::create_pipeline>(on_create_pipeline);reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);reshade::register_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);reshade::register_event<reshade::addon_event::present>(on_present);log_info("PTDE Material Response V2.11 active: PTDE c100 + DIFFUSE linear baseline plus analytic PTDE c101 gain with the native pre-F0 SAT projection removed on 24 stable Phn Spc HemEnv hosts; stock DSR x^2.2 and downstream PBL/EnvSpec retained; HemEnvLerp stock DSR.");return true;}
extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon,HMODULE reshade_module){g_enabled.store(false);restore_hooks();release_device();{std::lock_guard<std::mutex> l(g_mat_mutex);g_material_donor.clear();}{std::lock_guard<std::mutex> l(g_pipe_mutex);g_pipes.clear();}reshade::unregister_addon(addon,reshade_module);}
BOOL WINAPI DllMain(HINSTANCE h,DWORD r,LPVOID){if(r==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(h);return TRUE;}
