#include "dsrrl/runtime/dof_preflight.hpp"

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <reshade.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <sstream>

namespace dsrrl::runtime::dof {
namespace {

namespace hashing = operators::legacy_plan::hashing;
using role = operators::dof::retained_shader_role;

struct pipeline_slot {
    std::atomic<std::uint64_t> handle{0u};
    std::atomic<std::uint32_t> encoded_role{0u};
};

inline constexpr std::size_t k_pipeline_slot_count = 256u;
inline constexpr std::size_t k_pipeline_probe_limit = 4u;
static_assert((k_pipeline_slot_count & (k_pipeline_slot_count - 1u)) == 0u);

std::array<pipeline_slot, k_pipeline_slot_count> g_pipeline_slots{};
std::atomic<std::uintptr_t> g_registry_device{0u};
std::atomic<std::uint64_t> g_registry_overflow{0u};
std::atomic<std::uint64_t> g_init_pipeline_events{0};
std::atomic<std::uint64_t> g_exact_pipeline_hits{0};
std::atomic<std::uint64_t> g_bind_hits{0};
std::atomic<std::uint64_t> g_bind_misses{0};
std::atomic<std::uint32_t> g_seen_role_mask{0};
std::atomic<std::uint64_t> g_present_count{0};

thread_local reshade::api::command_list *g_bound_command = nullptr;
thread_local role g_bound_role = role::count;
thread_local bool g_bound_exact = false;
thread_local std::uint32_t g_bind_miss_sample_counter = 0u;

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


constexpr std::uint32_t encode_role(role value) noexcept
{
    return static_cast<std::uint32_t>(value) + 1u;
}

constexpr role decode_role(std::uint32_t value) noexcept
{
    return value == 0u ? role::count :
        static_cast<role>(value - 1u);
}

constexpr std::uint64_t mix_handle(std::uint64_t value) noexcept
{
    value ^= value >> 33u;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33u;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33u;
    return value;
}

constexpr std::size_t slot_index(
    std::uint64_t handle,
    std::size_t probe) noexcept
{
    return static_cast<std::size_t>(
        (mix_handle(handle) + probe) &
        (k_pipeline_slot_count - 1u));
}

void clear_pipeline_registry() noexcept
{
    for (auto &slot : g_pipeline_slots) {
        slot.encoded_role.store(0u, std::memory_order_relaxed);
        slot.handle.store(0u, std::memory_order_relaxed);
    }
}

bool register_pipeline_handle(
    std::uint64_t handle,
    role selected) noexcept
{
    if (handle == 0u || selected == role::count)
        return false;

    for (std::size_t probe = 0u;
         probe < k_pipeline_probe_limit;
         ++probe) {
        auto &slot =
            g_pipeline_slots[slot_index(handle, probe)];
        auto current =
            slot.handle.load(std::memory_order_acquire);

        if (current == handle) {
            slot.encoded_role.store(
                encode_role(selected),
                std::memory_order_release);
            return true;
        }

        if (current == 0u) {
            std::uint64_t expected = 0u;
            if (slot.handle.compare_exchange_strong(
                    expected,
                    handle,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                slot.encoded_role.store(
                    encode_role(selected),
                    std::memory_order_release);
                return true;
            }
        }
    }

    ++g_registry_overflow;
    return false;
}

void unregister_pipeline_handle(
    std::uint64_t handle) noexcept
{
    if (handle == 0u)
        return;

    for (std::size_t probe = 0u;
         probe < k_pipeline_probe_limit;
         ++probe) {
        auto &slot =
            g_pipeline_slots[slot_index(handle, probe)];
        if (slot.handle.load(
                std::memory_order_acquire) != handle)
            continue;

        slot.encoded_role.store(
            0u,
            std::memory_order_release);
        slot.handle.store(
            0u,
            std::memory_order_release);
        return;
    }
}

role lookup_pipeline_handle(
    std::uint64_t handle) noexcept
{
    if (handle == 0u)
        return role::count;

    for (std::size_t probe = 0u;
         probe < k_pipeline_probe_limit;
         ++probe) {
        const auto &slot =
            g_pipeline_slots[slot_index(handle, probe)];
        if (slot.handle.load(
                std::memory_order_acquire) != handle)
            continue;

        return decode_role(
            slot.encoded_role.load(
                std::memory_order_acquire));
    }

    return role::count;
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

    const auto device_value =
        reinterpret_cast<std::uintptr_t>(device);
    auto registry_device =
        g_registry_device.load(std::memory_order_acquire);
    if (registry_device == 0u) {
        std::uintptr_t expected = 0u;
        if (g_registry_device.compare_exchange_strong(
                expected,
                device_value,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
            registry_device = device_value;
        else
            registry_device = expected;
    }
    if (registry_device != device_value)
        return;

    if (!register_pipeline_handle(
            pipeline.handle,
            signature->role))
        return;

    g_seen_role_mask.fetch_or(
        role_bit(signature->role),
        std::memory_order_release);
    ++g_exact_pipeline_hits;
}

void on_destroy_pipeline(
    reshade::api::device *,
    reshade::api::pipeline pipeline)
{
    unregister_pipeline_handle(pipeline.handle);
}

void on_destroy_device(reshade::api::device *device)
{
    const auto device_value =
        reinterpret_cast<std::uintptr_t>(device);
    auto expected = device_value;
    if (g_registry_device.compare_exchange_strong(
            expected,
            0u,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        clear_pipeline_registry();
        g_seen_role_mask.store(
            0u,
            std::memory_order_release);
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
    if (g_seen_role_mask.load(
            std::memory_order_acquire) != 0u)
        selected = lookup_pipeline_handle(pipeline.handle);

    g_bound_command = cmd_list;
    g_bound_role = selected;
    g_bound_exact = selected != role::count;

    if (g_bound_exact) {
        ++g_bind_hits;
    } else {
        // Telemetry must not become the hot path. Sample one miss per 1024
        // non-DoF pixel-pipeline binds instead of issuing a global atomic RMW
        // for every unrelated draw.
        ++g_bind_miss_sample_counter;
        if ((g_bind_miss_sample_counter & 0x3ffu) == 0u)
            ++g_bind_misses;
    }
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
       << " registry_overflow="
       << g_registry_overflow.load()
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
        g_registry_overflow.store(0);
        g_registry_device.store(0);
        clear_pipeline_registry();

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

    clear_pipeline_registry();
    g_registry_device.store(
        0u,
        std::memory_order_release);

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
