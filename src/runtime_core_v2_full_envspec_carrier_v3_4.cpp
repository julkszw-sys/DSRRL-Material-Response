#define AddonInit V31InternalAddonInit
#define AddonUninit V31InternalAddonUninit
#define NAME V31_INTERNAL_NAME
#define DESCRIPTION V31_INTERNAL_DESCRIPTION
#include "runtime_core_v2_full_envspec_carrier_v3_part1.inc"
#include "runtime_core_v2_full_envspec_carrier_v3_part2.inc"
#include "runtime_core_v2_full_envspec_carrier_v3_part3.inc"
#include "runtime_core_v2_full_envspec_carrier_v3_part4.inc"
#undef DESCRIPTION
#undef NAME
#undef AddonUninit
#undef AddonInit

#include <d3d11.h>
#include <unordered_map>

namespace dsrrl::runtime_v2::full_envspec_v3 {
namespace {

constexpr std::uintptr_t k_parser_thunk_rva = 0x29DFC0;
constexpr std::array<std::uintptr_t,4> k_call_rvas = {0x291D9C,0x20E014,0x20EB7A,0x20FB99};
constexpr std::array<std::array<std::uint8_t,5>,4> k_call_bytes = {{
    {{0xE8,0x1F,0xC2,0x00,0x00}}, {{0xE8,0x07,0xDA,0x01,0x00}},
    {{0xE8,0xA1,0xCE,0x01,0x00}}, {{0xE8,0x82,0xBE,0x01,0x00}}
}};
constexpr char k_build131_sha[] = "db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966";

struct call_patch { std::uint8_t *site=nullptr; std::array<std::uint8_t,5> original{}; std::uint8_t *relay=nullptr; bool installed=false; };
std::array<call_patch,4> g_call_patches{};
std::uint8_t *g_relay_page=nullptr;
using parser_thunk_fn = void (__fastcall *)(void *, const void *, std::uint32_t);
parser_thunk_fn g_parser_thunk=nullptr;
selector_fn g_selector_entry=nullptr;

std::atomic<std::uint64_t> g_host_ok{0},g_host_fail{0};
std::atomic<std::uint64_t> g_ps_stock_created{0},g_ps_dedicated_created{0};
std::atomic<std::uint64_t> g_ps_stock_match{0},g_ps_dedicated_match{0},g_ps_unknown{0};
std::atomic<std::uint64_t> g_present_reject{0},g_present_nonpmetal{0},g_raw_created{0},g_native_tx{0};

void *alloc_near(void *near_address)
{
    SYSTEM_INFO si{}; GetSystemInfo(&si);
    const auto gran=static_cast<std::uintptr_t>(si.dwAllocationGranularity);
    const auto anchor=reinterpret_cast<std::uintptr_t>(near_address)&~(gran-1u);
    const auto lo=reinterpret_cast<std::uintptr_t>(si.lpMinimumApplicationAddress);
    const auto hi=reinterpret_cast<std::uintptr_t>(si.lpMaximumApplicationAddress);
    constexpr std::uintptr_t limit=0x7FFF0000ull;
    for(std::uintptr_t d=0;d<=limit;d+=gran){
        if(anchor>=d){ const auto a=anchor-d; if(a>=lo) if(void *p=VirtualAlloc(reinterpret_cast<void *>(a),4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE)) return p; }
        if(d!=0&&anchor<=hi-d){ const auto a=anchor+d; if(a<=hi) if(void *p=VirtualAlloc(reinterpret_cast<void *>(a),4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE)) return p; }
    }
    return nullptr;
}

void write_relay(std::uint8_t *dst,const void *target)
{
    dst[0]=0xFF; dst[1]=0x25; dst[2]=dst[3]=dst[4]=dst[5]=0;
    const auto a=reinterpret_cast<std::uint64_t>(target); std::memcpy(dst+6,&a,sizeof(a)); dst[14]=dst[15]=0x90;
}

bool patch_call(std::size_t i,std::uint8_t *site,const void *hook,const std::array<std::uint8_t,5> &expected)
{
    if(i>=g_call_patches.size()||site==nullptr||hook==nullptr||g_relay_page==nullptr||std::memcmp(site,expected.data(),5)!=0) return false;
    auto &s=g_call_patches[i]; s.site=site; s.original=expected; s.relay=g_relay_page+i*16; write_relay(s.relay,hook);
    const std::intptr_t disp=s.relay-(site+5); if(disp<std::numeric_limits<std::int32_t>::min()||disp>std::numeric_limits<std::int32_t>::max()) return false;
    DWORD old=0; if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old)) return false;
    std::array<std::uint8_t,5> p{}; p[0]=0xE8; const auto rel=static_cast<std::int32_t>(disp); std::memcpy(p.data()+1,&rel,4); std::memcpy(site,p.data(),5);
    FlushInstructionCache(GetCurrentProcess(),site,5); DWORD ignored=0; VirtualProtect(site,5,old,&ignored); s.installed=true; return true;
}

