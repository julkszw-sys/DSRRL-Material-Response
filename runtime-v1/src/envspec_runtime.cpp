#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/envspec_runtime.hpp"

#include "dsrrl/operators/env_spec/legacy_resource_bridge.hpp"
#include "dsrrl/sha256.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <Windows.h>

using namespace reshade::api;

namespace dsrrl::runtime::envspec {
namespace {

constexpr std::uint32_t k_dsr_size=256u;
constexpr std::uint32_t k_faces=6u;
constexpr std::uint32_t k_dsr_levels=8u;
constexpr std::size_t k_dsr_tight_bc6h_bytes=524352u;
constexpr std::uint32_t k_ptde_size=32u;
constexpr std::uint32_t k_ptde_slots=4u;
constexpr std::size_t k_ptde_face_bytes=
    static_cast<std::size_t>(k_ptde_size)*k_ptde_size*4u;
constexpr std::size_t k_ptde_bytes_per_cube=
    k_ptde_face_bytes*k_faces;

struct native_resource_record {
    std::uint16_t probe_ordinal=0;
};

struct srv_binding {
    bool valid=false;
    resource_view view{};
};

struct command_bindings {
    std::array<srv_binding,2> srv{};
};

struct ptde_rgba_cube {
    std::uint16_t probe_ordinal=0;
    std::uint8_t slot=0;
    resource resource_handle{};
    resource_view view{};
};

std::mutex g_mutex;
std::unordered_map<std::uint64_t,native_resource_record> g_native_resources;
std::unordered_map<std::uint64_t,std::uint64_t> g_resource_by_view;
std::unordered_map<std::uint64_t,command_bindings> g_command_bindings;
std::unordered_map<std::uint32_t,ptde_rgba_cube> g_ptde_cubes;
device *g_device=nullptr;
std::vector<std::uint8_t> g_ptde_pack;
sampler g_ptde_sampler{};
bool g_pack_admitted=false;
bool g_sampler_ready=false;

std::atomic<std::uint64_t> g_native_candidates{0};
std::atomic<std::uint64_t> g_native_matches{0};
std::atomic<std::uint64_t> g_native_hash_miss{0};
std::atomic<std::uint64_t> g_view_matches{0};
std::atomic<std::uint64_t> g_t12_seen{0};
std::atomic<std::uint64_t> g_t14_seen{0};
std::atomic<std::uint64_t> g_draw_exact{0};
std::atomic<std::uint64_t> g_draw_present{0};
std::atomic<std::uint64_t> g_draw_pair_ready{0};
std::atomic<std::uint64_t> g_draw_pair_miss{0};
std::atomic<std::uint64_t> g_pack_admit_ok{0};
std::atomic<std::uint64_t> g_pack_admit_fail{0};
std::atomic<std::uint64_t> g_raw_cube_created{0};
std::atomic<std::uint64_t> g_raw_cube_fail{0};
std::atomic<std::uint64_t> g_carrier_ready{0};
std::atomic<std::uint64_t> g_carrier_miss{0};
std::atomic<std::uint64_t> g_present{0};
std::atomic<bool> g_first_pair_logged{false};
std::atomic<bool> g_first_carrier_logged{false};

std::uint64_t command_id(command_list *cmd) noexcept
{
    return static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(cmd));
}

std::filesystem::path process_dir()
{
    std::wstring buffer(32768,L'\0');
    const DWORD n=GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if(n==0u || n>=buffer.size())
        return {};
    buffer.resize(n);
    return std::filesystem::path(buffer).parent_path();
}

bool admit_ptde_pack()
{
    try{
        const auto root=process_dir();
        if(root.empty())
            return false;
        const auto path=
            root/std::filesystem::path(
                operators::env_spec::k_legacy_packed_gi_relative_path);

        std::error_code ec;
        const auto size=std::filesystem::file_size(path,ec);
        if(ec ||
           size!=operators::env_spec::k_legacy_packed_gi_size)
            return false;

        std::ifstream stream(path,std::ios::binary);
        if(!stream)
            return false;

        std::vector<std::uint8_t> bytes(
            static_cast<std::size_t>(size));
        if(!stream.read(
               reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size())))
            return false;

        const auto digest=dsrrl::sha256(
            std::span<const std::byte>{
                reinterpret_cast<const std::byte*>(bytes.data()),
                bytes.size()});
        if(digest!=operators::env_spec::k_legacy_packed_gi_sha256)
            return false;

        g_ptde_pack=std::move(bytes);
        return true;
    }catch(...){
        return false;
    }
}

