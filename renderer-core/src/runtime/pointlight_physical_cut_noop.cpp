// R43 RC1 descendant: compile-only inert ABI for physically cut PointLight islands.
// The real fixed/clustered R40 and clustered-Spc producer, pipeline and draw
// implementations are deliberately excluded from the integrated target.
// These definitions preserve linkage for common Material Response/P_Metal routing
// without installing hooks, modifying shaders, allocating carriers or replaying draws.
#include "dsrrl/runtime/fixed_pointlight_draw_runtime.hpp"
#include "dsrrl/runtime/fixed_pointlight_pipeline_runtime.hpp"
#include "dsrrl/runtime/clustered_pnts_draw_runtime.hpp"
#include "dsrrl/runtime/clustered_pnts_pipeline_runtime.hpp"

namespace dsrrl::runtime {

void fixed_pointlight_selector_event_bridge(void *) noexcept {}
bool fixed_pointlight_draw_runtime::install() noexcept { return false; }
void fixed_pointlight_draw_runtime::uninstall() noexcept {}
void fixed_pointlight_draw_runtime::selector_event(void *) noexcept {}
bool fixed_pointlight_draw_runtime::prepare_t19(
    ID3D11DeviceContext *, std::uint8_t,
    prepared_fixed_pointlight_draw &out) noexcept {
    out = {}; return false;
}
void fixed_pointlight_draw_runtime::release_prepared_draw(
    prepared_fixed_pointlight_draw &out) noexcept { out = {}; }
void fixed_pointlight_draw_runtime::consume_draw_selection() noexcept {}
void fixed_pointlight_draw_runtime::on_destroy_device(
    reshade::api::device *) noexcept {}
fixed_pointlight_telemetry fixed_pointlight_draw_runtime::telemetry() const noexcept {
    return {};
}
void fixed_pointlight_draw_runtime::reset() noexcept {}

fixed_pointlight_pipeline_runtime::~fixed_pointlight_pipeline_runtime() = default;
bool fixed_pointlight_pipeline_runtime::register_candidate(
    reshade::api::device *,
    const operators::point_light::fixed_local_single_materialize_outcome &,
    const std::uint8_t *, std::size_t) noexcept { return false; }
bool fixed_pointlight_pipeline_runtime::on_init_pipeline(
    reshade::api::device *, std::uint32_t,
    const reshade::api::pipeline_subobject *,
    reshade::api::pipeline) noexcept { return false; }
bool fixed_pointlight_pipeline_runtime::on_bind_pipeline(
    reshade::api::command_list *, reshade::api::pipeline_stage,
    reshade::api::pipeline) noexcept { return false; }
void fixed_pointlight_pipeline_runtime::on_destroy_pipeline(
    reshade::api::pipeline) noexcept {}
void fixed_pointlight_pipeline_runtime::on_destroy_device(
    reshade::api::device *) noexcept {}
bool fixed_pointlight_pipeline_runtime::pipeline_attested(
    std::uint64_t) const noexcept { return false; }
bool fixed_pointlight_pipeline_runtime::pipeline_attested_cached(
    std::uint64_t) const noexcept { return false; }
bool fixed_pointlight_pipeline_runtime::bound_light_count(
    reshade::api::command_list *, std::uint8_t &) const noexcept { return false; }
bool fixed_pointlight_pipeline_runtime::prepare_bound_shader(
    reshade::api::command_list *,
    prepared_fixed_pointlight_shader &out) noexcept {
    out = {}; return false;
}
void fixed_pointlight_pipeline_runtime::release_prepared_shader(
    prepared_fixed_pointlight_shader &out) noexcept { out = {}; }
fixed_pointlight_pipeline_telemetry
fixed_pointlight_pipeline_runtime::telemetry() const noexcept { return {}; }
void fixed_pointlight_pipeline_runtime::reset() noexcept {}

void clustered_pnts_builder_event_bridge(void *, void *) noexcept {}
void clustered_pnts_selector_event_bridge(void *, const void *) noexcept {}
void clustered_pnts_selector_source_event_bridge() noexcept {}
void clustered_pnts_selector_identity_event_bridge(
    const operators::material_response::material_identity &, bool) noexcept {}
bool clustered_pnts_draw_runtime::install() noexcept { return false; }
void clustered_pnts_draw_runtime::uninstall() noexcept {}
void clustered_pnts_draw_runtime::builder_event(void *, void *) noexcept {}
void clustered_pnts_draw_runtime::selector_event(
    void *, const void *) noexcept {}
void clustered_pnts_draw_runtime::selector_source_event() noexcept {}
void clustered_pnts_draw_runtime::frame_event(std::uint64_t) noexcept {}
void clustered_pnts_draw_runtime::selector_identity_event(
    const operators::material_response::material_identity &, bool) noexcept {}
bool clustered_pnts_draw_runtime::current_draw_authority(
    bool, operators::material_response::material_identity &,
    operators::material_response::decision &) const noexcept { return false; }
bool clustered_pnts_draw_runtime::prepare_sidecar(
    ID3D11DeviceContext *,
    const operators::material_response::decision &,
    prepared_clustered_pnts_draw &out) noexcept {
    out = {}; return false;
}
void clustered_pnts_draw_runtime::release_prepared_draw(
    prepared_clustered_pnts_draw &out) noexcept { out = {}; }
void clustered_pnts_draw_runtime::consume_draw_selection() noexcept {}
void clustered_pnts_draw_runtime::on_destroy_device(
    reshade::api::device *) noexcept {}
clustered_pnts_telemetry clustered_pnts_draw_runtime::telemetry() const noexcept {
    return {};
}
void clustered_pnts_draw_runtime::reset() noexcept {}

clustered_pnts_pipeline_runtime::~clustered_pnts_pipeline_runtime() = default;
bool clustered_pnts_pipeline_runtime::register_candidate(
    reshade::api::device *,
    const operators::point_light::clustered_pnts_direct_materialize_outcome &,
    const std::uint8_t *, std::size_t,
    const std::uint8_t *, std::size_t) noexcept { return false; }
bool clustered_pnts_pipeline_runtime::on_init_pipeline(
    reshade::api::device *, std::uint32_t,
    const reshade::api::pipeline_subobject *,
    reshade::api::pipeline) noexcept { return false; }
bool clustered_pnts_pipeline_runtime::on_bind_pipeline(
    reshade::api::command_list *, reshade::api::pipeline_stage,
    reshade::api::pipeline) noexcept { return false; }
void clustered_pnts_pipeline_runtime::on_destroy_pipeline(
    reshade::api::pipeline) noexcept {}
void clustered_pnts_pipeline_runtime::on_destroy_device(
    reshade::api::device *) noexcept {}
bool clustered_pnts_pipeline_runtime::pipeline_attested(
    std::uint64_t) const noexcept { return false; }
bool clustered_pnts_pipeline_runtime::pipeline_attested_cached(
    std::uint64_t) const noexcept { return false; }
bool clustered_pnts_pipeline_runtime::bound_metadata(
    reshade::api::command_list *, bool &, bool &) const noexcept {
    return false;
}
bool clustered_pnts_pipeline_runtime::prepare_bound_shader(
    reshade::api::command_list *,
    prepared_clustered_pnts_shader &out) noexcept {
    out = {}; return false;
}
void clustered_pnts_pipeline_runtime::release_prepared_shader(
    prepared_clustered_pnts_shader &out) noexcept { out = {}; }
clustered_pnts_pipeline_telemetry
clustered_pnts_pipeline_runtime::telemetry() const noexcept { return {}; }
void clustered_pnts_pipeline_runtime::reset() noexcept {}

} // namespace dsrrl::runtime