void unpatch_calls()
{
    for(auto &s:g_call_patches){ if(!s.installed||s.site==nullptr) continue; DWORD old=0; if(VirtualProtect(s.site,5,PAGE_EXECUTE_READWRITE,&old)){ std::memcpy(s.site,s.original.data(),5); FlushInstructionCache(GetCurrentProcess(),s.site,5); DWORD ignored=0; VirtualProtect(s.site,5,old,&ignored);} s={}; }
    if(g_relay_page!=nullptr){ VirtualFree(g_relay_page,0,MEM_RELEASE); g_relay_page=nullptr; }
    g_parser_thunk=nullptr; g_selector_entry=nullptr;
}

bool verify_host()
{
    HMODULE h=GetModuleHandleW(L"DSRRL_Material_Response_1.45.addon64");
    if(h==nullptr){++g_host_fail;return false;} std::array<std::uint8_t,32> d{}; const auto p=module_path(h);
    if(p.empty()||!sha256_file(p,d)||hex_string(d.data(),d.size())!=k_build131_sha){++g_host_fail;return false;} ++g_host_ok; return true;
}

void remember_material(void *material,const void *raw,std::uint32_t size,const wchar_t *key)
{
    material_binding b{};
    if(material!=nullptr&&raw!=nullptr&&size!=0&&size<=(1u<<20)){
        std::array<std::uint8_t,32> sha{}; const auto name=fnv1a_utf16_basename_lower_ascii(key);
        if(sha256_bytes(raw,size,sha)){ b.record=find_material(name,sha); if(b.record!=nullptr&&b.record->state==material_envspec_state::explicit_none) b.explicit_none_safe=b.record->explicit_none_safe; }
    }
    if(material!=nullptr){ std::lock_guard lock(g_mutex); if(b.record!=nullptr){g_materials[reinterpret_cast<std::uintptr_t>(material)]=b;++g_material_mapped;}else{g_materials.erase(reinterpret_cast<std::uintptr_t>(material));++g_material_unmapped;} }
}

void __fastcall parser_hook_v34(void *owner,const void *raw,std::uint32_t size)
{
    ++g_parser_seen; const wchar_t *key=nullptr; void *material=nullptr;
    if(owner!=nullptr){ __try{ key=*reinterpret_cast<const wchar_t *const *>(static_cast<const std::uint8_t *>(owner)+0x08); material=static_cast<std::uint8_t *>(owner)+0x28; }__except(EXCEPTION_EXECUTE_HANDLER){key=nullptr;material=nullptr;} }
    if(g_parser_thunk!=nullptr) g_parser_thunk(owner,raw,size); remember_material(material,raw,size,key);
}

std::uintptr_t __fastcall selector_hook_v34(void *container,void *a2,std::int32_t material_index,std::uint32_t a4,std::uint64_t a5,std::uint64_t a6,std::uint64_t a7,std::uint64_t a8,std::uint64_t a9,std::uint64_t a10)
{
    ++g_selector_seen; std::uintptr_t actual=0;
    if(container!=nullptr&&material_index>=0){ __try{ const auto base=*reinterpret_cast<const std::uintptr_t *>(static_cast<const std::uint8_t *>(container)+0x10); if(base!=0) actual=*reinterpret_cast<const std::uintptr_t *>(base+static_cast<std::uintptr_t>(material_index)*24u); }__except(EXCEPTION_EXECUTE_HANDLER){actual=0;} }
    if(actual!=0)++g_selector_resolved; g_active_material=actual; if(g_selector_entry==nullptr)return 0;
    return g_selector_entry(container,a2,material_index,a4,a5,a6,a7,a8,a9,a10);
}

