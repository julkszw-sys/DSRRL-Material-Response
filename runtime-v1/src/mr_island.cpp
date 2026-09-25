#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/mr_island.hpp"
#include "dsrrl/runtime/mr_dxbc_transform.hpp"
#include "dsrrl/runtime/engine_hooks.hpp"
#include "dsrrl/runtime/envspec_runtime.hpp"
#include "dsrrl/runtime/asset_bridges.hpp"
#include "dsrrl/runtime/draw_replay.hpp"
#include "dsrrl/runtime/d3d11_cb_window.hpp"
#include "dsrrl/runtime/upper_lower_runtime.hpp"
#include "dsrrl/runtime/generated_ul_stable_hashes.hpp"
#include "dsrrl/runtime/generated_spec_material_routes.hpp"
#include "semantic_spec_donor_overrides_generated.hpp"
#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
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
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
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
std::atomic<std::uint64_t> g_shader_v13_pass{0}, g_shader_v13_fail{0};
std::atomic<std::uint64_t> g_envspec_rgba_shader_pass{0}, g_envspec_rgba_shader_fail{0};
std::atomic<std::uint64_t> g_lerp_shader_pass{0}, g_lerp_shader_fail{0};
std::atomic<std::uint64_t> g_v9a_shader_pass{0}, g_v9a_shader_fail{0};
std::atomic<std::uint64_t> g_lerp_binds{0}, g_lerp_replays{0}, g_v10_replays{0};
std::atomic<bool> g_v10_first_draw_logged{false};
std::atomic<std::uint64_t> g_v13_source_ready{0}, g_v13_source_miss{0};
std::atomic<std::uint64_t> g_v13_resource_ready{0}, g_v13_resource_miss{0};
std::atomic<std::uint64_t> g_v13_b12_update{0}, g_v13_b12_fail{0}, g_v13_replay{0};
std::atomic<bool> g_v13_first_draw_logged{false};
std::atomic<std::uint64_t> g_envspec_rgba_candidate{0}, g_envspec_rgba_replay{0};
std::atomic<std::uint64_t> g_envspec_rgba_bind_fail{0}, g_envspec_rgba_restore_fail{0};
std::atomic<bool> g_envspec_rgba_first_draw_logged{false};
std::atomic<std::uint64_t> g_target_binds{0}, g_replays{0}, g_fail_open{0};
std::atomic<std::uint64_t> g_diffuse_candidates{0}, g_normal_candidates{0};
std::atomic<std::uint64_t> g_spec_candidates{0}, g_ul_candidates{0};
std::atomic<std::uint64_t> g_full_diffuse_replays{0}, g_stock_diffuse_replays{0};
std::atomic<std::uint64_t> g_normal_replays{0}, g_spec_replays{0}, g_ul_replays{0};
std::atomic<std::uint64_t> g_b12_create{0}, g_b12_hit{0}, g_restore_fail{0}, g_present{0};
std::atomic<std::uint64_t> g_envspec_key_valid{0}, g_envspec_key_fail{0};
std::atomic<std::uint64_t> g_envspec_mtd_exact{0}, g_envspec_selector_exact{0};
std::atomic<std::uint64_t> g_envspec_draw_exact{0}, g_envspec_draw_present{0};
std::atomic<std::uint64_t> g_envspec_draw_none_safe{0}, g_envspec_draw_none_unsafe{0};
std::atomic<std::uint64_t> g_envspec_draw_nospc{0};
std::atomic<std::uint64_t> g_semantic_spec_override_hit{0};
std::atomic<std::uint64_t> g_semantic_spec_ambiguous_hold{0};
std::atomic<std::uint64_t> g_ptde_tex_mtd_exact{0};
std::atomic<std::uint64_t> g_ptde_tex_selector_exact{0};
std::atomic<std::uint64_t> g_diffuse_semantic_hold{0};
std::atomic<std::uint64_t> g_normal_semantic_hold{0};

namespace mtd_sem=dsrrl::operators::material_response;

constexpr std::string_view k_pmetal_raw_mtd_sha256 =
    "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b";
constexpr std::string_view k_nonhomologous_normal_pd_sha256 =
    "e0e263990784d9cc118e2b3afc895f25de9b5780458a796d38447dc55b1ee17e";
constexpr std::string_view k_nonhomologous_normal_pleather_ds_sha256 =
    "53819ead337c1ecd8593d8535c1fc1fdde589f00eb8ea015a8ae347da71073ea";

void log_info(const std::string &s){ reshade::log::message(reshade::log::level::info,s.c_str()); }
void log_error(const std::string &s){ reshade::log::message(reshade::log::level::error,s.c_str()); }

std::mutex g_material_mutex;
std::unordered_map<void*,std::uint16_t> g_material_donor;
std::unordered_map<void*,std::uint8_t> g_material_spec_override;
std::unordered_map<void*,mtd_sem::mtd_envspec_semantics> g_material_envspec;
std::unordered_map<void*,mtd_sem::ptde_flver_texture_semantics> g_material_ptde_texture;

std::uint64_t legacy_semantic_key_hash(const wchar_t *semantic_key) noexcept
{
    if(!semantic_key) return 0u;
    constexpr std::uint64_t offset=14695981039346656037ull;
    constexpr std::uint64_t prime=1099511628211ull;
    std::uint64_t hash=offset;
    const auto *bytes=reinterpret_cast<const std::uint8_t*>(semantic_key);

    for(std::size_t i=0;i<512u;++i){
        std::uint16_t u=0u;
        if(!engine::safe_read_bytes(bytes+i*2u,&u,sizeof(u)))
            return 0u;
        if(u==0u) return hash;
        if(u>=static_cast<std::uint16_t>(L'A') &&
           u<=static_cast<std::uint16_t>(L'Z'))
            u=static_cast<std::uint16_t>(
                u+static_cast<std::uint16_t>(L'a'-L'A'));
        hash^=static_cast<std::uint8_t>(u&0xffu);
        hash*=prime;
        hash^=static_cast<std::uint8_t>((u>>8u)&0xffu);
        hash*=prime;
    }
    return 0u;
}

bool read_semantic_name(
    const wchar_t *semantic_key,
    std::wstring &out) noexcept
{
    out.clear();
    if(!semantic_key) return false;

    for(std::size_t i=0;i<512u;++i){
        std::uint16_t u=0u;
        if(!engine::safe_read_bytes(
                reinterpret_cast<const std::uint8_t*>(semantic_key)+i*2u,
                &u,
                sizeof(u))){
            out.clear();
            return false;
        }
        if(u==0u)
            return !out.empty();
        out.push_back(static_cast<wchar_t>(u));
    }

    out.clear();
    return false;
}

int donor_for(void *material)
{
    if(!material) return -1;
    std::lock_guard lock(g_material_mutex);
    const auto it=g_material_donor.find(material);
    return it==g_material_donor.end() ? -1 : static_cast<int>(it->second);
}

int spec_override_for(void *material)
{
    if(!material) return -1;
    std::lock_guard lock(g_material_mutex);
    const auto it=g_material_spec_override.find(material);
    return it==g_material_spec_override.end() ?
        -1 :
        static_cast<int>(it->second);
}

mtd_sem::mtd_envspec_semantics envspec_for(void *material)
{
    if(!material) return {};
    std::lock_guard lock(g_material_mutex);
    const auto it=g_material_envspec.find(material);
    return it==g_material_envspec.end()
        ? mtd_sem::mtd_envspec_semantics{}
        : it->second;
}