std::optional<resource_view> get_ptde_rgba_cube(
    std::uint16_t probe,
    std::uint8_t slot) noexcept
{
    if(g_device==nullptr || !g_pack_admitted ||
       probe>=operators::env_spec::k_legacy_envspec_probe_count ||
       slot>=k_ptde_slots)
        return std::nullopt;

    const std::uint32_t key=
        static_cast<std::uint32_t>(probe)*k_ptde_slots+slot;
    {
        std::lock_guard lock(g_mutex);
        const auto it=g_ptde_cubes.find(key);
        if(it!=g_ptde_cubes.end())
            return it->second.view;
    }

    const std::size_t offset=
        static_cast<std::size_t>(key)*k_ptde_bytes_per_cube;
    if(offset>g_ptde_pack.size() ||
       k_ptde_bytes_per_cube>g_ptde_pack.size()-offset){
        ++g_raw_cube_fail;
        return std::nullopt;
    }

    std::array<subresource_data,k_faces> sub{};
    for(std::uint32_t face=0;face<k_faces;++face){
        sub[face].data=
            g_ptde_pack.data()+offset+
            static_cast<std::size_t>(face)*k_ptde_face_bytes;
        sub[face].row_pitch=k_ptde_size*4u;
        sub[face].slice_pitch=
            static_cast<std::uint32_t>(k_ptde_face_bytes);
    }

    const resource_desc desc(
        resource_type::texture_2d,
        k_ptde_size,
        k_ptde_size,
        k_faces,
        1u,
        format::r8g8b8a8_unorm,
        1u,
        memory_heap::default_,
        resource_usage::shader_resource,
        resource_flags::cube_compatible);

    ptde_rgba_cube cube{};
    cube.probe_ordinal=probe;
    cube.slot=slot;

    const bool resource_ok=
        g_device->create_resource(
            desc,
            sub.data(),
            resource_usage::shader_resource,
            &cube.resource_handle);
    bool view_ok=false;
    if(resource_ok){
        const resource_view_desc vd(
            resource_view_type::texture_cube,
            format::r8g8b8a8_unorm,
            0u,1u,0u,k_faces);
        view_ok=g_device->create_resource_view(
            cube.resource_handle,
            resource_usage::shader_resource,
            vd,
            &cube.view);
    }

    if(!resource_ok || !view_ok){
        if(cube.view.handle!=0u)
            g_device->destroy_resource_view(cube.view);
        if(cube.resource_handle.handle!=0u)
            g_device->destroy_resource(cube.resource_handle);
        ++g_raw_cube_fail;
        return std::nullopt;
    }

    resource_view existing{};
    bool inserted=false;
    {
        std::lock_guard lock(g_mutex);
        const auto result=g_ptde_cubes.emplace(key,cube);
        inserted=result.second;
        existing=result.first->second.view;
    }
    if(!inserted){
        g_device->destroy_resource_view(cube.view);
        g_device->destroy_resource(cube.resource_handle);
        return existing;
    }

    ++g_raw_cube_created;
    return cube.view;
}

void release_ptde_carrier(device *device_ptr) noexcept
{
    if(device_ptr==nullptr)
        return;

    std::unordered_map<std::uint32_t,ptde_rgba_cube> dead;
    sampler sampler_to_destroy{};
    {
        std::lock_guard lock(g_mutex);
        dead.swap(g_ptde_cubes);
        sampler_to_destroy=g_ptde_sampler;
        g_ptde_sampler={};
        g_sampler_ready=false;
        g_pack_admitted=false;
        g_ptde_pack.clear();
        if(g_device==device_ptr)
            g_device=nullptr;
    }

    for(const auto &[_,cube]:dead){
        if(cube.view.handle!=0u)
            device_ptr->destroy_resource_view(cube.view);
        if(cube.resource_handle.handle!=0u)
            device_ptr->destroy_resource(cube.resource_handle);
    }
    if(sampler_to_destroy.handle!=0u)
        device_ptr->destroy_sampler(sampler_to_destroy);
}

void on_init_device(device *device_ptr)
{
    if(device_ptr==nullptr)
        return;

    {
        std::lock_guard lock(g_mutex);
        if(g_device!=nullptr && g_device!=device_ptr)
            return;
        g_device=device_ptr;
    }

    g_pack_admitted=admit_ptde_pack();
    if(g_pack_admitted)
        ++g_pack_admit_ok;
    else
        ++g_pack_admit_fail;

    sampler_desc sd{};
    sd.filter=filter_mode::anisotropic;
    sd.address_u=texture_address_mode::mirror;
    sd.address_v=texture_address_mode::mirror;
    sd.address_w=texture_address_mode::wrap;
    sd.max_anisotropy=1.0f;
    sd.min_lod=0.0f;
    sd.max_lod=0.0f;
    g_sampler_ready=
        device_ptr->create_sampler(sd,&g_ptde_sampler);

    reshade::log::message(
        (g_pack_admitted && g_sampler_ready)
            ? reshade::log::level::info
            : reshade::log::level::warning,
        (g_pack_admitted && g_sampler_ready)
            ? "DSRRL EnvSpec RESOURCE preflight: exact raw-RGBA pack + PTDE sampler ready; draw mutation remains OFF."
            : "DSRRL EnvSpec RESOURCE preflight: pack/sampler unavailable; fail-open stock DSR.");
}

