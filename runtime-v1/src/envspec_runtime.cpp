#include "dsrrl/runtime/envspec_runtime.hpp"

#include "dsrrl/operators/env_spec/legacy_resource_bridge.hpp"
#include "dsrrl/sha256.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

using namespace reshade::api;

namespace dsrrl::runtime::envspec {
namespace {

constexpr std::uint32_t k_dsr_size=256u;
constexpr std::uint32_t k_faces=6u;
constexpr std::uint32_t k_dsr_levels=8u;
constexpr std::size_t k_dsr_tight_bc6h_bytes=524352u;

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

std::mutex g_mutex;
std::unordered_map<std::uint64_t,native_resource_record> g_native_resources;
std::unordered_map<std::uint64_t,std::uint64_t> g_resource_by_view;
std::unordered_map<std::uint64_t,command_bindings> g_command_bindings;

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
std::atomic<std::uint64_t> g_present{0};
std::atomic<bool> g_first_pair_logged{false};

std::uint64_t command_id(command_list *cmd) noexcept
{
    return static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(cmd));
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
      <<" pair_miss="<<g_draw_pair_miss.load();
    reshade::log::message(
        reshade::log::level::info,
        os.str().c_str());
}

} // namespace

bool register_runtime() noexcept
{
    try{
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

    std::lock_guard lock(g_mutex);
    g_native_resources.clear();
    g_resource_by_view.clear();
    g_command_bindings.clear();
}

identity_snapshot observe_draw(
    command_list *cmd,
    const operators::material_response::mtd_envspec_semantics &material,
    bool exact_material) noexcept
{
    identity_snapshot out;
    out.exact_material=
        exact_material && material.exact_identity_match;
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
    if(state.srv[1].valid){
        const auto probe=probe_for_view(state.srv[1].view);
        if(probe.has_value()){
            out.probe_b_ready=true;
            out.probe_b=probe.value();
        }
    }

    if(out.ready()){
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
    }
    return out;
}

} // namespace dsrrl::runtime::envspec