mtd_sem::ptde_flver_texture_semantics ptde_texture_for(void *material)
{
    if(!material) return {};
    std::lock_guard lock(g_material_mutex);
    const auto it=g_material_ptde_texture.find(material);
    return it==g_material_ptde_texture.end()
        ? mtd_sem::ptde_flver_texture_semantics{}
        : it->second;
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
thread_local int g_draw_spec_override=-1;
thread_local mtd_sem::mtd_envspec_semantics g_draw_envspec{};
thread_local bool g_draw_envspec_exact=false;
thread_local mtd_sem::ptde_flver_texture_semantics g_draw_ptde_texture{};
thread_local int g_bound_host=-1;
thread_local bool g_bound_lerp=false;
thread_local bool g_bound_subsurface=false;
constexpr int k_subsurface_material=static_cast<int>(dsrrl::materialdonor::k_donors.size());
constexpr int k_pmetal_route=345;
static_assert(
    dsrrl::materialdonor::k_donors[345].sha256 ==
        k_pmetal_raw_mtd_sha256,
    "Build151 route345 must remain exact P_Metal[DSB].mtd.");
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

    std::array<ID3D11PixelShader*,24> diffuse{};
    std::array<ID3D11PixelShader*,24> full{};
    std::array<ID3D11PixelShader*,24> stock_diffuse_full{};
    std::array<ID3D11PixelShader*,24> diffuse_ul{};
    std::array<ID3D11PixelShader*,24> full_ul{};
    std::array<ID3D11PixelShader*,24> stock_diffuse_full_ul{};
    std::array<ID3D11PixelShader*,24> full_spec{};
    std::array<ID3D11PixelShader*,24> full_ul_spec{};
    std::array<ID3D11PixelShader*,24> stock_diffuse_full_spec{};
    std::array<ID3D11PixelShader*,24> stock_diffuse_full_ul_spec{};

    std::array<ID3D11PixelShader*,24> lerp_diffuse{};
    std::array<ID3D11PixelShader*,24> lerp_full{};
    std::array<ID3D11PixelShader*,24> lerp_stock_diffuse_full{};
    std::array<ID3D11PixelShader*,24> lerp_diffuse_ul{};
    std::array<ID3D11PixelShader*,24> lerp_full_ul{};
    std::array<ID3D11PixelShader*,24> lerp_stock_diffuse_full_ul{};
    std::array<ID3D11PixelShader*,24> lerp_full_spec{};
    std::array<ID3D11PixelShader*,24> lerp_full_ul_spec{};
    std::array<ID3D11PixelShader*,24> lerp_stock_diffuse_full_spec{};
    std::array<ID3D11PixelShader*,24> lerp_stock_diffuse_full_ul_spec{};

    std::array<ID3D11PixelShader*,24> v9a_full{};
    std::array<ID3D11PixelShader*,24> v9a_full_ul{};
    std::array<ID3D11PixelShader*,24> v9a_full_spec{};
    std::array<ID3D11PixelShader*,24> v9a_full_ul_spec{};

    std::array<ID3D11PixelShader*,24> full_v13{};
    std::array<ID3D11PixelShader*,24> full_v13_ul{};
    std::array<ID3D11PixelShader*,24> full_v13_spec{};
    std::array<ID3D11PixelShader*,24> full_v13_ul_spec{};

    // Full Build131 PTDE EnvSpec consumer. These variants always include the
    // independently verified SpecRGB t10 bridge; EnvSpec is not allowed to
    // activate without its material texture input.
    std::array<ID3D11PixelShader*,24> pmetal_envspec_rgba_spec{};
    std::array<ID3D11PixelShader*,24> pmetal_envspec_rgba_ul_spec{};

    std::unordered_map<std::uint16_t,ID3D11Buffer*> b12;
    std::unordered_map<std::uintptr_t,ID3D11Buffer*> pmetal_b12;
};
device_state g_device;
std::mutex g_device_mutex;