void on_destroy_device(device *device_ptr)
{
    release_ptde_carrier(device_ptr);
}

bool is_native_envspec_desc(const resource_desc &desc) noexcept
{
    if(desc.type!=resource_type::texture_2d ||
       desc.texture.width!=k_dsr_size ||
       desc.texture.height!=k_dsr_size ||
       desc.texture.depth_or_layers!=k_faces ||
       desc.texture.levels!=k_dsr_levels ||
       desc.texture.samples!=1u)
        return false;

    return desc.texture.format==format::bc6h_typeless ||
           desc.texture.format==format::bc6h_ufloat ||
           desc.texture.format==format::bc6h_sfloat;
}

bool extract_tight_bc6h(
    const resource_desc &desc,
    const subresource_data *initial_data,
    std::vector<std::uint8_t> &out)
{
    if(!is_native_envspec_desc(desc) || initial_data==nullptr)
        return false;

    out.clear();
    out.reserve(k_dsr_tight_bc6h_bytes);

    for(std::uint32_t face=0;face<k_faces;++face){
        for(std::uint32_t level=0;level<k_dsr_levels;++level){
            const std::uint32_t sub=face*k_dsr_levels+level;
            const auto &src=initial_data[sub];
            if(src.data==nullptr)
                return false;

            const std::uint32_t width=
                std::max(1u,k_dsr_size>>level);
            const std::uint32_t height=
                std::max(1u,k_dsr_size>>level);
            const std::uint32_t block_cols=
                std::max(1u,(width+3u)/4u);
            const std::uint32_t block_rows=
                std::max(1u,(height+3u)/4u);
            const std::uint32_t tight_row=block_cols*16u;
            const std::uint32_t row_pitch=
                src.row_pitch!=0u ? src.row_pitch : tight_row;
            if(row_pitch<tight_row)
                return false;

            const auto *bytes=
                static_cast<const std::uint8_t*>(src.data);
            for(std::uint32_t y=0;y<block_rows;++y){
                const auto offset=
                    static_cast<std::size_t>(y)*row_pitch;
                out.insert(
                    out.end(),
                    bytes+offset,
                    bytes+offset+tight_row);
            }
        }
    }
    return out.size()==k_dsr_tight_bc6h_bytes;
}

std::optional<std::uint16_t> probe_for_view(resource_view view) noexcept
{
    if(view.handle==0u)
        return std::nullopt;

    std::lock_guard lock(g_mutex);
    const auto vit=g_resource_by_view.find(view.handle);
    if(vit==g_resource_by_view.end())
        return std::nullopt;
    const auto rit=g_native_resources.find(vit->second);
    if(rit==g_native_resources.end())
        return std::nullopt;
    return rit->second.probe_ordinal;
}

void on_init_command_list(command_list *cmd)
{
    if(!cmd) return;
    std::lock_guard lock(g_mutex);
    g_command_bindings.try_emplace(command_id(cmd));
}

void on_destroy_command_list(command_list *cmd)
{
    if(!cmd) return;
    std::lock_guard lock(g_mutex);
    g_command_bindings.erase(command_id(cmd));
}

void on_init_resource(
    device *,
    const resource_desc &desc,
    const subresource_data *initial_data,
    resource_usage,
    resource resource)
{
    if(resource.handle==0u || !is_native_envspec_desc(desc))
        return;

    ++g_native_candidates;
    std::vector<std::uint8_t> tight;
    if(!extract_tight_bc6h(desc,initial_data,tight)){
        ++g_native_hash_miss;
        return;
    }

    const auto digest=dsrrl::sha256(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(tight.data()),
        tight.size()});
    const auto probe=
        operators::env_spec::legacy_native_probe_for_sha(digest);
    if(!probe.has_value()){
        ++g_native_hash_miss;
        return;
    }

    {
        std::lock_guard lock(g_mutex);
        g_native_resources[resource.handle]=
            native_resource_record{probe.value()};
    }
    ++g_native_matches;
}