bool install_calls()
{
    HMODULE exe=GetModuleHandleW(nullptr); if(exe==nullptr)return false; std::array<std::uint8_t,32>d{}; const auto path=module_path(exe);
    if(path.empty()||!sha256_file(path,d)||hex_string(d.data(),d.size())!=k_expected_exe_sha256){++g_exe_guard_fail;return false;} ++g_exe_guard_pass;
    auto *base=reinterpret_cast<std::uint8_t *>(exe); g_relay_page=static_cast<std::uint8_t *>(alloc_near(base+k_call_rvas[0])); if(g_relay_page==nullptr){++g_hook_install_fail;return false;}
    g_parser_thunk=reinterpret_cast<parser_thunk_fn>(base+k_parser_thunk_rva); g_selector_entry=reinterpret_cast<selector_fn>(base+k_selector_rva);
    if(!patch_call(0,base+k_call_rvas[0],reinterpret_cast<const void *>(&parser_hook_v34),k_call_bytes[0])||
       !patch_call(1,base+k_call_rvas[1],reinterpret_cast<const void *>(&selector_hook_v34),k_call_bytes[1])||
       !patch_call(2,base+k_call_rvas[2],reinterpret_cast<const void *>(&selector_hook_v34),k_call_bytes[2])||
       !patch_call(3,base+k_call_rvas[3],reinterpret_cast<const void *>(&selector_hook_v34),k_call_bytes[3])){unpatch_calls();++g_hook_install_fail;return false;}
    ++g_hook_install_pass; return true;
}

struct dedicated_identity{std::uint32_t id,size;std::array<std::uint8_t,16> checksum;};
inline constexpr std::array<dedicated_identity,3> k_dedicated={{
    {20072u,19872u,{{0x17,0x17,0x21,0xfc,0x0e,0x2a,0xa0,0x10,0x88,0xb5,0x5f,0xa4,0xa9,0x06,0x7e,0x82}}},
    {20073u,19572u,{{0x38,0xa2,0x0c,0xac,0xd0,0xf1,0x1e,0x23,0xef,0xb2,0xc2,0xc8,0xf6,0x6b,0x3c,0x91}}},
    {20074u,18076u,{{0x02,0xb1,0x57,0xdf,0xe6,0x0e,0x24,0xbd,0x07,0xf4,0x05,0xb6,0xd6,0x87,0x47,0x1c}}}
}};
inline constexpr std::array<std::uint8_t,32> k_pmetal={{0xec,0xe7,0x0f,0x36,0xbd,0x25,0x17,0xd2,0x8c,0x84,0x95,0xe2,0x76,0xce,0xa5,0x37,0xf8,0xb5,0x19,0xd6,0xbe,0xd9,0x81,0x78,0x8e,0x79,0xa4,0x09,0xff,0xbf,0x76,0x3b}};
bool exact_pmetal(const material_record &m)noexcept{return m.raw_sha==k_pmetal;}

enum class receiver_kind:std::uint8_t{stock,dedicated}; struct receiver_rec{receiver_kind kind;std::uint32_t id;};
std::unordered_map<std::uint64_t,receiver_rec> g_ps;

bool dedicated_code(const void *code,std::size_t size,std::uint32_t &id)noexcept
{
    if(code==nullptr||size<20)return false; const auto *b=static_cast<const std::uint8_t *>(code); if(std::memcmp(b,"DXBC",4)!=0)return false;
    for(const auto &r:k_dedicated)if(r.size==size&&std::memcmp(b+4,r.checksum.data(),16)==0){id=r.id;return true;} return false;
}