void release_device_state()
{
    std::lock_guard lock(g_device_mutex);
    for(auto *&p:g_device.diffuse){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.stock_diffuse_full){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.diffuse_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.stock_diffuse_full_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full_ul_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.stock_diffuse_full_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.stock_diffuse_full_ul_spec){ if(p){p->Release();p=nullptr;} }

    for(auto *&p:g_device.lerp_diffuse){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_full){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_stock_diffuse_full){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_diffuse_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_full_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_stock_diffuse_full_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_full_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_full_ul_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_stock_diffuse_full_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.lerp_stock_diffuse_full_ul_spec){ if(p){p->Release();p=nullptr;} }

    for(auto *&p:g_device.v9a_full){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.v9a_full_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.v9a_full_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.v9a_full_ul_spec){ if(p){p->Release();p=nullptr;} }

    for(auto *&p:g_device.full_v13){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full_v13_ul){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full_v13_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.full_v13_ul_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.pmetal_envspec_rgba_spec){ if(p){p->Release();p=nullptr;} }
    for(auto *&p:g_device.pmetal_envspec_rgba_ul_spec){ if(p){p->Release();p=nullptr;} }
    for(auto &[_,b]:g_device.b12) if(b) b->Release();
    g_device.b12.clear();
    for(auto &[_,b]:g_device.pmetal_b12) if(b) b->Release();
    g_device.pmetal_b12.clear();
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
    if(g_device.diffuse[i] && g_device.full[i] &&
       g_device.stock_diffuse_full[i]) return true;

    const auto diffuse=transform(stock,p,variant::diffuse_v29);
    const auto full=transform(stock,p,variant::full_v211);
    const auto stock_diffuse_full=
        full.ok ? transform_stock_diffuse_material(full.code,p) : transform_result{};
    if(!diffuse.ok || !full.ok || !stock_diffuse_full.ok){
        ++g_shader_pair_fail;
        return false;
    }

    ID3D11PixelShader *ps_diffuse=nullptr,*ps_full=nullptr,*ps_stock_diffuse_full=nullptr;
    if(!create_shader(native,diffuse,ps_diffuse)){
        ++g_shader_pair_fail; return false;
    }
    if(!create_shader(native,full,ps_full) ||
       !create_shader(native,stock_diffuse_full,ps_stock_diffuse_full)){
        if(ps_diffuse) ps_diffuse->Release();
        if(ps_full) ps_full->Release();
        if(ps_stock_diffuse_full) ps_stock_diffuse_full->Release();
        ++g_shader_pair_fail; return false;
    }

    if(g_device.diffuse[i]) g_device.diffuse[i]->Release();
    if(g_device.full[i]) g_device.full[i]->Release();
    if(g_device.stock_diffuse_full[i]) g_device.stock_diffuse_full[i]->Release();
    g_device.diffuse[i]=ps_diffuse;
    g_device.full[i]=ps_full;
    g_device.stock_diffuse_full[i]=ps_stock_diffuse_full;
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
        const auto stock_diffuse_full_ul=
            transform_upper_lower(stock_diffuse_full.code);
        ID3D11PixelShader *ps_diffuse_ul=nullptr,*ps_full_ul=nullptr;
        ID3D11PixelShader *ps_stock_diffuse_full_ul=nullptr;
        if(create_shader(native,diffuse_ul,ps_diffuse_ul) &&
           create_shader(native,full_ul,ps_full_ul) &&
           create_shader(native,stock_diffuse_full_ul,ps_stock_diffuse_full_ul)){
            if(g_device.diffuse_ul[i]) g_device.diffuse_ul[i]->Release();
            if(g_device.full_ul[i]) g_device.full_ul[i]->Release();
            if(g_device.stock_diffuse_full_ul[i]) g_device.stock_diffuse_full_ul[i]->Release();
            g_device.diffuse_ul[i]=ps_diffuse_ul;
            g_device.full_ul[i]=ps_full_ul;
            g_device.stock_diffuse_full_ul[i]=ps_stock_diffuse_full_ul;
            ++g_shader_ul_pass;
        }else{
            if(ps_diffuse_ul) ps_diffuse_ul->Release();
            if(ps_full_ul) ps_full_ul->Release();
            if(ps_stock_diffuse_full_ul) ps_stock_diffuse_full_ul->Release();
            ++g_shader_ul_fail;
        }

        // The combined U/L + SpecRGB variants are built from their already
        // isolated U/L payloads so SpecRGB never decides the diffuse domain.
        if(full_ul.ok && stock_diffuse_full_ul.ok){
            const auto ul_spec=transform_spec_rgb(full_ul.code);
            const auto stock_ul_spec=transform_spec_rgb(stock_diffuse_full_ul.code);
            ID3D11PixelShader *ps_ul_spec=nullptr,*ps_stock_ul_spec=nullptr;
            if(create_shader(native,ul_spec,ps_ul_spec) &&
               create_shader(native,stock_ul_spec,ps_stock_ul_spec)){
                if(g_device.full_ul_spec[i]) g_device.full_ul_spec[i]->Release();
                if(g_device.stock_diffuse_full_ul_spec[i])
                    g_device.stock_diffuse_full_ul_spec[i]->Release();
                g_device.full_ul_spec[i]=ps_ul_spec;
                g_device.stock_diffuse_full_ul_spec[i]=ps_stock_ul_spec;
                ++g_shader_ul_spec_pass;
            }else{
                if(ps_ul_spec) ps_ul_spec->Release();
                if(ps_stock_ul_spec) ps_stock_ul_spec->Release();
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
    const auto stock_diffuse_spec=transform_spec_rgb(stock_diffuse_full.code);
    ID3D11PixelShader *ps_spec=nullptr,*ps_stock_diffuse_spec=nullptr;
    if(create_shader(native,spec,ps_spec) &&
       create_shader(native,stock_diffuse_spec,ps_stock_diffuse_spec)){
        if(g_device.full_spec[i]) g_device.full_spec[i]->Release();
        if(g_device.stock_diffuse_full_spec[i])
            g_device.stock_diffuse_full_spec[i]->Release();
        g_device.full_spec[i]=ps_spec;
        g_device.stock_diffuse_full_spec[i]=ps_stock_diffuse_spec;
        ++g_shader_spec_pass;
    }else{
        if(ps_spec) ps_spec->Release();
        if(ps_stock_diffuse_spec) ps_stock_diffuse_spec->Release();
        ++g_shader_spec_fail;
    }

    // Source-complete Build131 PTDE EnvSpec consumer, retargeted to the
    // exact current V2.11 bodies. It is materialized now but remains dormant
    // while env_spec is hold-off in the runtime manifest.
    if(i>=9u && i<=11u){
        const auto rgba=transform_pmetal_envspec_rgba(full.code);
        const auto rgba_ul=
            rgba.ok ? transform_upper_lower(rgba.code) : transform_result{};
        const auto rgba_spec=
            rgba.ok ? transform_spec_rgb(rgba.code) : transform_result{};
        const auto rgba_ul_spec=
            rgba_ul.ok ? transform_spec_rgb(rgba_ul.code) : transform_result{};

        bool any=false;
        ID3D11PixelShader *ps_rgba_spec=nullptr;
        if(create_shader(native,rgba_spec,ps_rgba_spec)){
            if(g_device.pmetal_envspec_rgba_spec[i])
                g_device.pmetal_envspec_rgba_spec[i]->Release();
            g_device.pmetal_envspec_rgba_spec[i]=ps_rgba_spec;
            any=true;
        }else if(ps_rgba_spec){
            ps_rgba_spec->Release();
        }

        ID3D11PixelShader *ps_rgba_ul_spec=nullptr;
        if(create_shader(native,rgba_ul_spec,ps_rgba_ul_spec)){
            if(g_device.pmetal_envspec_rgba_ul_spec[i])
                g_device.pmetal_envspec_rgba_ul_spec[i]->Release();
            g_device.pmetal_envspec_rgba_ul_spec[i]=ps_rgba_ul_spec;
            any=true;
        }else if(ps_rgba_ul_spec){
            ps_rgba_ul_spec->Release();
        }

        if(any) ++g_envspec_rgba_shader_pass;
        else ++g_envspec_rgba_shader_fail;
    }

    // Exact V13 P_Metal consumer exists only for the three stable P_Metal
    // receivers 33/34/35 => host indices 9/10/11. Failure is isolated: the
    // ordinary MR/SpecRGB/U/L variants above remain valid and are used fail-open.
    if(i>=9u && i<=11u){
        const auto v13=transform_pmetal_v13(full.code);
        const auto v13_ul=v13.ok ? transform_upper_lower(v13.code) : transform_result{};
        const auto v13_spec=v13.ok ? transform_spec_rgb(v13.code) : transform_result{};
        const auto v13_ul_spec=v13_ul.ok ? transform_spec_rgb(v13_ul.code) : transform_result{};

        ID3D11PixelShader *ps_v13=nullptr,*ps_v13_ul=nullptr;
        ID3D11PixelShader *ps_v13_spec=nullptr,*ps_v13_ul_spec=nullptr;
        const bool ok =
            create_shader(native,v13,ps_v13) &&
            create_shader(native,v13_ul,ps_v13_ul) &&
            create_shader(native,v13_spec,ps_v13_spec) &&
            create_shader(native,v13_ul_spec,ps_v13_ul_spec);

        if(ok){
            if(g_device.full_v13[i]) g_device.full_v13[i]->Release();
            if(g_device.full_v13_ul[i]) g_device.full_v13_ul[i]->Release();
            if(g_device.full_v13_spec[i]) g_device.full_v13_spec[i]->Release();
            if(g_device.full_v13_ul_spec[i]) g_device.full_v13_ul_spec[i]->Release();
            g_device.full_v13[i]=ps_v13;
            g_device.full_v13_ul[i]=ps_v13_ul;
            g_device.full_v13_spec[i]=ps_v13_spec;
            g_device.full_v13_ul_spec[i]=ps_v13_ul_spec;
            ++g_shader_v13_pass;
        }else{
            if(ps_v13) ps_v13->Release();
            if(ps_v13_ul) ps_v13_ul->Release();
            if(ps_v13_spec) ps_v13_spec->Release();
            if(ps_v13_ul_spec) ps_v13_ul_spec->Release();
            ++g_shader_v13_fail;
        }
    }

    // Owner-accepted V9A math is an additional exact variant over current
    // V2.11. It is materialized only for the three P_Metal stable pairs.
    if(i>=9u && i<=11u){
        const auto v9a=transform_v9a(full.code);
        const auto v9a_ul=v9a.ok ? transform_upper_lower(v9a.code) : transform_result{};
        const auto v9a_spec=v9a.ok ? transform_spec_rgb(v9a.code) : transform_result{};
        const auto v9a_ul_spec=v9a_ul.ok ? transform_spec_rgb(v9a_ul.code) : transform_result{};

        ID3D11PixelShader *ps_v9a=nullptr,*ps_v9a_ul=nullptr;
        ID3D11PixelShader *ps_v9a_spec=nullptr,*ps_v9a_ul_spec=nullptr;
        const bool ok =
            create_shader(native,v9a,ps_v9a) &&
            create_shader(native,v9a_ul,ps_v9a_ul) &&
            create_shader(native,v9a_spec,ps_v9a_spec) &&
            create_shader(native,v9a_ul_spec,ps_v9a_ul_spec);

        if(ok){
            if(g_device.v9a_full[i]) g_device.v9a_full[i]->Release();
            if(g_device.v9a_full_ul[i]) g_device.v9a_full_ul[i]->Release();
            if(g_device.v9a_full_spec[i]) g_device.v9a_full_spec[i]->Release();
            if(g_device.v9a_full_ul_spec[i]) g_device.v9a_full_ul_spec[i]->Release();
            g_device.v9a_full[i]=ps_v9a;
            g_device.v9a_full_ul[i]=ps_v9a_ul;
            g_device.v9a_full_spec[i]=ps_v9a_spec;
            g_device.v9a_full_ul_spec[i]=ps_v9a_ul_spec;
            ++g_v9a_shader_pass;
        }else{
            if(ps_v9a) ps_v9a->Release();
            if(ps_v9a_ul) ps_v9a_ul->Release();
            if(ps_v9a_spec) ps_v9a_spec->Release();
            if(ps_v9a_ul_spec) ps_v9a_ul_spec->Release();
            ++g_v9a_shader_fail;
        }
    }

    return true;
}

bool ensure_lerp_shader_pair(
    device *d,
    const build151::lerp_plan &p,
    std::span<const std::uint8_t> stock)
{
    auto *native=reinterpret_cast<ID3D11Device*>(d->get_native());
    if(!native) return false;

    std::lock_guard lock(g_device_mutex);
    if(g_device.device!=native) return false;

    const auto i=static_cast<std::size_t>(p.pair_index);
    if(i>=24u) return false;
    if(g_device.lerp_diffuse[i] && g_device.lerp_full[i] &&
       g_device.lerp_stock_diffuse_full[i]) return true;

    const auto diffuse=transform_lerp(stock,p,variant::diffuse_v29);
    const auto full=transform_lerp(stock,p,variant::full_v211);
    const auto stock_diffuse_full=
        full.ok ? transform_stock_diffuse_material_lerp(full.code,p) : transform_result{};
    if(!diffuse.ok || !full.ok || !stock_diffuse_full.ok){
        ++g_lerp_shader_fail;
        return false;
    }

    const auto diffuse_ul=transform_upper_lower(diffuse.code);
    const auto full_ul=transform_upper_lower(full.code);
    const auto stock_diffuse_full_ul=transform_upper_lower(stock_diffuse_full.code);
    const auto full_spec=transform_spec_rgb(full.code);
    const auto stock_diffuse_full_spec=transform_spec_rgb(stock_diffuse_full.code);
    const auto full_ul_spec=full_ul.ok ? transform_spec_rgb(full_ul.code) : transform_result{};
    const auto stock_diffuse_full_ul_spec=
        stock_diffuse_full_ul.ok ? transform_spec_rgb(stock_diffuse_full_ul.code) : transform_result{};

    ID3D11PixelShader *ps_diffuse=nullptr,*ps_full=nullptr,*ps_stock_diffuse_full=nullptr;
    ID3D11PixelShader *ps_diffuse_ul=nullptr,*ps_full_ul=nullptr,*ps_stock_diffuse_full_ul=nullptr;
    ID3D11PixelShader *ps_full_spec=nullptr,*ps_full_ul_spec=nullptr;
    ID3D11PixelShader *ps_stock_diffuse_full_spec=nullptr,*ps_stock_diffuse_full_ul_spec=nullptr;

    const bool ok =
        create_shader(native,diffuse,ps_diffuse) &&
        create_shader(native,full,ps_full) &&
        create_shader(native,stock_diffuse_full,ps_stock_diffuse_full) &&
        create_shader(native,diffuse_ul,ps_diffuse_ul) &&
        create_shader(native,full_ul,ps_full_ul) &&
        create_shader(native,stock_diffuse_full_ul,ps_stock_diffuse_full_ul) &&
        create_shader(native,full_spec,ps_full_spec) &&
        create_shader(native,stock_diffuse_full_spec,ps_stock_diffuse_full_spec) &&
        create_shader(native,full_ul_spec,ps_full_ul_spec) &&
        create_shader(native,stock_diffuse_full_ul_spec,ps_stock_diffuse_full_ul_spec);

    if(!ok){
        if(ps_diffuse) ps_diffuse->Release();
        if(ps_full) ps_full->Release();
        if(ps_stock_diffuse_full) ps_stock_diffuse_full->Release();
        if(ps_diffuse_ul) ps_diffuse_ul->Release();
        if(ps_full_ul) ps_full_ul->Release();
        if(ps_stock_diffuse_full_ul) ps_stock_diffuse_full_ul->Release();
        if(ps_full_spec) ps_full_spec->Release();
        if(ps_stock_diffuse_full_spec) ps_stock_diffuse_full_spec->Release();
        if(ps_full_ul_spec) ps_full_ul_spec->Release();
        if(ps_stock_diffuse_full_ul_spec) ps_stock_diffuse_full_ul_spec->Release();
        ++g_lerp_shader_fail;
        return false;
    }

    if(g_device.lerp_diffuse[i]) g_device.lerp_diffuse[i]->Release();
    if(g_device.lerp_full[i]) g_device.lerp_full[i]->Release();
    if(g_device.lerp_stock_diffuse_full[i]) g_device.lerp_stock_diffuse_full[i]->Release();
    if(g_device.lerp_diffuse_ul[i]) g_device.lerp_diffuse_ul[i]->Release();
    if(g_device.lerp_full_ul[i]) g_device.lerp_full_ul[i]->Release();
    if(g_device.lerp_stock_diffuse_full_ul[i]) g_device.lerp_stock_diffuse_full_ul[i]->Release();
    if(g_device.lerp_full_spec[i]) g_device.lerp_full_spec[i]->Release();
    if(g_device.lerp_full_ul_spec[i]) g_device.lerp_full_ul_spec[i]->Release();
    if(g_device.lerp_stock_diffuse_full_spec[i]) g_device.lerp_stock_diffuse_full_spec[i]->Release();
    if(g_device.lerp_stock_diffuse_full_ul_spec[i]) g_device.lerp_stock_diffuse_full_ul_spec[i]->Release();

    g_device.lerp_diffuse[i]=ps_diffuse;
    g_device.lerp_full[i]=ps_full;
    g_device.lerp_stock_diffuse_full[i]=ps_stock_diffuse_full;
    g_device.lerp_diffuse_ul[i]=ps_diffuse_ul;
    g_device.lerp_full_ul[i]=ps_full_ul;
    g_device.lerp_stock_diffuse_full_ul[i]=ps_stock_diffuse_full_ul;
    g_device.lerp_full_spec[i]=ps_full_spec;
    g_device.lerp_full_ul_spec[i]=ps_full_ul_spec;
    g_device.lerp_stock_diffuse_full_spec[i]=ps_stock_diffuse_full_spec;
    g_device.lerp_stock_diffuse_full_ul_spec[i]=ps_stock_diffuse_full_ul_spec;
    ++g_lerp_shader_pass;
    return true;
}


struct pmetal_envspec_draw_state {
    ID3D11ShaderResourceView *old_t12=nullptr;
    ID3D11ShaderResourceView *old_t14=nullptr;
    ID3D11SamplerState *old_s12=nullptr;
    ID3D11SamplerState *old_s14=nullptr;
    bool b_required=false;
    bool bound=false;
};

void release_pmetal_envspec_state(
    pmetal_envspec_draw_state &state) noexcept
{
    if(state.old_t12) state.old_t12->Release();
    if(state.old_t14) state.old_t14->Release();
    if(state.old_s12) state.old_s12->Release();
    if(state.old_s14) state.old_s14->Release();
    state={};
}

bool bind_pmetal_envspec(
    ID3D11DeviceContext *ctx,
    const envspec::identity_snapshot &snapshot,
    pmetal_envspec_draw_state &state) noexcept
{
    if(!ctx || !snapshot.carrier_ready())
        return false;

    auto *new_t12=reinterpret_cast<ID3D11ShaderResourceView*>(
        static_cast<std::uintptr_t>(snapshot.ptde_a_view.handle));
    auto *new_s12=reinterpret_cast<ID3D11SamplerState*>(
        static_cast<std::uintptr_t>(snapshot.ptde_sampler.handle));
    auto *new_t14=snapshot.probe_b_required ?
        reinterpret_cast<ID3D11ShaderResourceView*>(
            static_cast<std::uintptr_t>(snapshot.ptde_b_view.handle)) :
        nullptr;
    auto *new_s14=snapshot.probe_b_required ? new_s12 : nullptr;

    if(!new_t12 || !new_s12 ||
       (snapshot.probe_b_required && (!new_t14 || !new_s14)))
        return false;

    state.b_required=snapshot.probe_b_required;
    ctx->PSGetShaderResources(12u,1u,&state.old_t12);
    ctx->PSGetSamplers(12u,1u,&state.old_s12);
    if(state.b_required){
        ctx->PSGetShaderResources(14u,1u,&state.old_t14);
        ctx->PSGetSamplers(14u,1u,&state.old_s14);
    }

    ctx->PSSetShaderResources(12u,1u,&new_t12);
    ctx->PSSetSamplers(12u,1u,&new_s12);
    if(state.b_required){
        ctx->PSSetShaderResources(14u,1u,&new_t14);
        ctx->PSSetSamplers(14u,1u,&new_s14);
    }

    ID3D11ShaderResourceView *check_t12=nullptr,*check_t14=nullptr;
    ID3D11SamplerState *check_s12=nullptr,*check_s14=nullptr;
    ctx->PSGetShaderResources(12u,1u,&check_t12);
    ctx->PSGetSamplers(12u,1u,&check_s12);
    bool ok=check_t12==new_t12 && check_s12==new_s12;
    if(state.b_required){
        ctx->PSGetShaderResources(14u,1u,&check_t14);
        ctx->PSGetSamplers(14u,1u,&check_s14);
        ok=ok && check_t14==new_t14 && check_s14==new_s14;
    }
    if(check_t12) check_t12->Release();
    if(check_t14) check_t14->Release();
    if(check_s12) check_s12->Release();
    if(check_s14) check_s14->Release();

    if(!ok){
        // Restore exact prior state even when the prior binding was NULL.
        ctx->PSSetShaderResources(12u,1u,&state.old_t12);
        ctx->PSSetSamplers(12u,1u,&state.old_s12);
        if(state.b_required){
            ctx->PSSetShaderResources(14u,1u,&state.old_t14);
            ctx->PSSetSamplers(14u,1u,&state.old_s14);
        }
        release_pmetal_envspec_state(state);
        return false;
    }

    state.bound=true;
    return true;
}

bool restore_pmetal_envspec(
    ID3D11DeviceContext *ctx,
    pmetal_envspec_draw_state &state) noexcept
{
    if(!state.bound){
        release_pmetal_envspec_state(state);
        return true;
    }
    if(!ctx){
        release_pmetal_envspec_state(state);
        return false;
    }

    ctx->PSSetShaderResources(12u,1u,&state.old_t12);
    ctx->PSSetSamplers(12u,1u,&state.old_s12);
    if(state.b_required){
        ctx->PSSetShaderResources(14u,1u,&state.old_t14);
        ctx->PSSetSamplers(14u,1u,&state.old_s14);
    }

    ID3D11ShaderResourceView *check_t12=nullptr,*check_t14=nullptr;
    ID3D11SamplerState *check_s12=nullptr,*check_s14=nullptr;
    ctx->PSGetShaderResources(12u,1u,&check_t12);
    ctx->PSGetSamplers(12u,1u,&check_s12);
    bool ok=check_t12==state.old_t12 && check_s12==state.old_s12;
    if(state.b_required){
        ctx->PSGetShaderResources(14u,1u,&check_t14);
        ctx->PSGetSamplers(14u,1u,&check_s14);
        ok=ok && check_t14==state.old_t14 && check_s14==state.old_s14;
    }
    if(check_t12) check_t12->Release();
    if(check_t14) check_t14->Release();
    if(check_s12) check_s12->Release();
    if(check_s14) check_s14->Release();

    release_pmetal_envspec_state(state);
    return ok;
}

bool pmetal_native_env_ready(
    ID3D11DeviceContext *ctx,
    float beta) noexcept
{
    if(!ctx || !std::isfinite(beta)) return false;

    ID3D11ShaderResourceView *t12=nullptr,*t14=nullptr;
    ID3D11SamplerState *s12=nullptr,*s14=nullptr;
    ctx->PSGetShaderResources(12,1,&t12);
    ctx->PSGetSamplers(12,1,&s12);
    const bool need_b=beta!=0.0f;
    if(need_b){
        ctx->PSGetShaderResources(14,1,&t14);
        ctx->PSGetSamplers(14,1,&s14);
    }

    bool t12_cube=false,t14_cube=!need_b;
    if(t12){
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
        t12->GetDesc(&desc);
        t12_cube=desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURECUBE;
    }
    if(t14){
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
        t14->GetDesc(&desc);
        t14_cube=desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURECUBE;
    }

    const bool ok=t12 && s12 && t12_cube &&
                  (!need_b || (t14 && s14 && t14_cube));
    if(t12) t12->Release();
    if(t14) t14->Release();
    if(s12) s12->Release();
    if(s14) s14->Release();
    if(ok) ++g_v13_resource_ready;
    else ++g_v13_resource_miss;
    return ok;
}

ID3D11Buffer *realize_pmetal_b12(
    ID3D11DeviceContext *ctx,
    ID3D11Device *device,
    int donor_index,
    const upper_lower::pmetal_env_source &source) noexcept
{
    if(!ctx || !device || donor_index<0 ||
       static_cast<std::size_t>(donor_index)>=dsrrl::materialdonor::k_donors.size() ||
       !std::isfinite(source.a[0]) || !std::isfinite(source.a[1]) ||
       !std::isfinite(source.a[2]) || !std::isfinite(source.b[0]) ||
       !std::isfinite(source.b[1]) || !std::isfinite(source.b[2]) ||
       !std::isfinite(source.beta))
        return nullptr;

    ID3D11Buffer *buffer=nullptr;
    try {
        {
            std::lock_guard lock(g_device_mutex);
            if(g_device.device!=device) return nullptr;
            const auto key=reinterpret_cast<std::uintptr_t>(ctx);
            if(const auto it=g_device.pmetal_b12.find(key);
               it!=g_device.pmetal_b12.end() && it->second){
                buffer=it->second;
                buffer->AddRef();
            }else{
                D3D11_BUFFER_DESC desc{};
                desc.ByteWidth=64;
                desc.Usage=D3D11_USAGE_DYNAMIC;
                desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
                desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
                if(FAILED(device->CreateBuffer(&desc,nullptr,&buffer)) || !buffer)
                    return nullptr;
                g_device.pmetal_b12.emplace(key,buffer);
                buffer->AddRef();
            }
        }

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(ctx->Map(buffer,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)) ||
           !mapped.pData){
            buffer->Release();
            return nullptr;
        }

        const auto &d=dsrrl::materialdonor::k_donors[
            static_cast<std::size_t>(donor_index)];
        struct f4{float x,y,z,w;};
        const std::array<f4,4> payload={{
            {d.c101_f0q[0],d.c101_f0q[1],d.c101_f0q[2],
             d.has_c101?1.0f:0.0f},
            {d.c100[0],d.c100[1],d.c100[2],1.0f},
            {source.a[0],source.a[1],source.a[2],0.0f},
            {source.b[0],source.b[1],source.b[2],source.beta}
        }};
        std::memcpy(mapped.pData,payload.data(),sizeof(payload));
        ctx->Unmap(buffer,0);
        ++g_v13_b12_update;
        return buffer;
    } catch (...) {
        if(buffer) buffer->Release();
        return nullptr;
    }
}

ID3D11Buffer *realize_b12(
    ID3D11Device *device,
    int donor_index,
    const dsrrl::materialdonor::donor &resolved_donor,
    int semantic_override) noexcept
{
    if(!device || donor_index<0 ||
       static_cast<std::size_t>(donor_index)>=dsrrl::materialdonor::k_donors.size())
        return nullptr;

    ID3D11Buffer *buffer=nullptr;
    try {
        std::lock_guard lock(g_device_mutex);
        if(g_device.device!=device) return nullptr;

        const auto key=
            semantic_override>=0 ?
            static_cast<std::uint16_t>(0x8000u+static_cast<unsigned>(semantic_override)) :
            static_cast<std::uint16_t>(donor_index);
        if(const auto it=g_device.b12.find(key);it!=g_device.b12.end() && it->second){
            it->second->AddRef(); ++g_b12_hit; return it->second;
        }

        const auto &d=resolved_donor;
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
    restore_ps_constant_buffer_window(
        ctx,ctx1,12u,
        x.explicit_window ? x.window : x.base,
        x.explicit_window,
        x.first,
        x.count);
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

        const int body_target=subsurface_target(sha);
        if(body_target>=0){
            if(body_target<33 || body_target>35){
                ++g_fail_open;
                return false;
            }
            std::lock_guard lock(g_pending_mutex);
            g_pending.push_back({
                ps->code_size,sha,
                {static_cast<std::uint8_t>(body_target-24),pipeline_kind::subsurface}
            });
            return false;
        }

        if(const auto *p=find_plan(ps->code_size,sha)){
            if(!ensure_shader_pair(d,*p,{bytes,ps->code_size})){
                ++g_fail_open;
                g_quarantined.store(true);
                log_error("DSRRL Runtime v1 MR: exact stable V2.11 shader transform failed; MR quarantined.");
                return false;
            }
            std::lock_guard lock(g_pending_mutex);
            g_pending.push_back({
                ps->code_size,sha,{p->index,pipeline_kind::stable}
            });
            return false;
        }

        if(const auto *p=find_lerp_plan(ps->code_size,sha)){
            if(!ensure_lerp_shader_pair(d,*p,{bytes,ps->code_size})){
                // Build151 Lerp coverage is additive. Exact-transform failure
                // is isolated to this stock receiver and fails open.
                ++g_fail_open;
                return false;
            }
            std::lock_guard lock(g_pending_mutex);
            g_pending.push_back({
                ps->code_size,sha,{p->pair_index,pipeline_kind::lerp}
            });
            return false;
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
        pipeline_target target{};
        bool matched=false;
        {
            std::lock_guard lock(g_pending_mutex);
            for(auto it=g_pending.begin();it!=g_pending.end();++it){
                if(ps->code_size!=it->size || sha!=it->sha)
                    continue;
                target=it->target;
                g_pending.erase(it);
                matched=true;
                break;
            }
        }
        if(matched){
            std::lock_guard lock(g_pipeline_mutex);
            g_pipelines[p.handle]=target;
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

    pipeline_target target{};
    bool matched=false;
    {
        std::lock_guard lock(g_pipeline_mutex);
        if(const auto it=g_pipelines.find(p.handle);it!=g_pipelines.end()){
            target=it->second;
            matched=true;
        }
    }

    if(matched){
        g_bound_host=static_cast<int>(target.pair);
        g_bound_lerp=target.kind==pipeline_kind::lerp;
        g_bound_subsurface=target.kind==pipeline_kind::subsurface;
        g_bound_command=cmd;
        ++g_target_binds;
        if(g_bound_lerp) ++g_lerp_binds;
    }else{
        g_bound_host=-1;
        g_bound_lerp=false;
        g_bound_subsurface=false;
        g_bound_command=nullptr;
        g_draw_donor=-1;
        g_draw_spec_override=-1;
        g_draw_envspec={};
        g_draw_envspec_exact=false;
        g_draw_ptde_texture={};
        upper_lower::consume_draw_selection();
    }
}

bool on_draw_indexed(command_list *cmd,std::uint32_t index_count,std::uint32_t instance_count,
                     std::uint32_t first_index,std::int32_t vertex_offset,std::uint32_t first_instance)
{
    const auto draw_envspec=g_draw_envspec;
    const bool draw_envspec_exact=g_draw_envspec_exact;
    const auto draw_ptde_texture=g_draw_ptde_texture;
    const int draw_spec_override=g_draw_spec_override;
    g_draw_spec_override=-1;
    g_draw_envspec={};
    g_draw_envspec_exact=false;
    g_draw_ptde_texture={};

    // Pixel-inert live preflight must obey the same receiver/command ownership
    // boundary as any future visible EnvSpec carrier. A selector token alone is
    // not sufficient: it may precede a non-target draw on the same thread.
    // Unknown/unowned draws therefore remain completely stock and do not
    // contribute EnvSpec identity/carrier telemetry.
    const bool envspec_receiver_owned=
        cmd && cmd==g_bound_command &&
        !g_bound_subsurface &&
        g_bound_host>=0 && g_bound_host<24;
    envspec::identity_snapshot envspec_identity{};
    if(envspec_receiver_owned){
        envspec_identity=
            envspec::observe_draw(
                cmd,
                draw_envspec,
                draw_envspec_exact,
                g_bound_lerp);
    }

    if(envspec_receiver_owned && draw_envspec_exact){
        ++g_envspec_draw_exact;
        switch(draw_envspec.router_state){
        case mtd_sem::mtd_envspec_router_state::present:
            ++g_envspec_draw_present;
            break;
        case mtd_sem::mtd_envspec_router_state::explicit_none:
            if(draw_envspec.suppress_dsr_only_safe)
                ++g_envspec_draw_none_safe;
            else
                ++g_envspec_draw_none_unsafe;
            break;
        case mtd_sem::mtd_envspec_router_state::nospc_host:
            ++g_envspec_draw_nospc;
            break;
        case mtd_sem::mtd_envspec_router_state::unknown:
        default:
            break;
        }
    }

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

    auto don=dsrrl::materialdonor::k_donors[static_cast<std::size_t>(donor)];
    int semantic_override=body_route ? -1 : draw_spec_override;
    if(semantic_override>=0 &&
       static_cast<std::size_t>(semantic_override) <
           k_semantic_spec_donor_overrides.size()){
        const auto &ov=
            k_semantic_spec_donor_overrides[
                static_cast<std::size_t>(semantic_override)];
        if(ov.raw_sha256==don.sha256){
            for(std::size_t i=0;i<3u;++i){
                don.c101_ptde[i]=ov.c101[i];
                don.c101_f0q[i]=ov.c101_f0q[i];
            }
            don.c101_tier=1u;
            don.c102=ov.c102;
            don.slot=ov.slot;
            don.has_c101=true;
        }else{
            semantic_override=-1;
        }
    }else{
        semantic_override=-1;
    }

    const std::uint32_t receiver_id=24u+static_cast<std::uint32_t>(g_bound_host);
    assets::material_route_scope route{};
    route.exact=true;
    const bool ptde_diffuse_use=
        mtd_sem::ptde_flver_texture_semantic_present(
            draw_ptde_texture,
            mtd_sem::ptde_texture_semantic::diffuse);
    const bool ptde_bump_use=
        mtd_sem::ptde_flver_texture_semantic_present(
            draw_ptde_texture,
            mtd_sem::ptde_texture_semantic::bump);
    route.diffuse_eligible=g_bound_host<12 && ptde_diffuse_use;
    const std::string_view donor_sha=don.sha256;
    const bool normal_material_homologous=
        donor_sha!=k_nonhomologous_normal_pd_sha256 &&
        donor_sha!=k_nonhomologous_normal_pleather_ds_sha256;
    route.normal_eligible=
        g_bound_host<12 && ptde_bump_use && normal_material_homologous;
    if(g_bound_host<12 && !ptde_diffuse_use)
        ++g_diffuse_semantic_hold;
    if(g_bound_host<12 && !ptde_bump_use)
        ++g_normal_semantic_hold;
    route.diffuse_c100_carrier_active=false;
    route.specular_material_verified=
        don.has_c101 &&
        generated::spec_material_route_allowed(don.sha256,receiver_id);
    route.route_index=static_cast<std::uint32_t>(donor);
    route.receivers={receiver_id,0u,0u};

    const bool diffuse_candidate =
        route.diffuse_eligible &&
        g_core->features().enabled(core::operator_id::diffuse) &&
        g_core->features().enabled(core::operator_id::diffuse_material_domain) &&
        assets::diffuse_ready(ctx,route,receiver_id);
    route.diffuse_c100_carrier_active=diffuse_candidate;

    const bool ul_candidate =
        g_core->features().enabled(core::operator_id::upper_lower) &&
        upper_lower::selected_snapshot_ready();
    const bool spec_candidate =
        route.specular_material_verified &&
        g_core->features().enabled(core::operator_id::spec_rgb) &&
        assets::spec_ready(ctx,route,receiver_id);
    const bool normal_candidate =
        route.normal_eligible &&
        g_core->features().enabled(core::operator_id::normal) &&
        assets::normal_ready(ctx,route,receiver_id);
    // Freeze the exact preflight decision for the mutation phase so the
    // transaction plan and actual resource writes cannot diverge.
    route.normal_eligible=normal_candidate;

    if(diffuse_candidate) ++g_diffuse_candidates;
    if(normal_candidate) ++g_normal_candidates;
    if(spec_candidate) ++g_spec_candidates;
    if(ul_candidate) ++g_ul_candidates;

    const bool pmetal_exact_route =
        !body_route &&
        donor==k_pmetal_route &&
        std::string_view(don.sha256)==k_pmetal_raw_mtd_sha256 &&
        receiver_id>=33u && receiver_id<=35u;

    // Full PTDE EnvSpec is independent from the Diffuse/t0 bridge. Its exact
    // dependencies are P_Metal identity, stable receiver, PTDE SpecRGB t10,
    // PTDE material/source b12, raw-RGBA probe carrier and exact sampler.
    // The feature remains hold-off in the manifest, so this path is dormant
    // unless explicitly armed by the control plane.
    const bool pmetal_envspec_feature =
        pmetal_exact_route &&
        !g_bound_lerp &&
        don.has_c101 &&
        spec_candidate &&
        g_core->features().enabled(core::operator_id::env_spec);

    // Lower-priority islands remain independently eligible. Dispatch below
    // suppresses them only after a higher-priority replacement shader is
    // actually available; a requested-but-unready EnvSpec must not disable
    // the owner-accepted V10 path.
    const bool pmetal_v10_feature =
        pmetal_exact_route &&
        diffuse_candidate &&
        g_core->features().enabled(core::operator_id::pmetal_black_safe_v10);

    const bool pmetal_v13_feature =
        pmetal_exact_route &&
        diffuse_candidate &&
        !g_bound_lerp &&
        g_core->features().enabled(core::operator_id::pmetal_black_safe_source);

    upper_lower::pmetal_env_source pmetal_source{};
    bool pmetal_source_ready=false;
    if(pmetal_envspec_feature || pmetal_v13_feature){
        pmetal_source_ready=
            upper_lower::selected_pmetal_env_source(pmetal_source);
        if(pmetal_v13_feature){
            if(pmetal_source_ready) ++g_v13_source_ready;
            else ++g_v13_source_miss;
        }
    }

    // Current exact consumer authority is the stable HemEnv 33/34/35 family.
    // Its PTDE B branch is legal only when beta is zero in this one-endpoint
    // route. Any transitional/two-endpoint case remains fail-open until the
    // exact HemEnvLerp consumer family is independently ported.
    const bool pmetal_envspec_candidate =
        pmetal_envspec_feature &&
        pmetal_source_ready &&
        pmetal_source.beta==0.0f &&
        envspec_identity.carrier_ready() &&
        envspec_identity.slot==2u;
    if(pmetal_envspec_candidate)
        ++g_envspec_rgba_candidate;

    bool pmetal_candidate=false;
    if(pmetal_v13_feature && pmetal_source_ready)
        pmetal_candidate=pmetal_native_env_ready(ctx,pmetal_source.beta);

    bool issued=false;
    bool restore_ok=true;
    bool intended_ul=false;
    bool spec_active=false;
    bool pmetal_envspec_active=false;
    bool pmetal_v10_active=false;
    bool pmetal_active=false;
    bool tx_started=false;
    std::uint64_t command=0;

    ID3D11Device *dev=nullptr;
    ctx->GetDevice(&dev);
    ID3D11PixelShader *oldps=nullptr,*replacement=nullptr,*fallback_replacement=nullptr;
    ID3D11Buffer *b12=nullptr;
    ID3D11DeviceContext1 *ctx1=nullptr;
    cb_capture oldcb{};
    assets::draw_state asset_state{};
    upper_lower::draw_state ul_state{};
    pmetal_envspec_draw_state envspec_state{};

    std::array<ID3D11ClassInstance*,256> old_classes{};
    UINT old_class_count=static_cast<UINT>(old_classes.size());
    bool captured=false;
    if(dev){
        ctx->PSGetShader(&oldps,old_classes.data(),&old_class_count);
        captured=true;
        ctx->QueryInterface(__uuidof(ID3D11DeviceContext1),reinterpret_cast<void**>(&ctx1));
        oldcb=capture_cb(ctx,ctx1);

        {
            std::lock_guard lock(g_device_mutex);
            if(g_device.device==dev){
                const auto i=static_cast<std::size_t>(g_bound_host);

                // Full c100 + linear-diffuse variants are legal only when
                // the exact PTDE t0 dependency is ready. Otherwise preserve
                // the stock DSR diffuse/c100/domain lane while retaining the
                // independently verified c101/SpecRGB/U/L islands.
                ID3D11PixelShader *base_diffuse =
                    diffuse_candidate ?
                        (g_bound_lerp ? g_device.lerp_diffuse[i] : g_device.diffuse[i]) :
                        nullptr;
                ID3D11PixelShader *base_full =
                    diffuse_candidate ?
                        (g_bound_lerp ? g_device.lerp_full[i] : g_device.full[i]) :
                        (g_bound_lerp ? g_device.lerp_stock_diffuse_full[i] :
                                        g_device.stock_diffuse_full[i]);
                ID3D11PixelShader *base_diffuse_ul =
                    diffuse_candidate ?
                        (g_bound_lerp ? g_device.lerp_diffuse_ul[i] : g_device.diffuse_ul[i]) :
                        nullptr;
                ID3D11PixelShader *base_full_ul =
                    diffuse_candidate ?
                        (g_bound_lerp ? g_device.lerp_full_ul[i] : g_device.full_ul[i]) :
                        (g_bound_lerp ? g_device.lerp_stock_diffuse_full_ul[i] :
                                        g_device.stock_diffuse_full_ul[i]);
                ID3D11PixelShader *base_full_spec =
                    diffuse_candidate ?
                        (g_bound_lerp ? g_device.lerp_full_spec[i] : g_device.full_spec[i]) :
                        (g_bound_lerp ? g_device.lerp_stock_diffuse_full_spec[i] :
                                        g_device.stock_diffuse_full_spec[i]);
                ID3D11PixelShader *base_full_ul_spec =
                    diffuse_candidate ?
                        (g_bound_lerp ? g_device.lerp_full_ul_spec[i] : g_device.full_ul_spec[i]) :
                        (g_bound_lerp ? g_device.lerp_stock_diffuse_full_ul_spec[i] :
                                        g_device.stock_diffuse_full_ul_spec[i]);

                ID3D11PixelShader *ul_shader =
                    don.has_c101 ? base_full_ul : base_diffuse_ul;
                intended_ul=ul_candidate && ul_shader!=nullptr;

                if(spec_candidate){
                    ID3D11PixelShader *spec_shader =
                        intended_ul ? base_full_ul_spec : base_full_spec;
                    spec_active=spec_shader!=nullptr;
                }

                if(don.has_c101){
                    fallback_replacement =
                        intended_ul && spec_active ? base_full_ul_spec :
                        intended_ul ? base_full_ul :
                        spec_active ? base_full_spec :
                                      base_full;
                }else{
                    fallback_replacement =
                        intended_ul ? base_diffuse_ul : base_diffuse;
                }
                if(fallback_replacement) fallback_replacement->AddRef();

                if(pmetal_envspec_candidate && spec_active){
                    ID3D11PixelShader *rgba =
                        intended_ul ?
                            g_device.pmetal_envspec_rgba_ul_spec[i] :
                            g_device.pmetal_envspec_rgba_spec[i];
                    if(rgba){
                        replacement=rgba;
                        replacement->AddRef();
                        pmetal_envspec_active=true;
                    }
                }

                if(!replacement && pmetal_v10_feature && don.has_c101){
                    ID3D11PixelShader *v10 =
                        intended_ul && spec_active ? g_device.v9a_full_ul_spec[i] :
                        intended_ul ? g_device.v9a_full_ul[i] :
                        spec_active ? g_device.v9a_full_spec[i] :
                                      g_device.v9a_full[i];
                    if(v10){
                        replacement=v10;
                        replacement->AddRef();
                        pmetal_v10_active=true;
                    }
                }

                if(!replacement && pmetal_candidate && don.has_c101){
                    ID3D11PixelShader *v13 =
                        intended_ul && spec_active ? g_device.full_v13_ul_spec[i] :
                        intended_ul ? g_device.full_v13_ul[i] :
                        spec_active ? g_device.full_v13_spec[i] :
                                      g_device.full_v13[i];
                    if(v13){
                        replacement=v13;
                        replacement->AddRef();
                        pmetal_active=true;
                    }
                }

                // V10 failure is deliberately ordinary-MR fail-open, not a
                // fallback into the less-proven V13 consumer.
                if(!replacement && fallback_replacement){
                    replacement=fallback_replacement;
                    fallback_replacement=nullptr;
                }
            }
        }

        if(pmetal_envspec_active){
            b12=realize_pmetal_b12(ctx,dev,donor,pmetal_source);
            if(!b12){
                ++g_envspec_rgba_bind_fail;
                pmetal_envspec_active=false;
                if(replacement){replacement->Release();replacement=nullptr;}
                if(fallback_replacement){
                    replacement=fallback_replacement;
                    fallback_replacement=nullptr;
                }
                b12=realize_b12(dev,donor,don,semantic_override);
            }
        }else if(pmetal_active){
            b12=realize_pmetal_b12(ctx,dev,donor,pmetal_source);
            if(!b12){
                ++g_v13_b12_fail;
                pmetal_active=false;
                if(replacement){replacement->Release();replacement=nullptr;}
                if(fallback_replacement){
                    replacement=fallback_replacement;
                    fallback_replacement=nullptr;
                }
                b12=realize_b12(dev,donor,don,semantic_override);
            }
        }else{
            b12=realize_b12(dev,donor,don,semantic_override);
        }

        // Exact stock DXBC has no dynamic class linkage. Unknown linkage
        // fails open, and all captured class references are released below.
        if(oldps && replacement && b12 && oldcb.coherent && old_class_count==0 &&
           (!body_route || subsurface_dispatch_ready(static_cast<int>(receiver_id),
            body_material,g_core->features().enabled(core::operator_id::subsurface),true,spec_active))){
            core::render_patch_plan plan{};
            plan.patches[plan.patch_count++]={core::operator_id::material_response,0u,true,false};
            if(diffuse_candidate){
                plan.patches[plan.patch_count++]={
                    core::operator_id::diffuse_material_domain,
                    0u,
                    true,
                    false};
                plan.patches[plan.patch_count++]={
                    core::operator_id::diffuse,
                    0u,
                    false,
                    true};
            }
            if(normal_candidate)
                plan.patches[plan.patch_count++]={core::operator_id::normal,0u,false,true};
            if(body_route)
                plan.patches[plan.patch_count++]={core::operator_id::subsurface,0u,true,false};
            if(spec_active)
                plan.patches[plan.patch_count++]={core::operator_id::spec_rgb,0u,true,true};
            if(pmetal_envspec_active)
                plan.patches[plan.patch_count++]={
                    core::operator_id::env_spec,
                    0u,
                    true,
                    true};
            if(pmetal_v10_active)
                plan.patches[plan.patch_count++]={
                    core::operator_id::pmetal_black_safe_v10,
                    0u,
                    true,
                    false};
            if(pmetal_active)
                plan.patches[plan.patch_count++]={
                    core::operator_id::pmetal_black_safe_source,
                    0u,
                    true,
                    false};
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
                    bool envspec_ok=true;
                    if(pmetal_envspec_active){
                        envspec_ok=
                            assets_ok &&
                            bind_pmetal_envspec(
                                ctx,
                                envspec_identity,
                                envspec_state);
                        if(!envspec_ok)
                            ++g_envspec_rgba_bind_fail;
                    }
                    // Subsurf bypass is all-or-nothing: never drop SSS if a
                    // required ordinary PTDE surface dependency failed to bind.
                    // Full EnvSpec is likewise all-or-nothing with its exact
                    // t12/t14+s12/s14 transaction.
                    if(assets_ok && envspec_ok && (!body_route ||
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
                const bool envspec_restored=
                    restore_pmetal_envspec(ctx,envspec_state);
                if(!envspec_restored)
                    ++g_envspec_rgba_restore_fail;
                const bool ul_restored=upper_lower::restore_draw(ctx,ul_state);
                ctx->PSSetShader(oldps,old_classes.data(),old_class_count);
                restore_cb(ctx,ctx1,oldcb);
                const bool mr_restored=verify_restore(ctx,ctx1,oldcb);
                restore_ok=
                    assets_restored && envspec_restored &&
                    ul_restored && mr_restored;

                const bool tx_restored=g_core->transactions().restore(command);
                tx_started=false;
                if(!tx_restored || !restore_ok){
                    ++g_restore_fail;
                    g_quarantined.store(true);
                    log_error("DSRRL Runtime v1 MR: unified draw restore fault; MR/SpecRGB/U/L/PMetal transaction quarantined.");
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
        (void)restore_pmetal_envspec(ctx,envspec_state);
        (void)upper_lower::restore_draw(ctx,ul_state);
        if(captured){
            ctx->PSSetShader(oldps,old_classes.data(),old_class_count);
            restore_cb(ctx,ctx1,oldcb);
        }
    }else{
        ++g_replays;
        if(g_bound_lerp) ++g_lerp_replays;
        if(body_route) ++g_subsurface_replays;
        if(diffuse_candidate) ++g_full_diffuse_replays;
        else ++g_stock_diffuse_replays;
        if(normal_candidate) ++g_normal_replays;
        if(spec_active) ++g_spec_replays;
        if(intended_ul) ++g_ul_replays;

        if(pmetal_envspec_active){
            ++g_envspec_rgba_replay;
            if(!g_envspec_rgba_first_draw_logged.exchange(true)){
                std::ostringstream os;
                os<<"[DSRRL PMETAL ENVSPEC RGBA] FIRST_DRAW receiver="
                  <<receiver_id
                  <<" slot="<<static_cast<unsigned>(envspec_identity.slot)
                  <<" probeA="<<envspec_identity.probe_a
                  <<" ul="<<(intended_ul?1:0)
                  <<" spec="<<(spec_active?1:0);
                log_info(os.str());
            }
        }

        if(pmetal_v10_active){
            ++g_v10_replays;
            if(!g_v10_first_draw_logged.exchange(true)){
                std::ostringstream os;
                os<<"[DSRRL V10 PMETAL] FIRST_DRAW receiver="<<receiver_id
                  <<" lerp="<<(g_bound_lerp?1:0)
                  <<" ul="<<(intended_ul?1:0)
                  <<" spec="<<(spec_active?1:0);
                log_info(os.str());
            }
        }

        if(pmetal_active){
            ++g_v13_replay;
            if(!g_v13_first_draw_logged.exchange(true)){
                std::ostringstream os;
                os<<"[DSRRL V13 PMETAL] FIRST_DRAW receiver="<<receiver_id
                  <<" beta="<<pmetal_source.beta
                  <<" bankA=0x"<<std::hex<<pmetal_source.bank_signature_a
                  <<" rowA="<<std::dec<<pmetal_source.row_id_a
                  <<" bankB=0x"<<std::hex<<pmetal_source.bank_signature_b
                  <<" rowB="<<std::dec<<pmetal_source.row_id_b
                  <<" ul="<<(intended_ul?1:0)
                  <<" spec="<<(spec_active?1:0);
                log_info(os.str());
            }
        }
    }

    for(auto *instance:old_classes) if(instance) instance->Release();
    upper_lower::consume_draw_selection();
    release_cb(oldcb);
    if(ctx1)ctx1->Release();
    if(b12)b12->Release();
    if(replacement)replacement->Release();
    if(fallback_replacement)fallback_replacement->Release();
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
              <<" lerp_shader_pass="<<g_lerp_shader_pass.load()<<" lerp_shader_fail="<<g_lerp_shader_fail.load()
              <<" v9a_shader_pass="<<g_v9a_shader_pass.load()<<" v9a_shader_fail="<<g_v9a_shader_fail.load()
              <<" v13_shader_pass="<<g_shader_v13_pass.load()<<" v13_shader_fail="<<g_shader_v13_fail.load()
              <<" env_rgba_shader_pass="<<g_envspec_rgba_shader_pass.load()
              <<" env_rgba_shader_fail="<<g_envspec_rgba_shader_fail.load()
              <<" env_rgba_candidate="<<g_envspec_rgba_candidate.load()
              <<" env_rgba_replay="<<g_envspec_rgba_replay.load()
              <<" env_rgba_bind_fail="<<g_envspec_rgba_bind_fail.load()
              <<" env_rgba_restore_fail="<<g_envspec_rgba_restore_fail.load()
              <<" v13_source_ready="<<g_v13_source_ready.load()<<" v13_source_miss="<<g_v13_source_miss.load()
              <<" v13_resource_ready="<<g_v13_resource_ready.load()<<" v13_resource_miss="<<g_v13_resource_miss.load()
              <<" v13_b12_update="<<g_v13_b12_update.load()<<" v13_b12_fail="<<g_v13_b12_fail.load()
              <<" v13_replay="<<g_v13_replay.load()
              <<" binds="<<g_target_binds.load()<<" lerp_binds="<<g_lerp_binds.load()
              <<" replay="<<g_replays.load()<<" lerp_replay="<<g_lerp_replays.load()
              <<" diff_candidate="<<g_diffuse_candidates.load()
              <<" diff_full_replay="<<g_full_diffuse_replays.load()
              <<" diff_stock_replay="<<g_stock_diffuse_replays.load()
              <<" norm_candidate="<<g_normal_candidates.load()
              <<" norm_replay="<<g_normal_replays.load()
              <<" spec_candidate="<<g_spec_candidates.load()
              <<" spec_replay="<<g_spec_replays.load()
              <<" ul_candidate="<<g_ul_candidates.load()
              <<" ul_replay="<<g_ul_replays.load()
              <<" v10_replay="<<g_v10_replays.load()
              <<" subsurface_replay="<<g_subsurface_replays.load()<<" b12_create="<<g_b12_create.load()
              <<" b12_hit="<<g_b12_hit.load()
               <<" envkey_ok="<<g_envspec_key_valid.load()<<" envkey_fail="<<g_envspec_key_fail.load()
               <<" env_mtd_exact="<<g_envspec_mtd_exact.load()
               <<" env_sel_exact="<<g_envspec_selector_exact.load()
               <<" env_draw_exact="<<g_envspec_draw_exact.load()
               <<" env_present="<<g_envspec_draw_present.load()
               <<" env_none_safe="<<g_envspec_draw_none_safe.load()
               <<" env_none_hold="<<g_envspec_draw_none_unsafe.load()
               <<" env_nospc="<<g_envspec_draw_nospc.load()
               <<" spec_semantic_hit="<<g_semantic_spec_override_hit.load()
               <<" spec_ambiguous_hold="<<g_semantic_spec_ambiguous_hold.load()
               <<" ptde_tex_mtd_exact="<<g_ptde_tex_mtd_exact.load()
               <<" ptde_tex_sel_exact="<<g_ptde_tex_selector_exact.load()
               <<" diff_sem_hold="<<g_diffuse_semantic_hold.load()
               <<" norm_sem_hold="<<g_normal_semantic_hold.load()
               <<" failopen="<<g_fail_open.load()
              <<" restore_fail="<<g_restore_fail.load()<<" quarantined="<<(g_quarantined.load()?1:0);
            log_info(os.str());
        }
    } catch (...) {
        // Telemetry is non-authoritative and must never escape the callback ABI.
    }
}

} // namespace

void mtd_event(
    void *material,
    const void *raw,
    std::uint32_t len,
    const wchar_t *semantic_key) noexcept
{
    if(!material || !raw || !len) return;
    ++g_mtd_seen;
    try{
        const auto digest=dsrrl::sha256({
            reinterpret_cast<const std::byte*>(raw),len
        });
        const auto hash=dsrrl::to_hex(digest);
        const int idx=hash==subsurface::k_dsr_body_subsurf_material_sha256 ?
            k_subsurface_material : dsrrl::materialdonor::find_sha256(hash);

        const std::uint64_t legacy_key=
            legacy_semantic_key_hash(semantic_key);
        if(legacy_key!=0u) ++g_envspec_key_valid;
        else ++g_envspec_key_fail;

        const auto envspec=
            mtd_sem::classify_mtd_envspec_semantics_legacy(
                legacy_key,
                digest);
        const auto ptde_texture=
            mtd_sem::classify_ptde_flver_texture_semantics_legacy(
                legacy_key,
                digest);
        if(ptde_texture.exact_host_identity_match)
            ++g_ptde_tex_mtd_exact;

        int semantic_spec_override=-1;
        std::wstring semantic_name;
        if(read_semantic_name(semantic_key,semantic_name))
            semantic_spec_override=
                find_semantic_spec_donor_override(
                    semantic_name,
                    hash);

        if(semantic_spec_override>=0)
            ++g_semantic_spec_override_hit;
        else if(semantic_spec_raw_hash_is_ambiguous(hash))
            ++g_semantic_spec_ambiguous_hold;

        std::lock_guard lock(g_material_mutex);
        g_material_donor.erase(material);
        g_material_spec_override.erase(material);
        g_material_envspec.erase(material);
        g_material_ptde_texture.erase(material);

        if(idx>=0){
            g_material_donor[material]=static_cast<std::uint16_t>(idx);
            ++g_mapped;
        }else{
            ++g_unmapped;
        }

        if(semantic_spec_override>=0)
            g_material_spec_override[material]=
                static_cast<std::uint8_t>(semantic_spec_override);

        if(envspec.exact_identity_match){
            g_material_envspec[material]=envspec;
            ++g_envspec_mtd_exact;
        }
        if(ptde_texture.exact_host_identity_match)
            g_material_ptde_texture[material]=ptde_texture;
    }catch(...){++g_fail_open;}
}

void selector_event(void *container,void *,void *ret,void *,void *,std::int32_t material_index) noexcept
{
    ++g_selector_seen;
    const auto base=engine::image_base();
    if(!base){
        g_draw_donor=-1;
        g_draw_spec_override=-1;
        g_draw_envspec={};
        g_draw_envspec_exact=false;
        g_draw_ptde_texture={};
        return;
    }
    const auto rva=reinterpret_cast<std::uintptr_t>(ret)-base;
    if(rva!=k_ret_sel_1 && rva!=k_ret_sel_2 && rva!=k_ret_sel_3){
        g_draw_donor=-1;
        g_draw_spec_override=-1;
        g_draw_envspec={};
        g_draw_envspec_exact=false;
        g_draw_ptde_texture={};
        return;
    }
    void *actual=resolve_material(container,material_index);
    g_draw_donor=donor_for(actual);
    g_draw_spec_override=spec_override_for(actual);
    g_draw_envspec=envspec_for(actual);
    g_draw_envspec_exact=g_draw_envspec.exact_identity_match;
    g_draw_ptde_texture=ptde_texture_for(actual);
    if(g_draw_donor>=0) ++g_selector_mapped;
    if(g_draw_envspec_exact) ++g_envspec_selector_exact;
    if(g_draw_ptde_texture.exact_host_identity_match)
        ++g_ptde_tex_selector_exact;
}

bool register_runtime(core::renderer_core &core) noexcept
{
    g_core=&core;
    g_quarantined.store(false);
    g_v13_first_draw_logged.store(false);
    g_v10_first_draw_logged.store(false);
    g_envspec_rgba_first_draw_logged.store(false);
    g_draw_donor=-1; g_draw_spec_override=-1; g_draw_envspec={}; g_draw_envspec_exact=false; g_draw_ptde_texture={};
    g_bound_host=-1; g_bound_lerp=false; g_bound_subsurface=false; g_bound_command=nullptr;
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
    {
        std::lock_guard lock(g_material_mutex);
        g_material_donor.clear();
        g_material_spec_override.clear();
        g_material_envspec.clear();
        g_material_ptde_texture.clear();
    }
    g_draw_donor=-1; g_draw_spec_override=-1; g_draw_envspec={}; g_draw_envspec_exact=false; g_draw_ptde_texture={};
    g_bound_host=-1; g_bound_lerp=false; g_bound_subsurface=false; g_bound_command=nullptr;
    g_core=nullptr;
}

} // namespace dsrrl::runtime::mr