void on_destroy_resource(device *,resource resource)
{
    if(resource.handle==0u) return;
    std::lock_guard lock(g_mutex);
    g_native_resources.erase(resource.handle);
    for(auto it=g_resource_by_view.begin();
        it!=g_resource_by_view.end();){
        if(it->second==resource.handle)
            it=g_resource_by_view.erase(it);
        else
            ++it;
    }
}

void on_init_resource_view(
    device *,
    resource resource,
    resource_usage usage,
    const resource_view_desc &,
    resource_view view)
{
    if(resource.handle==0u || view.handle==0u ||
       (usage&resource_usage::shader_resource)==
           resource_usage::undefined)
        return;

    std::lock_guard lock(g_mutex);
    if(g_native_resources.find(resource.handle)!=
       g_native_resources.end()){
        g_resource_by_view[view.handle]=resource.handle;
        ++g_view_matches;
    }
}

void on_destroy_resource_view(device *,resource_view view)
{
    if(view.handle==0u) return;
    std::lock_guard lock(g_mutex);
    g_resource_by_view.erase(view.handle);
}

int tracked_slot(std::uint32_t binding) noexcept
{
    return binding==12u ? 0 : (binding==14u ? 1 : -1);
}

void on_push_descriptors(
    command_list *cmd,
    shader_stage stages,
    pipeline_layout,
    std::uint32_t,
    const descriptor_table_update &update)
{
    if(!cmd ||
       (stages&shader_stage::pixel)!=shader_stage::pixel ||
       update.descriptors==nullptr)
        return;

    std::lock_guard lock(g_mutex);
    auto &state=g_command_bindings[command_id(cmd)];

    for(std::uint32_t i=0;i<update.count;++i){
        const int slot=tracked_slot(update.binding+i);
        if(slot<0) continue;

        if(update.type==descriptor_type::shader_resource_view){
            const auto *views=
                static_cast<const resource_view*>(update.descriptors);
            state.srv[static_cast<std::size_t>(slot)]=
                {true,views[i]};
        }else if(update.type==
                 descriptor_type::sampler_with_resource_view){
            const auto *views=
                static_cast<const sampler_with_resource_view*>(
                    update.descriptors);
            state.srv[static_cast<std::size_t>(slot)]=
                {true,views[i].view};
        }else{
            continue;
        }

        if(slot==0) ++g_t12_seen;
        else ++g_t14_seen;
    }
}

void on_present(
    command_queue *,
    swapchain *,
    const rect *,
    const rect *,
    std::uint32_t,
    const rect *)
{
    const auto n=++g_present;
    if(n!=300u && !(n>300u && (n%1200u)==0u))
        return;

    std::ostringstream os;
    os<<"DSRRL EnvSpec preflight: present="<<n
      <<" native_candidates="<<g_native_candidates.load()
      <<" native_matches="<<g_native_matches.load()
      <<" native_hash_miss="<<g_native_hash_miss.load()
      <<" views="<<g_view_matches.load()
      <<" t12="<<g_t12_seen.load()
      <<" t14="<<g_t14_seen.load()
      <<" draw_exact="<<g_draw_exact.load()
      <<" draw_present="<<g_draw_present.load()
      <<" pair_ready="<<g_draw_pair_ready.load()
      <<" pair_miss="<<g_draw_pair_miss.load()
      <<" pack_ok="<<g_pack_admit_ok.load()
      <<" pack_fail="<<g_pack_admit_fail.load()
      <<" raw_created="<<g_raw_cube_created.load()
      <<" raw_fail="<<g_raw_cube_fail.load()
      <<" carrier_ready="<<g_carrier_ready.load()
      <<" carrier_miss="<<g_carrier_miss.load();
    reshade::log::message(
        reshade::log::level::info,
        os.str().c_str());
}

} // namespace

bool register_runtime() noexcept
{
    try{
        reshade::register_event<reshade::addon_event::init_device>(
            on_init_device);
        reshade::register_event<reshade::addon_event::destroy_device>(
            on_destroy_device);
        reshade::register_event<reshade::addon_event::init_command_list>(
            on_init_command_list);
        reshade::register_event<reshade::addon_event::destroy_command_list>(
            on_destroy_command_list);
        reshade::register_event<reshade::addon_event::init_resource>(
            on_init_resource);
        reshade::register_event<reshade::addon_event::destroy_resource>(
            on_destroy_resource);
        reshade::register_event<reshade::addon_event::init_resource_view>(
            on_init_resource_view);
        reshade::register_event<reshade::addon_event::destroy_resource_view>(
            on_destroy_resource_view);
        reshade::register_event<reshade::addon_event::push_descriptors>(
            on_push_descriptors);
        reshade::register_event<reshade::addon_event::present>(
            on_present);
        return true;
    }catch(...){
        unregister_runtime();
        return false;
    }
}

