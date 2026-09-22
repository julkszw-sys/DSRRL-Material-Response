#include "runtime_core_v2_reshade.hpp"

#include <reshade.hpp>

#include <cstdint>

namespace dsrrl::runtime_v2 {
namespace {

tracker g_tracker;
receiver_classifier_fn g_receiver_classifier = nullptr;
snapshot_sink_fn g_snapshot_sink = nullptr;
reshade::api::device *g_device = nullptr;

std::uint64_t command_id(reshade::api::command_list *cmd_list) noexcept
{
    return static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(cmd_list));
}

std::uint64_t fnv1a64(const void *data, std::size_t size) noexcept
{
    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;

    std::uint64_t hash = offset;
    const auto *bytes = static_cast<const std::uint8_t *>(data);

    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= prime;
    }

    return hash;
}

bool accepts(reshade::api::device *device) noexcept
{
    return g_device == nullptr || g_device == device;
}

void on_init_device(reshade::api::device *device)
{
    if (g_device != nullptr && g_device != device)
        g_tracker.reset_all();

    g_device = device;
}

void on_destroy_device(reshade::api::device *device)
{
    if (g_device == device) {
        g_device = nullptr;
        g_tracker.reset_all();
    }
}

void on_init_command_list(reshade::api::command_list *cmd_list)
{
    g_tracker.init_command(command_id(cmd_list));
}

void on_destroy_command_list(reshade::api::command_list *cmd_list)
{
    g_tracker.destroy_command(command_id(cmd_list));
}

void on_init_resource(
    reshade::api::device *device,
    const reshade::api::resource_desc &,
    const reshade::api::subresource_data *,
    reshade::api::resource_usage,
    reshade::api::resource resource)
{
    if (!accepts(device))
        return;

    // Description/logical identity are filled by operator-specific routing.
    // The common layer owns lifetime/generation only.
    g_tracker.init_resource(resource.handle, 0, 0);
}

void on_destroy_resource(
    reshade::api::device *device,
    reshade::api::resource resource)
{
    if (!accepts(device))
        return;

    g_tracker.destroy_resource(resource.handle);
}

void on_init_resource_view(
    reshade::api::device *device,
    reshade::api::resource resource,
    reshade::api::resource_usage,
    const reshade::api::resource_view_desc &,
    reshade::api::resource_view view)
{
    if (!accepts(device))
        return;

    g_tracker.init_view(view.handle, resource.handle);
}

void on_destroy_resource_view(
    reshade::api::device *device,
    reshade::api::resource_view view)
{
    if (!accepts(device))
        return;

    g_tracker.destroy_view(view.handle);
}

void on_init_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects,
    reshade::api::pipeline pipeline)
{
    if (!accepts(device))
        return;

    const reshade::api::shader_desc *pixel_shader = nullptr;

    for (std::uint32_t i = 0; i < subobject_count; ++i) {
        if (subobjects[i].type == reshade::api::pipeline_subobject_type::pixel_shader) {
            pixel_shader =
                static_cast<const reshade::api::shader_desc *>(subobjects[i].data);
            break;
        }
    }

    std::uint64_t fast_hash = 0;
    std::uint32_t receiver_id = 0;
    std::uint64_t consumer_family_hash = 0;
    bool confirmed = false;

    if (pixel_shader != nullptr &&
        pixel_shader->code != nullptr &&
        pixel_shader->code_size != 0) {
        fast_hash = fnv1a64(pixel_shader->code, pixel_shader->code_size);

        if (g_receiver_classifier != nullptr) {
            confirmed = g_receiver_classifier(
                pixel_shader->code,
                pixel_shader->code_size,
                fast_hash,
                &receiver_id,
                &consumer_family_hash);
        }
    }

    g_tracker.init_pipeline(
        pipeline.handle,
        fast_hash,
        receiver_id,
        consumer_family_hash,
        confirmed);
}

void on_destroy_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline pipeline)
{
    if (!accepts(device))
        return;

    g_tracker.destroy_pipeline(pipeline.handle);
}

void on_bind_pipeline(
    reshade::api::command_list *cmd_list,
    reshade::api::pipeline_stage stages,
    reshade::api::pipeline pipeline)
{
    if ((stages & reshade::api::pipeline_stage::pixel_shader) == 0)
        return;

    g_tracker.bind_pipeline(command_id(cmd_list), pipeline.handle);
}

bool on_draw(
    reshade::api::command_list *cmd_list,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t,
    std::uint32_t)
{
    g_tracker.begin_draw(command_id(cmd_list));
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
    g_tracker.begin_draw(command_id(cmd_list));
    return false;
}

void on_present(
    reshade::api::command_queue *,
    reshade::api::swapchain *,
    const reshade::api::rect *,
    const reshade::api::rect *,
    std::uint32_t,
    const reshade::api::rect *)
{
    const runtime_snapshot snapshot = g_tracker.seal_frame();

    // The sink must remain bounded. Do not perform per-draw disk IO here.
    if (g_snapshot_sink != nullptr)
        g_snapshot_sink(snapshot);
}

} // namespace

tracker &global_tracker() noexcept
{
    return g_tracker;
}

void set_receiver_classifier(receiver_classifier_fn fn) noexcept
{
    g_receiver_classifier = fn;
}

void set_snapshot_sink(snapshot_sink_fn fn) noexcept
{
    g_snapshot_sink = fn;
}

void register_reshade_events()
{
    reshade::register_event<reshade::addon_event::init_device>(on_init_device);
    reshade::register_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::register_event<reshade::addon_event::init_command_list>(on_init_command_list);
    reshade::register_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
    reshade::register_event<reshade::addon_event::init_resource>(on_init_resource);
    reshade::register_event<reshade::addon_event::destroy_resource>(on_destroy_resource);
    reshade::register_event<reshade::addon_event::init_resource_view>(on_init_resource_view);
    reshade::register_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);
    reshade::register_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
    reshade::register_event<reshade::addon_event::destroy_pipeline>(on_destroy_pipeline);
    reshade::register_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
    reshade::register_event<reshade::addon_event::draw>(on_draw);
    reshade::register_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
    reshade::register_event<reshade::addon_event::present>(on_present);
}

void unregister_reshade_events()
{
    reshade::unregister_event<reshade::addon_event::present>(on_present);
    reshade::unregister_event<reshade::addon_event::draw_indexed>(on_draw_indexed);
    reshade::unregister_event<reshade::addon_event::draw>(on_draw);
    reshade::unregister_event<reshade::addon_event::bind_pipeline>(on_bind_pipeline);
    reshade::unregister_event<reshade::addon_event::destroy_pipeline>(on_destroy_pipeline);
    reshade::unregister_event<reshade::addon_event::init_pipeline>(on_init_pipeline);
    reshade::unregister_event<reshade::addon_event::destroy_resource_view>(on_destroy_resource_view);
    reshade::unregister_event<reshade::addon_event::init_resource_view>(on_init_resource_view);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(on_destroy_resource);
    reshade::unregister_event<reshade::addon_event::init_resource>(on_init_resource);
    reshade::unregister_event<reshade::addon_event::destroy_command_list>(on_destroy_command_list);
    reshade::unregister_event<reshade::addon_event::init_command_list>(on_init_command_list);
    reshade::unregister_event<reshade::addon_event::destroy_device>(on_destroy_device);
    reshade::unregister_event<reshade::addon_event::init_device>(on_init_device);
}

} // namespace dsrrl::runtime_v2