void init_pipeline_v34(reshade::api::device *,reshade::api::pipeline_layout,std::uint32_t count,const reshade::api::pipeline_subobject *subs,reshade::api::pipeline pipeline)
{
    const reshade::api::shader_desc *ps=nullptr; for(std::uint32_t i=0;i<count;++i)if(subs[i].type==reshade::api::pipeline_subobject_type::pixel_shader){ps=static_cast<const reshade::api::shader_desc *>(subs[i].data);break;}
    if(ps==nullptr||ps->code==nullptr||ps->code_size==0)return; std::uint32_t id=0; receiver_rec rec{};
    if(dedicated_code(ps->code,ps->code_size,id)){rec={receiver_kind::dedicated,id};++g_ps_dedicated_created;}
    else{std::uint64_t family=0;if(!classify_receiver(ps->code,ps->code_size,0,&id,&family))return;rec={receiver_kind::stock,id};++g_ps_stock_created;}
    std::lock_guard lock(g_mutex);g_ps[pipeline.handle]=rec;
}
void destroy_pipeline_v34(reshade::api::device *,reshade::api::pipeline p){std::lock_guard lock(g_mutex);g_ps.erase(p.handle);}

std::optional<receiver_rec> native_receiver(reshade::api::command_list *cmd)
{
    if(cmd==nullptr||cmd->get_native()==0)return std::nullopt; auto *ctx=reinterpret_cast<ID3D11DeviceContext *>(static_cast<std::uintptr_t>(cmd->get_native())); ID3D11PixelShader *ps=nullptr; ctx->PSGetShader(&ps,nullptr,nullptr);
    if(ps==nullptr){++g_ps_unknown;return std::nullopt;} const auto h=static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ps)); ps->Release(); std::lock_guard lock(g_mutex);const auto it=g_ps.find(h);
    if(it==g_ps.end()){++g_ps_unknown;return std::nullopt;} if(it->second.kind==receiver_kind::dedicated)++g_ps_dedicated_match;else++g_ps_stock_match;return it->second;
}

std::optional<reshade::api::resource_view> raw_cube(std::uint16_t probe,std::uint8_t slot)
{
    if(g_device==nullptr||g_ptde_pack.empty()||probe>=k_probe_count||slot>=k_ptde_slots)return std::nullopt; const std::uint32_t key=static_cast<std::uint32_t>(probe)*k_ptde_slots+slot;
    {std::lock_guard lock(g_mutex);const auto it=g_virtual_cubes.find(key);if(it!=g_virtual_cubes.end())return it->second.view;}
    const std::size_t off=static_cast<std::size_t>(key)*k_ptde_bytes_per_cube,face_bytes=static_cast<std::size_t>(k_ptde_size)*k_ptde_size*4u;std::array<reshade::api::subresource_data,k_faces> sub{};
    for(std::uint32_t f=0;f<k_faces;++f){sub[f].data=g_ptde_pack.data()+off+static_cast<std::size_t>(f)*face_bytes;sub[f].row_pitch=k_ptde_size*4u;sub[f].slice_pitch=static_cast<std::uint32_t>(face_bytes);}
    const reshade::api::resource_desc desc(reshade::api::resource_type::texture_2d,k_ptde_size,k_ptde_size,k_faces,1,reshade::api::format::r8g8b8a8_unorm,1,reshade::api::memory_heap::default_,reshade::api::resource_usage::shader_resource,reshade::api::resource_flags::cube_compatible);
    virtual_cube c{};c.probe_index=probe;c.slot=slot;g_internal_resource_create=true;const bool rok=g_device->create_resource(desc,sub.data(),reshade::api::resource_usage::shader_resource,&c.resource);bool vok=false;
    if(rok){const reshade::api::resource_view_desc vd(reshade::api::resource_view_type::texture_cube,reshade::api::format::r8g8b8a8_unorm,0,1,0,k_faces);vok=g_device->create_resource_view(c.resource,reshade::api::resource_usage::shader_resource,vd,&c.view);}if(!vok&&c.resource.handle!=0)g_device->destroy_resource(c.resource);g_internal_resource_create=false;if(!rok||!vok){++g_virtual_create_fail;return std::nullopt;}
    {std::lock_guard lock(g_mutex);const auto [it,inserted]=g_virtual_cubes.emplace(key,c);if(!inserted){g_internal_resource_create=true;g_device->destroy_resource_view(c.view);g_device->destroy_resource(c.resource);g_internal_resource_create=false;return it->second.view;}}
    ++g_virtual_created;++g_raw_created;return c.view;
}