void unregister_runtime() noexcept
{
    reshade::unregister_event<reshade::addon_event::present>(on_present);
    reshade::unregister_event<reshade::addon_event::push_descriptors>(
        on_push_descriptors);
    reshade::unregister_event<
        reshade::addon_event::destroy_resource_view>(
            on_destroy_resource_view);
    reshade::unregister_event<reshade::addon_event::init_resource_view>(
        on_init_resource_view);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(
        on_destroy_resource);
    reshade::unregister_event<reshade::addon_event::init_resource>(
        on_init_resource);
    reshade::unregister_event<
        reshade::addon_event::destroy_command_list>(
            on_destroy_command_list);
    reshade::unregister_event<reshade::addon_event::init_command_list>(
        on_init_command_list);
    reshade::unregister_event<reshade::addon_event::destroy_device>(
        on_destroy_device);
    reshade::unregister_event<reshade::addon_event::init_device>(
        on_init_device);

    if(g_device!=nullptr)
        release_ptde_carrier(g_device);

    std::lock_guard lock(g_mutex);
    g_native_resources.clear();
    g_resource_by_view.clear();
    g_command_bindings.clear();
}

identity_snapshot observe_draw(
    command_list *cmd,
    const operators::material_response::mtd_envspec_semantics &material,
    bool exact_material,
    bool probe_b_required) noexcept
{
    identity_snapshot out;
    out.exact_material=
        exact_material && material.exact_identity_match;
    out.probe_b_required=probe_b_required;
    if(!out.exact_material || !cmd)
        return out;

    ++g_draw_exact;
    out.material_present=
        material.router_state==
            operators::material_response::
                mtd_envspec_router_state::present;
    out.slot_valid=material.envspc_slot_valid;
    out.slot=material.envspc_slot;

    if(!out.material_present)
        return out;
    ++g_draw_present;

    command_bindings state{};
    {
        std::lock_guard lock(g_mutex);
        const auto it=g_command_bindings.find(command_id(cmd));
        if(it==g_command_bindings.end()){
            ++g_draw_pair_miss;
            return out;
        }
        state=it->second;
    }

    if(state.srv[0].valid){
        const auto probe=probe_for_view(state.srv[0].view);
        if(probe.has_value()){
            out.probe_a_ready=true;
            out.probe_a=probe.value();
        }
    }
    if(out.probe_b_required && state.srv[1].valid){
        const auto probe=probe_for_view(state.srv[1].view);
        if(probe.has_value()){
            out.probe_b_ready=true;
            out.probe_b=probe.value();
        }
    }

    if(out.identity_ready()){
        ++g_draw_pair_ready;
        if(!g_first_pair_logged.exchange(true)){
            std::ostringstream os;
            os<<"[DSRRL ENVSPEC PREFLIGHT] FIRST IDENTITY PASS slot="
              <<static_cast<unsigned>(out.slot)
              <<" probeA="<<out.probe_a
              <<" probeB="<<out.probe_b;
            reshade::log::message(
                reshade::log::level::info,
                os.str().c_str());
        }
    }else{
        ++g_draw_pair_miss;
        return out;
    }

    out.packed_gi_admitted=g_pack_admitted;
    out.stored_alpha_preserved=g_pack_admitted;
    out.ptde_sampler_ready=
        g_sampler_ready && g_ptde_sampler.handle!=0u;
    out.ptde_sampler=g_ptde_sampler;

    const auto a=get_ptde_rgba_cube(out.probe_a,out.slot);
    if(a.has_value()){
        out.ptde_a_ready=true;
        out.ptde_a_view=a.value();
    }

    if(out.probe_b_required){
        const auto b=get_ptde_rgba_cube(out.probe_b,out.slot);
        if(b.has_value()){
            out.ptde_b_ready=true;
            out.ptde_b_view=b.value();
        }
    }

    if(out.carrier_ready()){
        ++g_carrier_ready;
        if(!g_first_carrier_logged.exchange(true)){
            std::ostringstream os;
            os<<"[DSRRL ENVSPEC PREFLIGHT] FIRST RAW-RGBA CARRIER PASS slot="
              <<static_cast<unsigned>(out.slot)
              <<" probeA="<<out.probe_a;
            if(out.probe_b_required)
                os<<" probeB="<<out.probe_b;
            reshade::log::message(
                reshade::log::level::info,
                os.str().c_str());
        }
    }else{
        ++g_carrier_miss;
    }
    return out;
}

} // namespace dsrrl::runtime::envspec
