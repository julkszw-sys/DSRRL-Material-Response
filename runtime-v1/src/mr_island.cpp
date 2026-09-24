#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/mr_island.hpp"
#include "dsrrl/runtime/mr_dxbc_transform.hpp"
#include "dsrrl/runtime/engine_hooks.hpp"
#include "dsrrl/runtime/asset_bridges.hpp"
#include "dsrrl/runtime/draw_replay.hpp"
#include "dsrrl/runtime/upper_lower_runtime.hpp"
#include "dsrrl/runtime/generated_ul_stable_hashes.hpp"
#include "dsrrl/runtime/generated_spec_material_routes.hpp"
#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/sha256.hpp"
#include "dsrrl/runtime/subsurface_dispatch.hpp"
#include "ptde_material_donor_registry.hpp"

#include <reshade.hpp>
#include <Windows.h>
#include <d3d11_1.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace reshade::api;

namespace dsrrl::runtime::mr {
namespace {

constexpr std::uintptr_t k_ret_sel_1 = 0x20E019;
constexpr std::uintptr_t k_ret_sel_2 = 0x20EB7F;
constexpr std::uintptr_t k_ret_sel_3 = 0x20FB9E;

core::renderer_core *g_core = nullptr;
std::atomic<bool> g_enabled{false};
std::atomic<bool> g_quarantined{false};
std::atomic<std::uint64_t> g_draw_serial{0};

std::atomic<std::uint64_t> g_mtd_seen{0}, g_mapped{0}, g_unmapped{0};
std::atomic<std::uint64_t> g_selector_seen{0}, g_selector_mapped{0};
std::atomic<std::uint64_t> g_pipeline_seen{0}, g_shader_pair_pass{0}, g_shader_pair_fail{0};
std::atomic<std::uint64_t> g_shader_ul_pass{0}, g_shader_ul_fail{0};
std::atomic<std::uint64_t> g_shader_spec_pass{0}, g_shader_spec_fail{0};
std::atomic<std::uint64_t> g_shader_ul_spec_pass{0}, g_shader_ul_spec_fail{0};
std::atomic<std::uint64_t> g_lerp_shader_pass{0}, g_lerp_shader_fail{0};
std::atomic<std::uint64_t> g_v9a_shader_pass{0}, g_v9a_shader_fail{0};
std::atomic<std::uint64_t> g_lerp_binds{0}, g_lerp_replays{0}, g_v10_replays{0};
std::atomic<std::uint64_t> g_target_binds{0}, g_replays{0}, g_fail_open{0};
std::atomic<std::uint64_t> g_b12_create{0}, g_b12_hit{0}, g_restore_fail{0}, g_present{0};

void log_info(const std::string &s){ reshade::log::message(reshade::log::level::info,s.c_str()); }
void log_error(const std::string &s){ reshade::log::message(reshade::log::level::error,s.c_str()); }

std::mutex g_material_mutex;
std::unordered_map<void*,std::uint16_t> g_material_donor;

int donor_for(void *material)
{
    if(!material) return -1;
    std::lock_guard lock(g_material_mutex);
    const auto it=g_material_donor.find(material);
    return it==g_material_donor.end() ? -1 : static_cast<int>(it->second);
}

void *resolve_material(void *container,std::int32_t index) noexcept
{
    if(!container || index<0 || index>0x100000) return nullptr;
    void *array=nullptr;
    if(!engine::safe_read_bytes(static_cast<const std::uint8_t*>(container)+0x10,&array,sizeof(array)) || !array)
        return nullptr;
    void *material=nullptr;
    if(!engine::safe_read_bytes(static_cast<const std::uint8_t*>(array)+static_cast<std::size_t>(index)*24u,
                                &material,sizeof(material)))
        return nullptr;
    return material;
}

thread_local int g_draw_donor=-1;
thread_local int g_bound_host=-1;
thread_local bool g_bound_lerp=false;
thread_local bool g_bound_subsurface=false;
constexpr int k_subsurface_material=static_cast<int>(dsrrl::materialdonor::k_donors.size());
constexpr int k_pmetal_route=345;
namespace subsurface=dsrrl::operators::resource_bridges;
std::atomic<std::uint64_t> g_subsurface_replays{0};
thread_local const command_list *g_bound_command=nullptr;

enum class pipeline_kind : std::uint8_t {
    stable = 0,
    lerp,
    subsurface
};

struct pipeline_target {
    std::uint8_t pair=0;
    pipeline_kind kind=pipeline_kind::stable;
};

struct pending_pipeline {
    std::size_t size=0;
    std::string sha;
    pipeline_target target{};
};
std::mutex g_pending_mutex;
std::vector<pending_pipeline> g_pending;

std::mutex g_pipeline_mutex;
std::unordered_map<std::uint64_t,pipeline_target> g_pipelines;

struct device_state {
    ID3D11Device *device=nullptr;