struct native_state{
    ID3D11DeviceContext *ctx=nullptr;std::array<ID3D11ShaderResourceView *,2> srv{};std::array<ID3D11SamplerState *,2> sampler{};
    native_state()=default;native_state(const native_state&)=delete;native_state&operator=(const native_state&)=delete;
    native_state(native_state&&o)noexcept:ctx(o.ctx),srv(o.srv),sampler(o.sampler){o.ctx=nullptr;o.srv={};o.sampler={};}
    native_state&operator=(native_state&&o)noexcept{if(this!=&o){release();ctx=o.ctx;srv=o.srv;sampler=o.sampler;o.ctx=nullptr;o.srv={};o.sampler={};}return *this;}
    ~native_state(){release();}void release()noexcept{for(auto *&v:srv)if(v){v->Release();v=nullptr;}for(auto *&s:sampler)if(s){s->Release();s=nullptr;}ctx=nullptr;}
};
std::optional<native_state> capture(reshade::api::command_list *cmd){if(cmd==nullptr||cmd->get_native()==0)return std::nullopt;native_state s{};s.ctx=reinterpret_cast<ID3D11DeviceContext *>(static_cast<std::uintptr_t>(cmd->get_native()));s.ctx->PSGetShaderResources(12,1,&s.srv[0]);s.ctx->PSGetShaderResources(14,1,&s.srv[1]);s.ctx->PSGetSamplers(12,1,&s.sampler[0]);s.ctx->PSGetSamplers(14,1,&s.sampler[1]);return std::optional<native_state>(std::move(s));}

struct plan{native_state old{};std::array<ID3D11ShaderResourceView *,2> replacement{};std::array<bool,2> replace{};bool exact_sampler=false;};
std::optional<plan> build_plan_v34(reshade::api::command_list *cmd)
{
    const auto b=active_binding();if(!b.has_value()||b->record==nullptr){++g_material_unknown;++g_fail_open;return std::nullopt;}const material_record &m=*b->record;const auto r=native_receiver(cmd);if(!r.has_value()){++g_receiver_reject;++g_fail_open;return std::nullopt;}++g_receiver_match;
    auto st=capture(cmd);if(!st.has_value()){++g_fail_open;return std::nullopt;}plan p{};p.old=std::move(*st);
    if(m.state==material_envspec_state::present){
        if(!exact_pmetal(m)){++g_present_nonpmetal;++g_material_unknown;++g_fail_open;return std::nullopt;}if(r->kind!=receiver_kind::dedicated){++g_present_reject;++g_fail_open;return std::nullopt;}if(p.old.srv[0]==nullptr||p.old.srv[1]==nullptr){++g_resource_route_miss;++g_fail_open;return std::nullopt;}
        const auto a=probe_for_view(reshade::api::resource_view{static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p.old.srv[0]))});const auto bb=probe_for_view(reshade::api::resource_view{static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p.old.srv[1]))});if(!a.has_value()||!bb.has_value()){++g_resource_route_miss;++g_fail_open;return std::nullopt;}
        const auto ra=raw_cube(*a,m.slot),rb=raw_cube(*bb,m.slot);if(!ra.has_value()||!rb.has_value()){++g_resource_route_miss;++g_fail_open;return std::nullopt;}p.replacement[0]=reinterpret_cast<ID3D11ShaderResourceView *>(static_cast<std::uintptr_t>(ra->handle));p.replacement[1]=reinterpret_cast<ID3D11ShaderResourceView *>(static_cast<std::uintptr_t>(rb->handle));p.replace={true,true};p.exact_sampler=true;g_resource_route_hit.fetch_add(2);++g_material_present;return p;
    }
    if(m.state==material_envspec_state::explicit_none&&b->explicit_none_safe){if(r->kind!=receiver_kind::stock){++g_fail_open;return std::nullopt;}if(g_black_view.handle==0&&!create_black_cube()){++g_fail_open;return std::nullopt;}auto *black=reinterpret_cast<ID3D11ShaderResourceView *>(static_cast<std::uintptr_t>(g_black_view.handle));bool any=false;for(int i=0;i<2;++i){if(p.old.srv[i]==nullptr)continue;p.replacement[i]=black;p.replace[i]=true;any=true;}if(!any){++g_resource_route_miss;++g_fail_open;return std::nullopt;}++g_material_explicit_none;return p;}
    ++g_material_unknown;++g_fail_open;return std::nullopt;
}

