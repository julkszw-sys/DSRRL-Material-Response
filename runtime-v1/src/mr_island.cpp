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
#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/sha256.hpp"
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
std::atomic<std::uint64_t> g_spec_shader_pair_pass{0}, g_spec_shader_pair_fail{0};
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
thread_local const command_list *g_bound_command=nullptr;

struct pending_pipeline {
    const shader_desc *descriptor=nullptr;
    std::size_t size=0;
    std::string sha;
    std::uint8_t host=0;
};
thread_local std::vector<pending_pipeline> g_pending;

std::mutex g_pipeline_mutex;
std::unordered_map<std::uint64_t,std::uint8_t> g_pipelines;

struct device_state {
    ID3D11Device *device=nullptr;
    std::array<ID3D11PixelShader*,24> diffuse{};
    std::array<ID3D11PixelShader*,24> full{};
    std::array<ID3D11PixelShader*,24> diffuse_specrgb{};
    std::array<ID3D11PixelShader*,24> full_specrgb{};
    std::unordered_map<std::uint16_t,ID3D11Buffer*> b12;
};
device_state g_device;
std::mutex g_device_mutex;

void release_device_state()
{
    std::lock_guard lock(g_device_mutex);
    for(auto *&p:g_device.diffuse){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.diffuse_specrgb){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full_specrgb){ if(p){p->Release();p=nullptr;} }
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

bool ensure_shader_pair(device *d,const plan &p,std::span<const std::uint8_t> stock)
{
    auto *native=reinterpret_cast<ID3D11Device*>(d->get_native());
    if(!native) return false;

    std::lock_guard lock(g_device_mutex);
    if(g_device.device!=native) return false;
    const auto i=static_cast<std::size_t>(p.index);
    if(g_device.diffuse[i] && g_device.full[i] &&
       g_device.diffuse_specrgb[i] && g_device.full_specrgb[i])
        return true;

    const auto diffuse=transform(stock,p,variant::diffuse_v29);
    const auto full=transform(stock,p,variant::full_v211);
    if(!diffuse.ok || !full.ok){
        ++g_shader_pair_fail;
        return false;
    }

    const auto diffuse_spec=transform(stock,p,variant::diffuse_v29_specrgb);
    const auto full_spec=transform(stock,p,variant::full_v211_specrgb);
    if(!diffuse_spec.ok || !full_spec.ok){
        ++g_spec_shader_pair_fail;
        return false;
    }

    ID3D11PixelShader *ps_diffuse=nullptr,*ps_full=nullptr;
    ID3D11PixelShader *ps_diffuse_spec=nullptr,*ps_full_spec=nullptr;

    auto release_local=[&]() noexcept {
        if(ps_diffuse){ps_diffuse->Release();ps_diffuse=nullptr;}
        if(ps_full){ps_full->Release();ps_full=nullptr;}
        if(ps_diffuse_spec){ps_diffuse_spec->Release();ps_diffuse_spec=nullptr;}
        if(ps_full_spec){ps_full_spec->Release();ps_full_spec=nullptr;}
    };

    if(FAILED(native->CreatePixelShader(diffuse.code.data(),diffuse.code.size(),nullptr,&ps_diffuse)) || !ps_diffuse ||
       FAILED(native->CreatePixelShader(full.code.data(),full.code.size(),nullptr,&ps_full)) || !ps_full){
        release_local(); ++g_shader_pair_fail; return false;
    }

    if(FAILED(native->CreatePixelShader(diffuse_spec.code.data(),diffuse_spec.code.size(),nullptr,&ps_diffuse_spec)) || !ps_diffuse_spec ||
       FAILED(native->CreatePixelShader(full_spec.code.data(),full_spec.code.size(),nullptr,&ps_full_spec)) || !ps_full_spec){
        release_local(); ++g_spec_shader_pair_fail; return false;
    }

    if(g_device.diffuse[i]) g_device.diffuse[i]->Release();
    if(g_device.full[i]) g_device.full[i]->Release();
    if(g_device.diffuse_specrgb[i]) g_device.diffuse_specrgb[i]->Release();
    if(g_device.full_specrgb[i]) g_device.full_specrgb[i]->Release();

    g_device.diffuse[i]=ps_diffuse;
    g_device.full[i]=ps_full;
    g_device.diffuse_specrgb[i]=ps_diffuse_spec;
    g_device.full_specrgb[i]=ps_full_spec;
    ++g_shader_pair_pass;
    ++g_spec_shader_pair_pass;
    return true;
}

ID3D11Buffer *realize_b12(ID3D11Device *device,int donor_index)
{
    if(!device || donor_index<0 ||
       static_cast<std::size_t>(donor_index)>=dsrrl::materialdonor::k_donors.size())
        return nullptr;

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
    ID3D11Buffer *buffer=nullptr;
    if(FAILED(device->CreateBuffer(&desc,&init,&buffer)) || !buffer) return nullptr;
    g_device.b12.emplace(key,buffer);
    buffer->AddRef(); ++g_b12_create; return buffer;
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
}

bool on_create_pipeline(device *d,pipeline_layout,std::uint32_t count,const pipeline_subobject *sub)
{
    if(!g_enabled.load() || g_quarantined.load() || !d || d->get_api()!=device_api::d3d11) return false;
    const auto *ps=find_ps(count,sub);
    if(!ps || !ps->code || !ps->code_size) return false;
    ++g_pipeline_seen;

    const auto *bytes=static_cast<const std::uint8_t*>(ps->code);
    const auto sha=dsrrl::to_hex(dsrrl::sha256({
        reinterpret_cast<const std::byte*>(bytes),ps->code_size
    }));
    const auto *p=find_plan(ps->code_size,sha);
    if(!p) return false;

    if(!ensure_shader_pair(d,*p,{bytes,ps->code_size})){
        ++g_fail_open; g_quarantined.store(true);
        log_error("DSRRL Runtime v1 MR: exact V2.11 shader transform failed; MR quarantined.");
        return false;
    }

    g_pending.push_back({ps,ps->code_size,sha,p->index});
    return false;
}

void on_init_pipeline(device *d,pipeline_layout,std::uint32_t count,const pipeline_subobject *sub,pipeline p)
{
    if(!d || d->get_api()!=device_api::d3d11 || !p.handle) return;
    const auto *ps=find_ps(count,sub); if(!ps) return;
    for(auto it=g_pending.begin();it!=g_pending.end();++it){
        if(it->descriptor!=ps) continue;
        const auto sha=dsrrl::to_hex(dsrrl::sha256({
            reinterpret_cast<const std::byte*>(ps->code),ps->code_size
        }));
        if(ps->code_size==it->size && sha==it->sha){
            std::lock_guard lock(g_pipeline_mutex);
            g_pipelines[p.handle]=it->host;
        }else{
            g_quarantined.store(true); ++g_fail_open;
            log_error("DSRRL Runtime v1 MR: create/init pipeline attestation mismatch.");
        }
        g_pending.erase(it); break;
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
    g_bound_host=host; g_bound_command=host>=0?cmd:nullptr;
    if(host>=0) ++g_target_binds; else g_draw_donor=-1;
}

bool on_draw_indexed(command_list *cmd,std::uint32_t index_count,std::uint32_t instance_count,
                     std::uint32_t first_index,std::int32_t vertex_offset,std::uint32_t first_instance)
{
    const int donor=g_draw_donor;
    g_draw_donor=-1;

    if(!g_core || !g_core->features().enabled(core::operator_id::material_response) ||
       !g_enabled.load() || g_quarantined.load() || !cmd || cmd!=g_bound_command ||
       g_bound_host<0 || g_bound_host>=24 || donor<0 ||
       static_cast<std::size_t>(donor)>=dsrrl::materialdonor::k_donors.size())
        return false;

    auto *ctx=reinterpret_cast<ID3D11DeviceContext*>(cmd->get_native());
    if(!ctx){++g_fail_open;return false;}

    core::render_patch_plan plan{};
    plan.patches[plan.patch_count++]={core::operator_id::material_response,0u,true,false};
    if(g_core->features().enabled(core::operator_id::spec_rgb))
        plan.patches[plan.patch_count++]={core::operator_id::spec_rgb,0u,false,true};
    if(g_core->features().enabled(core::operator_id::diffuse))
        plan.patches[plan.patch_count++]={core::operator_id::diffuse,0u,false,true};
    if(g_core->features().enabled(core::operator_id::normal))
        plan.patches[plan.patch_count++]={core::operator_id::normal,0u,false,true};
    plan.carrier_write_mask=0;
    const auto type=ctx->GetType()==D3D11_DEVICE_CONTEXT_DEFERRED ?
        core::context_kind::deferred : core::context_kind::immediate;
    const auto command=reinterpret_cast<std::uint64_t>(cmd);
    if(!g_core->transactions().begin(command,++g_draw_serial,type,plan)){
        ++g_fail_open; return false;
    }

    bool issued=false;
    bool restore_ok=true;
    ID3D11Device *dev=nullptr;
    ctx->GetDevice(&dev);
    ID3D11PixelShader *oldps=nullptr,*replacement=nullptr;
    ID3D11Buffer *b12=nullptr;
    ID3D11DeviceContext1 *ctx1=nullptr;
    cb_capture oldcb{};
    assets::draw_state asset_state{};

    if(dev){
        ctx->PSGetShader(&oldps,nullptr,nullptr);
        const auto &don=dsrrl::materialdonor::k_donors[static_cast<std::size_t>(donor)];
        {
            std::lock_guard lock(g_device_mutex);
            if(g_device.device==dev){
                const auto host=static_cast<std::size_t>(g_bound_host);
                const bool spec_enabled=g_core->features().enabled(core::operator_id::spec_rgb);
                if(spec_enabled)
                    replacement=don.has_c101?g_device.full_specrgb[host]:
                                              g_device.diffuse_specrgb[host];
                else
                    replacement=don.has_c101?g_device.full[host]:
                                              g_device.diffuse[host];
                if(replacement) replacement->AddRef();
            }
        }

        b12=realize_b12(dev,donor);
        ctx->QueryInterface(__uuidof(ID3D11DeviceContext1),reinterpret_cast<void**>(&ctx1));
        oldcb=capture_cb(ctx,ctx1);

        if(oldps && replacement && b12 && oldcb.coherent){
            ctx->PSSetShader(replacement,nullptr,0);
            ctx->PSSetConstantBuffers(12,1,&b12);

            const std::uint32_t receiver_id=24u+static_cast<std::uint32_t>(g_bound_host);
            assets::material_route_scope route{};
            route.exact=true;
            route.diffuse_normal_eligible=g_bound_host<12;
            route.diffuse_c100_carrier_active=true;
            route.route_index=static_cast<std::uint32_t>(donor);
            route.receivers={receiver_id,0u,0u};

            const bool assets_ok=assets::apply_draw(ctx,route,receiver_id,asset_state);
            if(assets_ok){
                if(instance_count<=1)
                    ctx->DrawIndexed(index_count,first_index,vertex_offset);
                else
                    ctx->DrawIndexedInstanced(index_count,instance_count,first_index,vertex_offset,first_instance);
                issued=true;
            }

            const bool assets_restored=assets::restore_draw(ctx,asset_state);
            ctx->PSSetShader(oldps,nullptr,0);
            restore_cb(ctx,ctx1,oldcb);
            const bool mr_restored=verify_restore(ctx,ctx1,oldcb);
            restore_ok=assets_restored && mr_restored;
        }
    }

    // If the draw was not issued, restore any partially changed state and let
    // ReShade execute the original draw. If it was issued, always consume the
    // event; returning false after a restore fault would duplicate the draw.
    if(!issued){
        ++g_fail_open;
        (void)assets::restore_draw(ctx,asset_state);
        if(oldps) ctx->PSSetShader(oldps,nullptr,0);
        restore_cb(ctx,ctx1,oldcb);
    }else{
        ++g_replays;
    }

    release_cb(oldcb);
    if(ctx1)ctx1->Release();
    if(b12)b12->Release();
    if(replacement)replacement->Release();
    if(oldps)oldps->Release();
    if(dev)dev->Release();

    const bool tx_restored=g_core->transactions().restore(command);
    if(!tx_restored || !restore_ok){
        ++g_restore_fail;
        g_quarantined.store(true);
        log_error("DSRRL Runtime v1 MR: draw restore fault; MR quarantined after issued transaction.");
    }

    return issued;
}
void on_present(command_queue *,swapchain *,const rect *,const rect *,std::uint32_t,const rect *)
{
    const auto n=++g_present;
    if(n==300 || (n>300 && (n%1200)==0)){
        std::ostringstream os;
        os<<"DSRRL Runtime v1 MR: present="<<n
          <<" MTD="<<g_mtd_seen.load()<<" mapped="<<g_mapped.load()<<" unmapped="<<g_unmapped.load()
          <<" selector="<<g_selector_seen.load()<<" selector_mapped="<<g_selector_mapped.load()
          <<" pipelines="<<g_pipeline_seen.load()<<" shader_pair_pass="<<g_shader_pair_pass.load()
          <<" shader_pair_fail="<<g_shader_pair_fail.load()
          <<" spec_shader_pair_pass="<<g_spec_shader_pair_pass.load()
          <<" spec_shader_pair_fail="<<g_spec_shader_pair_fail.load()
          <<" spec_feature="<<(g_core && g_core->features().enabled(core::operator_id::spec_rgb)?1:0)
          <<" binds="<<g_target_binds.load()
          <<" replay="<<g_replays.load()<<" b12_create="<<g_b12_create.load()
          <<" b12_hit="<<g_b12_hit.load()<<" failopen="<<g_fail_open.load()
          <<" restore_fail="<<g_restore_fail.load()<<" quarantined="<<(g_quarantined.load()?1:0);
        log_info(os.str());
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
        const int idx=dsrrl::materialdonor::find_sha256(hash);
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
    {std::lock_guard lock(g_pipeline_mutex);g_pipelines.clear();}
    {std::lock_guard lock(g_material_mutex);g_material_donor.clear();}
    g_core=nullptr;
}

} // namespace dsrrl::runtime::mr