    // Stable HemEnv pair family.
    std::array<ID3D11PixelShader*,24> diffuse{};
    std::array<ID3D11PixelShader*,24> full{};
    std::array<ID3D11PixelShader*,24> diffuse_ul{};
    std::array<ID3D11PixelShader*,24> full_ul{};
    std::array<ID3D11PixelShader*,24> full_spec{};
    std::array<ID3D11PixelShader*,24> full_ul_spec{};

    // Exact HemEnvLerp family. Pair index is shared with the stable host.
    std::array<ID3D11PixelShader*,24> lerp_diffuse{};
    std::array<ID3D11PixelShader*,24> lerp_full{};
    std::array<ID3D11PixelShader*,24> lerp_diffuse_ul{};
    std::array<ID3D11PixelShader*,24> lerp_full_ul{};
    std::array<ID3D11PixelShader*,24> lerp_full_spec{};
    std::array<ID3D11PixelShader*,24> lerp_full_ul_spec{};

    // Owner-accepted V9A math on the paired stable P_Metal alternates 9/10/11.
    std::array<ID3D11PixelShader*,24> v9a_full{};
    std::array<ID3D11PixelShader*,24> v9a_full_ul{};
    std::array<ID3D11PixelShader*,24> v9a_full_spec{};
    std::array<ID3D11PixelShader*,24> v9a_full_ul_spec{};