void apply(plan &p){for(int i=0;i<2;++i){if(!p.replace[i])continue;const UINT slot=i==0?12u:14u;auto *v=p.replacement[i];p.old.ctx->PSSetShaderResources(slot,1,&v);if(slot==12)++g_t12_rebind;else++g_t14_rebind;if(p.exact_sampler&&g_ptde_sampler.handle!=0){auto *s=reinterpret_cast<ID3D11SamplerState *>(static_cast<std::uintptr_t>(g_ptde_sampler.handle));p.old.ctx->PSSetSamplers(slot,1,&s);++g_sampler_rebind;}}++g_native_tx;}
void restore(plan &p){for(int i=0;i<2;++i){if(!p.replace[i])continue;const UINT slot=i==0?12u:14u;auto *v=p.old.srv[i];p.old.ctx->PSSetShaderResources(slot,1,&v);auto *s=p.old.sampler[i];p.old.ctx->PSSetSamplers(slot,1,&s);}++g_restore_pass;}

bool draw_v34(reshade::api::command_list *cmd,std::uint32_t vc,std::uint32_t ic,std::uint32_t fv,std::uint32_t fi){if(g_internal_replay)return false;auto p=build_plan_v34(cmd);g_active_material=0;if(!p.has_value())return false;apply(*p);g_internal_replay=true;cmd->draw(vc,ic,fv,fi);g_internal_replay=false;restore(*p);++g_replay_pass;return true;}
bool draw_indexed_v34(reshade::api::command_list *cmd,std::uint32_t icount,std::uint32_t inst,std::uint32_t first,std::int32_t vo,std::uint32_t fi){if(g_internal_replay)return false;auto p=build_plan_v34(cmd);g_active_material=0;if(!p.has_value())return false;apply(*p);g_internal_replay=true;cmd->draw_indexed(icount,inst,first,vo,fi);g_internal_replay=false;restore(*p);++g_replay_pass;return true;}

void present_v34(reshade::api::command_queue *,reshade::api::swapchain *,const reshade::api::rect *,const reshade::api::rect *,std::uint32_t,const reshade::api::rect *)
{
    const auto p=++g_present_count;if(p!=1&&(p%300)!=0)return;char line[1900]{};std::snprintf(line,sizeof(line),
        "[DSRRL FULL ENVSPEC V3.4] P=%llu HOST=%llu/%llu EXE=%llu/%llu HOOK=%llu/%llu PARSER=%llu mapped=%llu unmapped=%llu SELECTOR=%llu resolved=%llu nativeProbe=%llu/%llu miss=%llu psCreatedStock=%llu psCreatedDedicated=%llu psMatchStock=%llu psMatchDedicated=%llu psUnknown=%llu raw=%llu mat_present=%llu explicit_none=%llu unknown=%llu presentReject=%llu nonPMetal=%llu route=%llu miss=%llu t12=%llu t14=%llu sampler=%llu nativeTx=%llu replay=%llu restore=%llu/%llu failopen=%llu",
        static_cast<unsigned long long>(p),static_cast<unsigned long long>(g_host_ok.load()),static_cast<unsigned long long>(g_host_fail.load()),static_cast<unsigned long long>(g_exe_guard_pass.load()),static_cast<unsigned long long>(g_exe_guard_fail.load()),static_cast<unsigned long long>(g_hook_install_pass.load()),static_cast<unsigned long long>(g_hook_install_fail.load()),static_cast<unsigned long long>(g_parser_seen.load()),static_cast<unsigned long long>(g_material_mapped.load()),static_cast<unsigned long long>(g_material_unmapped.load()),static_cast<unsigned long long>(g_selector_seen.load()),static_cast<unsigned long long>(g_selector_resolved.load()),static_cast<unsigned long long>(g_native_matches.load()),static_cast<unsigned long long>(g_native_candidates.load()),static_cast<unsigned long long>(g_native_hash_miss.load()),static_cast<unsigned long long>(g_ps_stock_created.load()),static_cast<unsigned long long>(g_ps_dedicated_created.load()),static_cast<unsigned long long>(g_ps_stock_match.load()),static_cast<unsigned long long>(g_ps_dedicated_match.load()),static_cast<unsigned long long>(g_ps_unknown.load()),static_cast<unsigned long long>(g_raw_created.load()),static_cast<unsigned long long>(g_material_present.load()),static_cast<unsigned long long>(g_material_explicit_none.load()),static_cast<unsigned long long>(g_material_unknown.load()),static_cast<unsigned long long>(g_present_reject.load()),static_cast<unsigned long long>(g_present_nonpmetal.load()),static_cast<unsigned long long>(g_resource_route_hit.load()),static_cast<unsigned long long>(g_resource_route_miss.load()),static_cast<unsigned long long>(g_t12_rebind.load()),static_cast<unsigned long long>(g_t14_rebind.load()),static_cast<unsigned long long>(g_sampler_rebind.load()),static_cast<unsigned long long>(g_native_tx.load()),static_cast<unsigned long long>(g_replay_pass.load()),static_cast<unsigned long long>(g_restore_pass.load()),static_cast<unsigned long long>(g_restore_fail.load()),static_cast<unsigned long long>(g_fail_open.load()));reshade::log::message(reshade::log::level::info,line);
}

