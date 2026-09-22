#include "runtime_core_v2_reshade.hpp"

#include <reshade.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime_v2::diagnostic {
namespace {

constexpr std::array<std::uint32_t, 5> k_slots = {0, 2, 10, 12, 14};

struct diagnostic_counters {
    std::array<std::atomic<std::uint64_t>, k_slots.size()> descriptor_updates{};
    std::array<std::atomic<std::uint64_t>, k_slots.size()> draw_resolves{};
    std::atomic<std::uint64_t> draws_with_pipeline{0};
    std::atomic<std::uint64_t> envspec_pair_draws{0};
    std::atomic<std::uint64_t> presents{0};
};

diagnostic_counters g_diag;

std::size_t slot_index(std::uint32_t slot) noexcept
{
    for (std::size_t i = 0; i < k_slots.size(); ++i)
        if (k_slots[i] == slot)
            return i;
    return k_slots.size();
}

std::uint64_t command_id(reshade::api::command_list *cmd_list) noexcept
{
    return static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(cmd_list));
}

void observe_descriptor_updates(
    reshade::api::command_list *,
    reshade::api::shader_stage stages,
    reshade::api::pipeline_layout,
    std::uint32_t,
    const reshade::api::descriptor_table_update &update)
{
    if ((stages & reshade::api::shader_stage::pixel) != reshade::api::shader_stage::pixel)
        return;

    if (update.type != reshade::api::descriptor_type::shader_resource_view &&
        update.type != reshade::api::descriptor_type::sampler_with_resource_view)
        return;

    if (update.count == 0 || update.descriptors == nullptr)
        return;

    for (std::uint32_t i = 0; i < update.count; ++i) {
        const std::uint32_t slot = update.binding + i;
        const std::size_t index = slot_index(slot);
        if (index == k_slots.size())
            continue;

        std::uint64_t view = 0;
        if (update.type == reshade::api::descriptor_type::shader_resource_view) {
            const auto *descriptors =
                static_cast<const reshade::api::resource_view *>(update.descriptors);
            view = descriptors[i].handle;
        }
        else {
            const auto *descriptors =
                static_cast<const reshade::api::sampler_with_resource_view *>(update.descriptors);
            view = descriptors[i].view.handle;
        }

        if (view != 0)
            ++g_diag.descriptor_updates[index];
    }
}

void observe_draw(reshade::api::command_list *cmd_list)
{
    const std::uint64_t command = command_id(cmd_list);
    auto &tracker = global_tracker();

    if (tracker.resolve_bound_pipeline(command).has_value())
        ++g_diag.draws_with_pipeline;

    const auto t0 = tracker.resolve_bound_pixel_shader_resource(
        command, 0, operator_kind::diffuse);
    const auto t2 = tracker.resolve_bound_pixel_shader_resource(
        command, 2, operator_kind::normal);
    const auto t10 = tracker.resolve_bound_pixel_shader_resource(
        command, 10, operator_kind::specrgb);
    const auto t12 = tracker.resolve_bound_pixel_shader_resource(
        command, 12, operator_kind::envspec);
    const auto t14 = tracker.resolve_bound_pixel_shader_resource(
        command, 14, operator_kind::envspec);

    if (t0) ++g_diag.draw_resolves[0];
    if (t2) ++g_diag.draw_resolves[1];
    if (t10) ++g_diag.draw_resolves[2];
    if (t12) ++g_diag.draw_resolves[3];
    if (t14) ++g_diag.draw_resolves[4];
    if (t12 && t14) ++g_diag.envspec_pair_draws;
}

bool on_draw(
    reshade::api::command_list *cmd_list,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t)
{
    observe_draw(cmd_list);
    return false;
}

bool on_draw_indexed(
    reshade::api::command_list *cmd_list,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::int32_t,
    std::uint32_t)
{
    observe_draw(cmd_list);
    return false;
}