    std::unordered_map<std::uint16_t,ID3D11Buffer*> b12;
};
device_state g_device;
std::mutex g_device_mutex;

void release_device_state()
{
    std::lock_guard lock(g_device_mutex);
    for(auto *&p:g_device.diffuse){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.diffuse_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full_ul_spec){ if(p){p->Release();p=nullptr;} }

    for(auto *&p:g_device.lerp_diffuse){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_full){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_diffuse_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_full_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_full_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_full_ul_spec){ if(p){p->Release();p=nullptr;} }

    for(auto *&p:g_device.v9a_full){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.v9a_full_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.v9a_full_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.v9a_full_ul_spec){ if(p){p->Release();p=nullptr;} }

    for(auto &[_,b]:g_device.b12) if(b) b->Release();
    g_device.b12.clear();
    if(g_device.device){g_device.device->Release();g_device.device=nullptr;}
}

const shader_desc *find_ps(std::uint32_t count,const pipeline_subobject *sub) noexcept
{
    if(!sub) return nullptr;
    for(std::uint32_t i=0;i<count;++i)
        if(sub[i].type==pipeline_subobject_type::pixel_shader && sub[i].count==1 && sub[i].data)
            return static_cast<const shader_desc*>(sub[i].data);
    return nullptr;
}

bool create_shader(
    ID3D11Device *native,
    const transform_result &code,
    ID3D11PixelShader *&out) noexcept
{
    if(!code.ok || code.code.empty()) return false;
    return SUCCEEDED(native->CreatePixelShader(
        code.code.data(),code.code.size(),nullptr,&out)) && out!=nullptr;
}

bool ensure_shader_pair(device *d,const plan &p,std::span<const std::uint8_t> stock)
{
    auto *native=reinterpret_cast<ID3D11Device*>(d->get_native());
    if(!native) return false;

    std::lock_guard lock(g_device_mutex);
    if(g_device.device!=native) return false;
    const auto i=static_cast<std::size_t>(p.index);
    if(g_device.diffuse[i] && g_device.full[i]) return true;

    const auto diffuse=transform(stock,p,variant::diffuse_v29);
    const auto full=transform(stock,p,variant::full_v211);
    if(!diffuse.ok || !full.ok){
        ++g_shader_pair_fail;
        return false;
    }

    ID3D11PixelShader *ps_diffuse=nullptr,*ps_full=nullptr;
    if(!create_shader(native,diffuse,ps_diffuse)){
        ++g_shader_pair_fail; return false;
    }
    if(!create_shader(native,full,ps_full)){
        ps_diffuse->Release(); ++g_shader_pair_fail; return false;
    }

    if(g_device.diffuse[i]) g_device.diffuse[i]->Release();
    if(g_device.full[i]) g_device.full[i]->Release();
    g_device.diffuse[i]=ps_diffuse;
    g_device.full[i]=ps_full;
    ++g_shader_pair_pass;

    // U/L is compositional over the exact V29/V2.11 variants. The historical
    // V29 -> U/L output hash is pinned for all 24 stable hosts.
    const auto &ul_expected=generated::k_ul_stable_hashes[i];
    const auto diffuse_sha=dsrrl::to_hex(dsrrl::sha256({
        reinterpret_cast<const std::byte*>(diffuse.code.data()),diffuse.code.size()
    }));
    if(diffuse_sha==ul_expected.v29){
        const auto diffuse_ul=transform_upper_lower(diffuse.code,ul_expected.v29_ul);
        const auto full_ul=transform_upper_lower(full.code);
        ID3D11PixelShader *ps_diffuse_ul=nullptr,*ps_full_ul=nullptr;
        if(create_shader(native,diffuse_ul,ps_diffuse_ul) &&
           create_shader(native,full_ul,ps_full_ul)){
            if(g_device.diffuse_ul[i]) g_device.diffuse_ul[i]->Release();
            if(g_device.full_ul[i]) g_device.full_ul[i]->Release();
            g_device.diffuse_ul[i]=ps_diffuse_ul;
            g_device.full_ul[i]=ps_full_ul;
            ++g_shader_ul_pass;
        }else{
            if(ps_diffuse_ul) ps_diffuse_ul->Release();
            if(ps_full_ul) ps_full_ul->Release();
            ++g_shader_ul_fail;
        }

        // The combined U/L + SpecRGB variant is built from the already exact
        // U/L V2.11 payload so both operator islands share one pixel shader.
        if(full_ul.ok){
            const auto ul_spec=transform_spec_rgb(full_ul.code);
            ID3D11PixelShader *ps_ul_spec=nullptr;
            if(create_shader(native,ul_spec,ps_ul_spec)){
                if(g_device.full_ul_spec[i]) g_device.full_ul_spec[i]->Release();
                g_device.full_ul_spec[i]=ps_ul_spec;
                ++g_shader_ul_spec_pass;
            }else{
                if(ps_ul_spec) ps_ul_spec->Release();
                ++g_shader_ul_spec_fail;
            }
        }else{
            ++g_shader_ul_spec_fail;
        }
    }else{
        ++g_shader_ul_fail;
        ++g_shader_ul_spec_fail;
    }

    const auto spec=transform_spec_rgb(full.code);
    ID3D11PixelShader *ps_spec=nullptr;
    if(create_shader(native,spec,ps_spec)){
        if(g_device.full_spec[i]) g_device.full_spec[i]->Release();
        g_device.full_spec[i]=ps_spec;
        ++g_shader_spec_pass;
    }else{
        if(ps_spec) ps_spec->Release();
        ++g_shader_spec_fail;
    }

    return true;
}

ID3D11Buffer *realize_b12(ID3D11Device *device,int donor_index) noexcept
{
    if(!device || donor_index<0 ||
       static_cast<std::size_t>(donor_index)>=dsrrl::materialdonor::k_donors.size())
        return nullptr;

    ID3D11Buffer *buffer=nullptr;
    try {
        std::lock_guard lock(g_device_mutex);
        if(g_device.device!=device) return nullptr;

        const auto key=static_cast<std::uint16_t>(donor_index);
        if(const auto it=g_device.b12.find(key);it!=g_device.b12.end() && it->second){
            it->second->AddRef(); ++g_b12_hit; return it->second;
        }

        const auto &d=dsrrl::materialdonor::k_donors[static_cast<std::size_t>(donor_index)];
        struct f4{float x,y,z,w;};
        const std::array<f4,4> payload={{
            {d.c101_f0q[0],d.c101_f0q[1],d.c101_f0q[2],d.has_c101?1.0f:0.0f},
            {d.c100[0],d.c100[1],d.c100[2],1.0f},
            {0,0,0,0},{0,0,0,0}
        }};
        D3D11_BUFFER_DESC desc{}; desc.ByteWidth=64; desc.Usage=D3D11_USAGE_IMMUTABLE;
        desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA init{}; init.pSysMem=payload.data();
        if(FAILED(device->CreateBuffer(&desc,&init,&buffer)) || !buffer) return nullptr;

        g_device.b12.emplace(key,buffer);
        buffer->AddRef();
        ++g_b12_create;
        return buffer;
    } catch (...) {
        if(buffer) buffer->Release();
        return nullptr;
    }
}

struct cb_capture {
    ID3D11Buffer *base=nullptr,*window=nullptr;
    UINT first=0,count=0;
    bool explicit_window=false;
    bool coherent=true;
};

cb_capture capture_cb(ID3D11DeviceContext *ctx,ID3D11DeviceContext1 *ctx1)
{
    cb_capture out{};
    ctx->PSGetConstantBuffers(12,1,&out.base);
    if(ctx1){
        ctx1->PSGetConstantBuffers1(12,1,&out.window,&out.first,&out.count);
        out.coherent=out.base==out.window;
        out.explicit_window=out.window && out.count>=16 && (out.first%16)==0 && (out.count%16)==0;
    }
    return out;
}
void release_cb(cb_capture &x){if(x.base)x.base->Release();if(x.window)x.window->Release();x={};}
void restore_cb(ID3D11DeviceContext *ctx,ID3D11DeviceContext1 *ctx1,const cb_capture &x)
{
    if(ctx1 && x.explicit_window){
        ID3D11Buffer *b=x.window; UINT f=x.first,n=x.count;
        ctx1->PSSetConstantBuffers1(12,1,&b,&f,&n);
    }else{
        ID3D11Buffer *b=x.base; ctx->PSSetConstantBuffers(12,1,&b);
    }
}
bool verify_restore(ID3D11DeviceContext *ctx,ID3D11DeviceContext1 *ctx1,const cb_capture &x)
{
    ID3D11Buffer *base=nullptr; ctx->PSGetConstantBuffers(12,1,&base);
    bool ok=base==x.base; if(base)base->Release();
    if(ctx1){
        ID3D11Buffer *win=nullptr;UINT f=0,n=0;
        ctx1->PSGetConstantBuffers1(12,1,&win,&f,&n);
        ok=ok && win==x.window && (!x.explicit_window || (f==x.first && n==x.count));
        if(win)win->Release();
    }
    return ok;
}

void on_init_device(device *d)
{
    if(!d || d->get_api()!=device_api::d3d11) return;
    release_device_state();
    auto *native=reinterpret_cast<ID3D11Device*>(d->get_native());
    if(!native) return;
    std::lock_guard lock(g_device_mutex);
    g_device.device=native; native->AddRef();
}

void on_destroy_device(device *d)
{
    if(!d || d->get_api()!=device_api::d3d11) return;
    auto *native=reinterpret_cast<ID3D11Device*>(d->get_native());
    {
        std::lock_guard lock(g_device_mutex);
        if(g_device.device!=native) return;
    }
    release_device_state();
    {
        std::lock_guard lock(g_pending_mutex);
        g_pending.clear();
    }
    {
        std::lock_guard lock(g_pipeline_mutex);
        g_pipelines.clear();
    }
}

bool on_create_pipeline(device *d,pipeline_layout,std::uint32_t count,const pipeline_subobject *sub)
{
    try {
        if(!g_enabled.load() || g_quarantined.load() || !d || d->get_api()!=device_api::d3d11) return false;
        const auto *ps=find_ps(count,sub);
        if(!ps || !ps->code || !ps->code_size) return false;
        ++g_pipeline_seen;

        const auto *bytes=static_cast<const std::uint8_t*>(ps->code);
        const auto sha=dsrrl::to_hex(dsrrl::sha256({
            reinterpret_cast<const std::byte*>(bytes),ps->code_size
        }));
        // Record the exact Subsurf source independently of target creation
        // order. Ordinary shaders are materialized once when DSR creates them.
        const int body_target=subsurface_target(sha);
        if(body_target>=0){
            std::lock_guard lock(g_pending_mutex);
            g_pending.push_back({ps->code_size,sha,static_cast<std::uint8_t>(body_target)});
            return false;
        }
        const auto *p=find_plan(ps->code_size,sha);
        if(!p) return false;

        if(!ensure_shader_pair(d,*p,{bytes,ps->code_size})){
            ++g_fail_open; g_quarantined.store(true);
            log_error("DSRRL Runtime v1 MR: exact V2.11 shader transform failed; MR quarantined.");
            return false;
        }

        {
            std::lock_guard lock(g_pending_mutex);
            g_pending.push_back({ps->code_size,sha,p->index});
        }
        return false;
    } catch (...) {
        ++g_fail_open;
        g_quarantined.store(true);
        reshade::log::message(
            reshade::log::level::error,
            "DSRRL Runtime v1 MR: create_pipeline exception; MR quarantined fail-open.");
        return false;
    }
}

void on_init_pipeline(device *d,pipeline_layout,std::uint32_t count,const pipeline_subobject *sub,pipeline p)
{
    try {
        if(!d || d->get_api()!=device_api::d3d11 || !p.handle) return;
        const auto *ps=find_ps(count,sub);
        if(!ps || !ps->code || !ps->code_size) return;

        const auto sha=dsrrl::to_hex(dsrrl::sha256({
            reinterpret_cast<const std::byte*>(ps->code),ps->code_size
        }));

        // Do not depend on callback-local shader_desc object identity. The API
        // may present a different descriptor object at init_pipeline or dispatch
        // the callback from another worker thread. Correlate by certified full
        // source SHA-256 + size.
        std::uint8_t host=0;
        bool matched=false;
        {
            std::lock_guard lock(g_pending_mutex);
            for(auto it=g_pending.begin();it!=g_pending.end();++it){
                if(ps->code_size!=it->size || sha!=it->sha)
                    continue;
                host=it->host;
                g_pending.erase(it);
                matched=true;
                break;
            }
        }
        if(matched){
            std::lock_guard lock(g_pipeline_mutex);
            g_pipelines[p.handle]=host;
        }
    } catch (...) {
        ++g_fail_open;
        g_quarantined.store(true);
        reshade::log::message(
            reshade::log::level::error,
            "DSRRL Runtime v1 MR: init_pipeline exception; MR quarantined fail-open.");
    }
}

void on_destroy_pipeline(device *,pipeline p)
{
    std::lock_guard lock(g_pipeline_mutex);
    g_pipelines.erase(p.handle);
}

void on_bind_pipeline(command_list *cmd,pipeline_stage stages,pipeline p)
{
    if(!cmd || (static_cast<std::uint32_t>(stages)&static_cast<std::uint32_t>(pipeline_stage::pixel_shader))==0)
        return;
    int host=-1;
    {
        std::lock_guard lock(g_pipeline_mutex);
        if(const auto it=g_pipelines.find(p.handle);it!=g_pipelines.end()) host=it->second;
    }
    g_bound_subsurface=host>=33 && host<=35;
    g_bound_host=g_bound_subsurface ? host-24 : host;
    g_bound_command=host>=0?cmd:nullptr;
    if(host>=0) ++g_target_binds;
    else {
        g_draw_donor=-1;
        upper_lower::consume_draw_selection();
    }
}

bool on_draw_indexed(command_list *cmd,std::uint32_t index_count,std::uint32_t instance_count,
                     std::uint32_t first_index,std::int32_t vertex_offset,std::uint32_t first_instance)
{
    const bool body_material=g_draw_donor==k_subsurface_material;
    const bool body_route=g_bound_subsurface && body_material;
    const int donor=body_route ? dsrrl::materialdonor::find_sha256(
        subsurface::k_ptde_body_plain_material_sha256) : g_draw_donor;
    g_draw_donor=-1;
    if(g_bound_subsurface != body_material ||
       (body_route && (!g_core ||
        !g_core->features().enabled(core::operator_id::subsurface)))){
        upper_lower::consume_draw_selection();
        return false;
    }

    if(!g_core || !g_core->features().enabled(core::operator_id::material_response) ||
       !g_enabled.load() || g_quarantined.load() || !cmd || cmd!=g_bound_command ||
       g_bound_host<0 || g_bound_host>=24 || donor<0 ||
       static_cast<std::size_t>(donor)>=dsrrl::materialdonor::k_donors.size()){
        upper_lower::consume_draw_selection();
        return false;
    }

    auto *ctx=reinterpret_cast<ID3D11DeviceContext*>(cmd->get_native());
    if(!ctx){
        ++g_fail_open;
        upper_lower::consume_draw_selection();
        return false;
    }

    if(body_route && !assets::body_surface_ready(ctx)){
        ++g_fail_open;
        upper_lower::consume_draw_selection();
        return false;
    }

    const auto &don=dsrrl::materialdonor::k_donors[static_cast<std::size_t>(donor)];
    const std::uint32_t receiver_id=24u+static_cast<std::uint32_t>(g_bound_host);
    assets::material_route_scope route{};
    route.exact=true;
    route.diffuse_normal_eligible=g_bound_host<12;
    route.diffuse_c100_carrier_active=true;
    route.specular_material_verified=
        don.has_c101 &&
        generated::spec_material_route_allowed(don.sha256,receiver_id);
    route.route_index=static_cast<std::uint32_t>(donor);
    route.receivers={receiver_id,0u,0u};

    const bool ul_candidate =
        g_core->features().enabled(core::operator_id::upper_lower) &&
        upper_lower::selected_snapshot_ready();
    const bool spec_candidate =
        route.specular_material_verified &&
        g_core->features().enabled(core::operator_id::spec_rgb) &&
        assets::spec_ready(ctx,route,receiver_id);

    bool issued=false;
    bool restore_ok=true;
    bool intended_ul=false;
    bool spec_active=false;
    bool tx_started=false;
    std::uint64_t command=0;

    ID3D11Device *dev=nullptr;
    ctx->GetDevice(&dev);
    ID3D11PixelShader *oldps=nullptr,*replacement=nullptr;
    ID3D11Buffer *b12=nullptr;
    ID3D11DeviceContext1 *ctx1=nullptr;
    cb_capture oldcb{};
    assets::draw_state asset_state{};
    upper_lower::draw_state ul_state{};

    std::array<ID3D11ClassInstance*,256> old_classes{};
    UINT old_class_count=static_cast<UINT>(old_classes.size());
    bool captured=false;
    if(dev){
        ctx->PSGetShader(&oldps,old_classes.data(),&old_class_count);
        captured=true;
        b12=realize_b12(dev,donor);
        ctx->QueryInterface(__uuidof(ID3D11DeviceContext1),reinterpret_cast<void**>(&ctx1));
        oldcb=capture_cb(ctx,ctx1);

        {
            std::lock_guard lock(g_device_mutex);
            if(g_device.device==dev){
                const auto i=static_cast<std::size_t>(g_bound_host);
                ID3D11PixelShader *ul_shader =
                    don.has_c101 ? g_device.full_ul[i] : g_device.diffuse_ul[i];
                intended_ul=ul_candidate && ul_shader!=nullptr;

                if(spec_candidate){
                    ID3D11PixelShader *spec_shader =
                        intended_ul ? g_device.full_ul_spec[i] : g_device.full_spec[i];
                    spec_active=spec_shader!=nullptr;
                }

                if(don.has_c101){
                    replacement =
                        intended_ul && spec_active ? g_device.full_ul_spec[i] :
                        intended_ul ? g_device.full_ul[i] :
                        spec_active ? g_device.full_spec[i] :
                                      g_device.full[i];
                }else{
                    replacement=intended_ul ? g_device.diffuse_ul[i] : g_device.diffuse[i];
                }
                if(replacement) replacement->AddRef();
            }
        }

        // Exact stock DXBC has no dynamic class linkage. Unknown linkage
        // fails open, and all captured class references are released below.
        if(oldps && replacement && b12 && oldcb.coherent && old_class_count==0 &&
           (!body_route || subsurface_dispatch_ready(static_cast<int>(receiver_id),
            body_material,g_core->features().enabled(core::operator_id::subsurface),true,spec_active))){
            core::render_patch_plan plan{};
            plan.patches[plan.patch_count++]={core::operator_id::material_response,0u,true,false};
            if(g_core->features().enabled(core::operator_id::diffuse))
                plan.patches[plan.patch_count++]={core::operator_id::diffuse,0u,false,true};
            if(g_core->features().enabled(core::operator_id::normal))
                plan.patches[plan.patch_count++]={core::operator_id::normal,0u,false,true};
            if(body_route)
                plan.patches[plan.patch_count++]={core::operator_id::subsurface,0u,true,false};
            if(spec_active)
                plan.patches[plan.patch_count++]={core::operator_id::spec_rgb,0u,true,true};
            if(intended_ul)
                plan.patches[plan.patch_count++]={
                    core::operator_id::upper_lower,
                    core::carrier_ul_mask,
                    true,
                    false};
            plan.carrier_write_mask =
                intended_ul ? core::carrier_ul_mask : 0u;

            const auto type=ctx->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED ?
                core::context_kind::deferred : core::context_kind::immediate;
            command=reinterpret_cast<std::uint64_t>(cmd);
            tx_started=g_core->transactions().begin(command,++g_draw_serial,type,plan);

            if(tx_started){
                bool ul_bound=true;
                if(intended_ul)
                    ul_bound=upper_lower::bind_draw(ctx,ul_state);

                if(ul_bound){
                    ctx->PSSetShader(replacement,nullptr,0);
                    ctx->PSSetConstantBuffers(12,1,&b12);
                    route.spec_t10_consumer_active=spec_active;

                    const bool assets_ok=assets::apply_draw(ctx,route,receiver_id,asset_state);
                    // Subsurf bypass is all-or-nothing: never drop SSS if a
                    // required ordinary PTDE surface dependency failed to bind.
                    if(assets_ok && (!body_route ||
                       (asset_state.changed_t0 && asset_state.changed_t2 && asset_state.changed_t10))){
                        const auto replay =
                            choose_indexed_replay(
                                instance_count,
                                first_instance);
                        if(replay==indexed_replay_kind::draw_indexed)
                            ctx->DrawIndexed(
                                index_count,
                                first_index,
                                vertex_offset);
                        else
                            ctx->DrawIndexedInstanced(
                                index_count,
                                instance_count,
                                first_index,
                                vertex_offset,
                                first_instance);
                        issued=true;
                    }
                }else{
                    ++g_fail_open;
                }

                const bool assets_restored=assets::restore_draw(ctx,asset_state);
                const bool ul_restored=upper_lower::restore_draw(ctx,ul_state);
                ctx->PSSetShader(oldps,old_classes.data(),old_class_count);
                restore_cb(ctx,ctx1,oldcb);
                const bool mr_restored=verify_restore(ctx,ctx1,oldcb);
                restore_ok=assets_restored && ul_restored && mr_restored;

                const bool tx_restored=g_core->transactions().restore(command);
                tx_started=false;
                if(!tx_restored || !restore_ok){
                    ++g_restore_fail;
                    g_quarantined.store(true);
                    log_error("DSRRL Runtime v1 MR: unified draw restore fault; MR/SpecRGB/U/L transaction quarantined.");
                }
            }else{
                ++g_fail_open;
            }
        }
    }

    // No mutation is allowed outside the transaction. If no native draw was
    // issued, every locally captured state is restored and ReShade executes the
    // original stock draw exactly once.
    if(tx_started){
        (void)g_core->transactions().restore(command);
        tx_started=false;
    }
    if(!issued){
        ++g_fail_open;
        (void)assets::restore_draw(ctx,asset_state);
        (void)upper_lower::restore_draw(ctx,ul_state);
        if(captured){
            ctx->PSSetShader(oldps,old_classes.data(),old_class_count);
            restore_cb(ctx,ctx1,oldcb);
        }
    }else{
        ++g_replays;
        if(body_route) ++g_subsurface_replays;
    }

    for(auto *instance:old_classes) if(instance) instance->Release();
    upper_lower::consume_draw_selection();
    release_cb(oldcb);
    if(ctx1)ctx1->Release();
    if(b12)b12->Release();
    if(replacement)replacement->Release();
    if(oldps)oldps->Release();
    if(dev)dev->Release();

    return issued;
}
void on_present(command_queue *,swapchain *,const rect *,const rect *,std::uint32_t,const rect *)
{
    try {
        const auto n=++g_present;
        if(n==300 || (n>300 && (n%1200)==0)){
            std::ostringstream os;
            os<<"DSRRL Runtime v1 MR: present="<<n
              <<" MTD="<<g_mtd_seen.load()<<" mapped="<<g_mapped.load()<<" unmapped="<<g_unmapped.load()
              <<" selector="<<g_selector_seen.load()<<" selector_mapped="<<g_selector_mapped.load()
              <<" pipelines="<<g_pipeline_seen.load()<<" shader_pair_pass="<<g_shader_pair_pass.load()
              <<" shader_pair_fail="<<g_shader_pair_fail.load()
              <<" ul_shader_pass="<<g_shader_ul_pass.load()<<" ul_shader_fail="<<g_shader_ul_fail.load()
              <<" spec_shader_pass="<<g_shader_spec_pass.load()<<" spec_shader_fail="<<g_shader_spec_fail.load()
              <<" ul_spec_shader_pass="<<g_shader_ul_spec_pass.load()<<" ul_spec_shader_fail="<<g_shader_ul_spec_fail.load()
              <<" binds="<<g_target_binds.load()
              <<" replay="<<g_replays.load()<<" subsurface_replay="<<g_subsurface_replays.load()<<" b12_create="<<g_b12_create.load()
              <<" b12_hit="<<g_b12_hit.load()<<" failopen="<<g_fail_open.load()
              <<" restore_fail="<<g_restore_fail.load()<<" quarantined="<<(g_quarantined.load()?1:0);
            log_info(os.str());
        }
    } catch (...) {
        // Telemetry is non-authoritative and must never escape the callback ABI.
    }
}

} // namespace

void mtd_event(void *material,const void *raw,std::uint32_t len) noexcept
{
    if(!material || !raw || !len) return;
    ++g_mtd_seen;
    try{
        const auto hash=dsrrl::to_hex(dsrrl::sha256({
            reinterpret_cast<const std::byte*>(raw),len
        }));
        const int idx=hash==subsurface::k_dsr_body_subsurf_material_sha256 ?
            k_subsurface_material : dsrrl::materialdonor::find_sha256(hash);
        std::lock_guard lock(g_material_mutex);
        g_material_donor.erase(material);
        if(idx>=0){
            g_material_donor[material]=static_cast<std::uint16_t>(idx); ++g_mapped;
        }else ++g_unmapped;
    }catch(...){++g_fail_open;}
}

void selector_event(void *container,void *,void *ret,void *,void *,std::int32_t material_index) noexcept
{
    ++g_selector_seen;
    const auto base=engine::image_base();
    if(!base){g_draw_donor=-1;return;}
    const auto rva=reinterpret_cast<std::uintptr_t>(ret)-base;
    if(rva!=k_ret_sel_1 && rva!=k_ret_sel_2 && rva!=k_ret_sel_3){
        g_draw_donor=-1; return;
    }
    g_draw_donor=donor_for(resolve_material(container,material_index));
    if(g_draw_donor>=0) ++g_selector_mapped;
}

bool register_runtime(core::renderer_core &core) noexcept
{
    g_core=&core;
    g_quarantined.store(false);
    g_draw_donor=-1; g_bound_host=-1; g_bound_subsurface=false; g_bound_command=nullptr;
    g_enabled.store(true);
    reshade::register_event<reshade::addon_event::init_device>(on_init_device);
    reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::register_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
    reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
    reshade::register_event<reshade::addon_event::destroy_pipeline>(on_destroy_pipeline);
    reshade::register_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
    reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
    reshade::register_event<reshade::addon_event::present>(on_present);
    return true;
}

void unregister_runtime() noexcept
{
    g_enabled.store(false);
    reshade::unregister_event<reshade::addon_event::present>(on_present);
    reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
    reshade::unregister_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
    reshade::unregister_event<reshade::addon_event::destroy_pipeline>(on_destroy_pipeline);
    reshade::unregister_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
    reshade::unregister_event<reshade::addon_event::create_pipeline>(on_create_pipeline);
    reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
    release_device_state();
    {std::lock_guard lock(g_pending_mutex);g_pending.clear();}
    {std::lock_guard lock(g_pipeline_mutex);g_pipelines.clear();}
    {std::lock_guard lock(g_material_mutex);g_material_donor.clear();}
    g_draw_donor=-1; g_bound_host=-1; g_bound_subsurface=false; g_bound_command=nullptr;
    g_core=nullptr;
}

} // namespace dsrrl::runtime::mr
