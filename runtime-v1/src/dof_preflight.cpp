#include "dsrrl/runtime/dof_preflight.hpp"

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <reshade.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace dsrrl::runtime::dof {
namespace {

namespace hashing = operators::legacy_plan::hashing;
using role = operators::dof::retained_shader_role;

struct registry_state {
    std::mutex mutex;
    std::unordered_map<std::uint64_t, role> pipelines;
    reshade::api::device *device = nullptr;
};

registry_state g_registry;
std::atomic<std::uint64_t> g_init_pipeline_events{0};
std::atomic<std::uint64_t> g_exact_pipeline_hits{0};
std::atomic<std::uint64_t> g_bind_hits{0};
std::atomic<std::uint64_t> g_bind_misses{0};
std::atomic<std::uint32_t> g_seen_role_mask{0};
std::atomic<std::uint64_t> g_present_count{0};

thread_local reshade::api::command_list *g_bound_command = nullptr;
thread_local role g_bound_role = role::count;
thread_local bool g_bound_exact = false;

constexpr std::uint32_t role_bit(role value) noexcept
{
    const auto i = static_cast<std::uint32_t>(value);
    return i < 32u ? (1u << i) : 0u;
}

bool seen(role value, std::uint32_t mask) noexcept
{
    return (mask & role_bit(value)) != 0u;
}

bool active_flat_set_seen(std::uint32_t mask) noexcept
{
    return
        seen(role::depth_copy_msaa, mask) &&
        seen(role::depth_copy_single_fragment, mask) &&
        seen(role::dof_rate_cb, mask) &&
        seen(role::downsample, mask) &&
        seen(role::gauss_x, mask) &&
        (seen(role::gauss_y_adv, mask) ||
         seen(role::near_rate, mask));
}

const reshade::api::shader_desc *find_shader(
    reshade::api::pipeline_subobject_type type,
    std::uint32_t count,
    const reshade::api::pipeline_subobject *subobjects) noexcept
{
    if (subobjects == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < count; ++i) {
        if (subobjects[i].type == type &&
            subobjects[i].count == 1u &&
            subobjects[i].data != nullptr)
            return static_cast<const reshade::api::shader_desc *>(
                subobjects[i].data);
    }

    return nullptr;
}

operators::dof::digest32 digest_of(
    const reshade::api::shader_desc &shader) noexcept
{
    operators::dof::digest32 out{};
    if (shader.code == nullptr || shader.code_size == 0u)
        return out;

    out.bytes = hashing::sha256(
        static_cast<const std::uint8_t *>(shader.code),
        shader.code_size);
    return out;
}

void on_init_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects,
    reshade::api::pipeline pipeline)
{
    ++g_init_pipeline_events;

    if (device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11 ||
        pipeline.handle == 0u)
        return;

    const auto *vs = find_shader(
        reshade::api::pipeline_subobject_type::vertex_shader,
        subobject_count,
        subobjects);
    const auto *ps = find_shader(
        reshade::api::pipeline_subobject_type::pixel_shader,
        subobject_count,
        subobjects);

    if (vs == nullptr || ps == nullptr ||
        vs->code == nullptr || ps->code == nullptr ||
        vs->code_size == 0u || ps->code_size == 0u)
        return;

    const auto vertex_sha = digest_of(*vs);
    const auto pixel_sha = digest_of(*ps);
    const auto *signature = operators::dof::find_retained_pipeline(
        vertex_sha, vs->code_size,
        pixel_sha, ps->code_size);

    if (signature == nullptr)
        return;

    {
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        if (g_registry.device != nullptr &&
            g_registry.device != device)
            return;
        g_registry.device = device;
        g_registry.pipelines[pipeline.handle] = signature->role;
    }

    g_seen_role_mask.fetch_or(
        role_bit(signature->role),
        std::memory_order_release);
    ++g_exact_pipeline_hits;
}

void on_destroy_pipeline(
    reshade::api::device *,
    reshade::api::pipeline pipeline)
{
    std::lock_guard<std::mutex> lock(g_registry.mutex);
    g_registry.pipelines.erase(pipeline.handle);
}

void on_destroy_device(reshade::api::device *device)
{
    std::lock_guard<std::mutex> lock(g_registry.mutex);
    if (g_registry.device == device) {
        g_registry.pipelines.clear();
        g_registry.device = nullptr;
    }
}

void on_bind_pipeline(
    reshade::api::command_list *cmd_list,
    reshade::api::pipeline_stage stages,
    reshade::api::pipeline pipeline)
{
    if (cmd_list == nullptr)
        return;

    const auto pixel_mask =
        static_cast<std::uint32_t>(
            reshade::api::pipeline_stage::pixel_shader);
    if ((static_cast<std::uint32_t>(stages) & pixel_mask) == 0u)
        return;

    role selected = role::count;
    {
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        const auto it = g_registry.pipelines.find(pipeline.handle);
        if (it != g_registry.pipelines.end())
            selected = it->second;
    }

    g_bound_command = cmd_list;
    g_bound_role = selected;
    g_bound_exact = selected != role::count;

    if (g_bound_exact)
        ++g_bind_hits;
    else
        ++g_bind_misses;
}

void on_present(
    reshade::api::command_queue *,
    reshade::api::swapchain *,
    const reshade::api::rect *,
    const reshade::api::rect *,
    std::uint32_t,
    const reshade::api::rect *)
{
    const auto n = ++g_present_count;
    if (n != 1u && (n % 300u) != 0u)
        return;

    const auto mask = g_seen_role_mask.load(std::memory_order_acquire);
    std::ostringstream os;
    os << "[DSRRL DoF preflight] LIVE exact_pipeline_hits="
       << g_exact_pipeline_hits.load()
       << " bind_hits=" << g_bind_hits.load()
       << " seen_mask=0x" << std::hex << mask << std::dec
       << " flat_set=" << (active_flat_set_seen(mask) ? 1 : 0)
       << " plain_dofrate="
       << (seen(role::dof_rate_plain, mask) ? 1 : 0)
       << " mutation=0";
    reshade::log::message(
        reshade::log::level::info,
        os.str().c_str());
}

} // namespace