void log_snapshot(const runtime_snapshot &snapshot)
{
    const std::uint64_t present = ++g_diag.presents;
    if (present != 1 && (present % 300) != 0)
        return;

    char line[1024] = {};

    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL RUNTIME CORE V2] P=%llu LIVE res=%llu logical=%llu view=%llu pipe=%llu cmd=%llu "
        "SRV_UPD t0=%llu t2=%llu t10=%llu t12=%llu t14=%llu "
        "DRAW_RESOLVE t0=%llu t2=%llu t10=%llu t12=%llu t14=%llu env12+14=%llu pipeDraw=%llu",
        static_cast<unsigned long long>(present),
        static_cast<unsigned long long>(snapshot.live_resources),
        static_cast<unsigned long long>(snapshot.live_logical_resources),
        static_cast<unsigned long long>(snapshot.live_views),
        static_cast<unsigned long long>(snapshot.live_pipelines),
        static_cast<unsigned long long>(snapshot.live_commands),
        static_cast<unsigned long long>(g_diag.descriptor_updates[0].load()),
        static_cast<unsigned long long>(g_diag.descriptor_updates[1].load()),
        static_cast<unsigned long long>(g_diag.descriptor_updates[2].load()),
        static_cast<unsigned long long>(g_diag.descriptor_updates[3].load()),
        static_cast<unsigned long long>(g_diag.descriptor_updates[4].load()),
        static_cast<unsigned long long>(g_diag.draw_resolves[0].load()),
        static_cast<unsigned long long>(g_diag.draw_resolves[1].load()),
        static_cast<unsigned long long>(g_diag.draw_resolves[2].load()),
        static_cast<unsigned long long>(g_diag.draw_resolves[3].load()),
        static_cast<unsigned long long>(g_diag.draw_resolves[4].load()),
        static_cast<unsigned long long>(g_diag.envspec_pair_draws.load()),
        static_cast<unsigned long long>(g_diag.draws_with_pipeline.load()));

    reshade::log::message(reshade::log::level::info, line);

    const auto &diffuse = snapshot.operators[static_cast<std::size_t>(operator_kind::diffuse)];
    const auto &normal = snapshot.operators[static_cast<std::size_t>(operator_kind::normal)];
    const auto &spec = snapshot.operators[static_cast<std::size_t>(operator_kind::specrgb)];
    const auto &env = snapshot.operators[static_cast<std::size_t>(operator_kind::envspec)];

    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL RUNTIME CORE V2] IDENTITY miss/ambiguous "
        "DIF=%llu/%llu NRM=%llu/%llu SPC=%llu/%llu ENV=%llu/%llu "
        "STALE DIF=%llu NRM=%llu SPC=%llu ENV=%llu",
        static_cast<unsigned long long>(diffuse.logical_miss),
        static_cast<unsigned long long>(diffuse.logical_ambiguous),
        static_cast<unsigned long long>(normal.logical_miss),
        static_cast<unsigned long long>(normal.logical_ambiguous),
        static_cast<unsigned long long>(spec.logical_miss),
        static_cast<unsigned long long>(spec.logical_ambiguous),
        static_cast<unsigned long long>(env.logical_miss),
        static_cast<unsigned long long>(env.logical_ambiguous),
        static_cast<unsigned long long>(diffuse.stale_view),
        static_cast<unsigned long long>(normal.stale_view),
        static_cast<unsigned long long>(spec.stale_view),
        static_cast<unsigned long long>(env.stale_view));

    reshade::log::message(reshade::log::level::info, line);
}

} // namespace
} // namespace dsrrl::runtime_v2::diagnostic

extern "C" __declspec(dllexport) const char *NAME =
    "DSRRL Runtime Core V2 Diagnostic";
extern "C" __declspec(dllexport) const char *DESCRIPTION =
    "Pixel-inert DSRRL runtime/resource identity diagnostics for ReShade API 20.";

extern "C" __declspec(dllexport) bool AddonInit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    if (!reshade::register_addon(addon_module, reshade_module))
        return false;

    using namespace dsrrl::runtime_v2;
    using namespace dsrrl::runtime_v2::diagnostic;

    set_snapshot_sink(log_snapshot);
    register_reshade_events();

    reshade::register_event<reshade::addon_event::push_descriptors>(
        observe_descriptor_updates);
    reshade::register_event<reshade::addon_event::draw>(on_draw);
    reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);

    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL RUNTIME CORE V2] READY API20 PIXEL-INERT");

    return true;
}

extern "C" __declspec(dllexport) void AddonUninit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    using namespace dsrrl::runtime_v2;
    using namespace dsrrl::runtime_v2::diagnostic;

    reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
    reshade::unregister_event<reshade::addon_event::draw>(on_draw);
    reshade::unregister_event<reshade::addon_event::push_descriptors>(
        observe_descriptor_updates);

    unregister_reshade_events();
    set_snapshot_sink(nullptr);

    reshade::unregister_addon(addon_module, reshade_module);
}
