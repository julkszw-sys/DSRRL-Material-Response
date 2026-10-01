#include "dsrrl/runtime/subsurface_pipeline_registry.hpp"
#include "dsrrl/runtime/subsurface_draw_runtime.hpp"
#include "dsrrl/runtime/upper_lower_draw_runtime.hpp"
#include "dsrrl/runtime/upper_lower_pipeline_registry.hpp"
#include "dsrrl/runtime/upper_lower_hemenv_draw_runtime.hpp"
#include "dsrrl/runtime/hemdir3_mode_transport.hpp"
#include "dsrrl/runtime/hemdir3_pipeline_registry.hpp"
#include "dsrrl/runtime/hemdir3_draw_runtime.hpp"
#include "dsrrl/operators/lightbank/hemdir3_b13_materializer.hpp"
#include "dsrrl/operators/lightbank/upper_lower_hemenv_materializer.hpp"
#include "dsrrl/operators/resource_bridges/subsurface_plain_target_materializer.hpp"

namespace dsrrl::runtime {

bool subsurface_visible_bridge_enabled() noexcept
{
    return false;
}

bool subsurface_receiver_observe_pipeline(
    std::uint64_t,
    const void *,
    std::size_t) noexcept
{
    return false;
}

void subsurface_receiver_forget_pipeline(
    std::uint64_t) noexcept
{
}

void subsurface_receiver_observe_bind(
    const void *,
    bool,
    std::uint64_t) noexcept
{
}

bool subsurface_receiver_bound(
    const void *,
    std::uint32_t &target_plain_receiver_id) noexcept
{
    target_plain_receiver_id = 0u;
    return false;
}

void subsurface_receiver_pipeline_reset() noexcept
{
}

subsurface_pipeline_telemetry
subsurface_receiver_pipeline_stats() noexcept
{
    return {};
}

subsurface_draw_runtime::subsurface_draw_runtime(
    core::renderer_core &core,
    material_response_draw_runtime &mr,
    material_resource_draw_runtime &resources) noexcept
    : core_(core),
      mr_(mr),
      resources_(resources)
{
}

bool subsurface_draw_runtime::prepare(
    reshade::api::command_list *,
    const operators::material_response::material_identity &,
    prepared_subsurface_draw &prepared) noexcept
{
    prepared = {};
    return false;
}

void subsurface_draw_runtime::release(
    prepared_subsurface_draw &prepared) noexcept
{
    prepared = {};
}

subsurface_draw_telemetry
subsurface_draw_runtime::telemetry() const noexcept
{
    return {};
}

void subsurface_draw_runtime::reset() noexcept
{
}

void upper_lower_selector_event_bridge(
    void *,
    void *,
    void *,
    void *) noexcept
{
}

void upper_lower_pmetal_material_event_bridge(
    void *,
    const operators::material_response::material_identity &) noexcept
{
}

upper_lower_draw_runtime::upper_lower_draw_runtime(
    core::renderer_core &core) noexcept
    : core_(core)
{
}

bool upper_lower_draw_runtime::install(
    bool) noexcept
{
    return false;
}

void upper_lower_draw_runtime::uninstall() noexcept
{
}

void upper_lower_draw_runtime::selector_event(
    void *,
    void *,
    void *,
    void *) noexcept
{
}

void upper_lower_draw_runtime::pmetal_material_event(
    void *,
    const operators::material_response::material_identity &) noexcept
{
}

bool upper_lower_draw_runtime::direct_producer_active() const noexcept
{
    return false;
}

bool upper_lower_draw_runtime::direct_producer_ready_for_draw() const noexcept
{
    return false;
}

bool upper_lower_draw_runtime::prepare_upper_lower_carrier(
    ID3D11DeviceContext *,
    prepared_upper_lower_draw &prepared) noexcept
{
    prepared = {};
    return false;
}

bool upper_lower_draw_runtime::prepare_draw_request(
    ID3D11DeviceContext *,
    std::uint32_t,
    prepared_upper_lower_draw &prepared) noexcept
{
    prepared = {};
    return false;
}

void upper_lower_draw_runtime::release_prepared_draw(
    prepared_upper_lower_draw &prepared) noexcept
{
    prepared = {};
}

bool upper_lower_draw_runtime::prepare_hemdir3_carrier(
    ID3D11DeviceContext *,
    prepared_hemdir3_carrier &prepared) noexcept
{
    prepared = {};
    return false;
}

void upper_lower_draw_runtime::release_hemdir3_carrier(
    prepared_hemdir3_carrier &prepared) noexcept
{
    prepared = {};
}

bool upper_lower_draw_runtime::selected_pmetal_env_source(
    pmetal_env_source &out) const noexcept
{
    out = {};
    return false;
}

pmetal_env_source_diagnostic
upper_lower_draw_runtime::pmetal_source_diagnostic() const noexcept
{
    return {};
}

pmetal_draw_token_frontier
upper_lower_draw_runtime::pmetal_draw_token_state() const noexcept
{
    return {};
}

void upper_lower_draw_runtime::consume_draw_selection() noexcept
{
}

void upper_lower_draw_runtime::on_destroy_device(
    reshade::api::device *) noexcept
{
}

upper_lower_telemetry
upper_lower_draw_runtime::telemetry() const noexcept
{
    return {};
}

void upper_lower_draw_runtime::reset() noexcept
{
}

bool upper_lower_receiver_attest_created_code(
    const void *,
    std::size_t,
    const upper_lower_receiver_identity &) noexcept
{
    return false;
}

bool upper_lower_receiver_observe_pipeline(
    std::uint64_t,
    const void *,
    std::size_t) noexcept
{
    return false;
}

void upper_lower_receiver_forget_pipeline(
    std::uint64_t) noexcept
{
}

void upper_lower_receiver_observe_bind(
    const void *,
    bool,
    std::uint64_t) noexcept
{
}

bool upper_lower_receiver_bound(
    const void *,
    upper_lower_receiver_identity &identity) noexcept
{
    identity = {};
    return false;
}

void upper_lower_receiver_pipeline_reset() noexcept
{
}

upper_lower_pipeline_telemetry
upper_lower_receiver_pipeline_stats() noexcept
{
    return {};
}

upper_lower_hemenv_draw_runtime::
upper_lower_hemenv_draw_runtime(
    core::renderer_core &core,
    upper_lower_draw_runtime &lightbank) noexcept
    : core_(core),
      lightbank_(lightbank)
{
}

upper_lower_hemenv_draw_runtime::
~upper_lower_hemenv_draw_runtime() = default;

void upper_lower_hemenv_draw_runtime::on_init_device(
    reshade::api::device *) noexcept
{
}

void upper_lower_hemenv_draw_runtime::on_destroy_device(
    reshade::api::device *) noexcept
{
}

bool upper_lower_hemenv_draw_runtime::register_replacement(
    const operators::lightbank::
        upper_lower_hemenv_materialize_outcome &,
    const void *,
    std::size_t) noexcept
{
    return false;
}

bool upper_lower_hemenv_draw_runtime::prepare_draw_request(
    reshade::api::command_list *,
    const upper_lower_receiver_identity &,
    bool,
    prepared_upper_lower_hemenv_draw &prepared) noexcept
{
    prepared = {};
    return false;
}

void upper_lower_hemenv_draw_runtime::release_prepared_draw(
    prepared_upper_lower_hemenv_draw &prepared) noexcept
{
    prepared = {};
}

upper_lower_hemenv_draw_telemetry
upper_lower_hemenv_draw_runtime::telemetry() const noexcept
{
    return {};
}

void upper_lower_hemenv_draw_runtime::reset() noexcept
{
}

namespace hemdir3_mode_transport {

bool install() noexcept
{
    return false;
}

void uninstall() noexcept
{
}

void selector_begin(
    std::uint32_t) noexcept
{
}

bool snapshot(
    std::uint32_t &effective_mode) noexcept
{
    effective_mode = 0u;
    return false;
}

void consume_draw_selection() noexcept
{
}

telemetry status() noexcept
{
    return {};
}

void reset_stats() noexcept
{
}

} // namespace hemdir3_mode_transport

bool hemdir3_receiver_attest_created_code(
    const void *,
    std::size_t,
    const hemdir3_receiver_identity &) noexcept
{
    return false;
}

bool hemdir3_receiver_observe_pipeline(
    std::uint64_t,
    const void *,
    std::size_t) noexcept
{
    return false;
}

void hemdir3_receiver_forget_pipeline(
    std::uint64_t) noexcept
{
}

void hemdir3_receiver_observe_bind(
    const void *,
    bool,
    std::uint64_t) noexcept
{
}

bool hemdir3_receiver_bound(
    const void *,
    hemdir3_receiver_identity &identity) noexcept
{
    identity = {};
    return false;
}

void hemdir3_receiver_pipeline_reset() noexcept
{
}

hemdir3_pipeline_telemetry
hemdir3_receiver_pipeline_stats() noexcept
{
    return {};
}

hemdir3_draw_runtime::hemdir3_draw_runtime(
    core::renderer_core &core,
    upper_lower_draw_runtime &lightbank) noexcept
    : core_(core),
      lightbank_(lightbank)
{
}

hemdir3_draw_runtime::~hemdir3_draw_runtime() = default;

void hemdir3_draw_runtime::on_init_device(
    reshade::api::device *) noexcept
{
}

void hemdir3_draw_runtime::on_destroy_device(
    reshade::api::device *) noexcept
{
}

bool hemdir3_draw_runtime::register_replacement(
    const operators::lightbank::
        hemdir3_b13_materialize_outcome &,
    const void *,
    std::size_t) noexcept
{
    return false;
}

bool hemdir3_draw_runtime::prepare_draw_request(
    reshade::api::command_list *,
    const hemdir3_receiver_identity &,
    const operators::material_response::material_identity &,
    prepared_hemdir3_draw &prepared) noexcept
{
    prepared = {};
    return false;
}

void hemdir3_draw_runtime::release_prepared_draw(
    prepared_hemdir3_draw &prepared) noexcept
{
    prepared = {};
}

hemdir3_draw_telemetry
hemdir3_draw_runtime::telemetry() const noexcept
{
    return {};
}

void hemdir3_draw_runtime::reset() noexcept
{
}

} // namespace dsrrl::runtime

namespace dsrrl::operators::lightbank {

hemdir3_b13_materialize_outcome
materialize_hemdir3_b13_receiver(
    const core::feature_registry &,
    const std::uint8_t *,
    std::size_t,
    std::vector<std::uint8_t> &output) noexcept
{
    output.clear();
    return {};
}

upper_lower_hemenv_materialize_outcome
materialize_upper_lower_hemenv_receiver(
    const core::feature_registry &,
    const std::uint8_t *,
    std::size_t,
    std::vector<std::uint8_t> &output) noexcept
{
    output.clear();
    return {};
}

upper_lower_hemenv_materialize_outcome
augment_upper_lower_hemenv_verified_base(
    const std::uint8_t *,
    std::size_t,
    const std::uint8_t *,
    std::size_t,
    std::uint32_t,
    std::vector<std::uint8_t> &output) noexcept
{
    output.clear();
    return {};
}

} // namespace dsrrl::operators::lightbank

namespace dsrrl::operators::resource_bridges {

subsurface_plain_target_materialize_outcome
materialize_subsurface_plain_target(
    const std::uint8_t *,
    std::size_t,
    std::vector<std::uint8_t> &target) noexcept
{
    target.clear();
    return {};
}

} // namespace dsrrl::operators::resource_bridges