void register_events(){reshade::register_event<reshade::addon_event::init_device>(on_init_device);reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);reshade::register_event<reshade::addon_event::init_resource>(on_init_resource);reshade::register_event<reshade::addon_event::destroy_resource>(on_destroy_resource);reshade::register_event<reshade::addon_event::init_resource_view>(on_init_resource_view);reshade::register_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);reshade::register_event<reshade::addon_event::init_pipeline>(init_pipeline_v34);reshade::register_event<reshade::addon_event::destroy_pipeline>(destroy_pipeline_v34);reshade::register_event<reshade::addon_event::draw>(draw_v34);reshade::register_event<reshade::addon_event::draw_indexed>(draw_indexed_v34);reshade::register_event<reshade::addon_event::present>(present_v34);}
void unregister_events(){reshade::unregister_event<reshade::addon_event::present>(present_v34);reshade::unregister_event<reshade::addon_event::draw_indexed>(draw_indexed_v34);reshade::unregister_event<reshade::addon_event::draw>(draw_v34);reshade::unregister_event<reshade::addon_event::destroy_pipeline>(destroy_pipeline_v34);reshade::unregister_event<reshade::addon_event::init_pipeline>(init_pipeline_v34);reshade::unregister_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);reshade::unregister_event<reshade::addon_event::init_resource_view>(on_init_resource_view);reshade::unregister_event<reshade::addon_event::destroy_resource>(on_destroy_resource);reshade::unregister_event<reshade::addon_event::init_resource>(on_init_resource);reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);}

} }

extern "C" __declspec(dllexport) const char *NAME="DSRRL PTDE Full EnvSpec Carrier V3.4 Native PS Gate";
extern "C" __declspec(dllexport) const char *DESCRIPTION="Exact build131 + actual material + native D3D11 PS receiver gate; raw PTDE RGBA; native t12/t14+s12/s14 transaction; fail-open elsewhere.";
extern "C" __declspec(dllexport) bool AddonInit(HMODULE addon_module,HMODULE reshade_module)
{
    if(!reshade::register_addon(addon_module,reshade_module))return false;using namespace dsrrl::runtime_v2::full_envspec_v3;
    if(!verify_host()){reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V3.4] exact build131 host SHA guard failed; bridge inert");return true;}register_events();
    if(!load_pack(addon_module))reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V3.4] PTDE pack load/SHA failed; PRESENT fail-open");
    if(!load_router_sidecars(addon_module))reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V3.4] sidecar load/SHA failed; bridge fail-open");
    if(!install_calls())reshade::log::message(reshade::log::level::error,"[DSRRL FULL ENVSPEC V3.4] retail callsite guard failed; bridge fail-open");else reshade::log::message(reshade::log::level::info,"[DSRRL FULL ENVSPEC V3.4] native PS gate armed");return true;
}
extern "C" __declspec(dllexport) void AddonUninit(HMODULE addon_module,HMODULE reshade_module)
{
    using namespace dsrrl::runtime_v2::full_envspec_v3;unpatch_calls();unregister_events();{std::lock_guard lock(g_mutex);g_materials.clear();g_native_resources.clear();g_source_resource_by_view.clear();g_command_bindings.clear();g_ps.clear();}g_ptde_pack.clear();g_material_registry.clear();g_native_probe_registry.clear();reshade::unregister_addon(addon_module,reshade_module);
}