bool register_preflight_runtime() noexcept
{
    try {
        g_init_pipeline_events.store(0);
        g_exact_pipeline_hits.store(0);
        g_bind_hits.store(0);
        g_bind_misses.store(0);
        g_seen_role_mask.store(0);
        g_present_count.store(0);

        reshade::register_event<reshade::addon_event::init_pipeline>(
            on_init_pipeline);
        reshade::register_event<reshade::addon_event::destroy_pipeline>(
            on_destroy_pipeline);
        reshade::register_event<reshade::addon_event::destroy_device>(
            on_destroy_device);
        reshade::register_event<reshade::addon_event::bind_pipeline>(
            on_bind_pipeline);
        reshade::register_event<reshade::addon_event::present>(
            on_present);
        return true;
    } catch (...) {
        return false;
    }
}

void unregister_preflight_runtime() noexcept
{
    reshade::unregister_event<reshade::addon_event::present>(
        on_present);
    reshade::unregister_event<reshade::addon_event::bind_pipeline>(
        on_bind_pipeline);
    reshade::unregister_event<reshade::addon_event::destroy_device>(
        on_destroy_device);
    reshade::unregister_event<reshade::addon_event::destroy_pipeline>(
        on_destroy_pipeline);
    reshade::unregister_event<reshade::addon_event::init_pipeline>(
        on_init_pipeline);

    {
        std::lock_guard<std::mutex> lock(g_registry.mutex);
        g_registry.pipelines.clear();
        g_registry.device = nullptr;
    }

    g_bound_command = nullptr;
    g_bound_role = role::count;
    g_bound_exact = false;
}

bool bound_retained_role(
    reshade::api::command_list *cmd_list,
    operators::dof::retained_shader_role &selected) noexcept
{
    if (cmd_list == nullptr ||
        !g_bound_exact ||
        g_bound_command != cmd_list)
        return false;

    selected = g_bound_role;
    return true;
}

preflight_telemetry telemetry() noexcept
{
    const auto mask =
        g_seen_role_mask.load(std::memory_order_acquire);

    return {
        g_init_pipeline_events.load(),
        g_exact_pipeline_hits.load(),
        g_bind_hits.load(),
        g_bind_misses.load(),
        mask,
        active_flat_set_seen(mask),
        seen(role::dof_rate_plain, mask)
    };
}

} // namespace dsrrl::runtime::dof
