#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/dof_ptde_draw_bridge_runtime.hpp"

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/operators/dof/dof_resource_contract.hpp"
#include "dsrrl/runtime/dof_authored_state_runtime.hpp"
#include "dsrrl/runtime/dof_host_depth_route_runtime.hpp"
#include "dsrrl/runtime/dof_preflight.hpp"
#include "dsrrl/runtime/dof_private_resource_runtime.hpp"
#include "dsrrl/runtime/dof_ptde_scheduler_runtime.hpp"
#include "dsrrl/runtime/dof_tonemap_handoff_runtime.hpp"

#include <reshade.hpp>

#include <Windows.h>
#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace dsrrl::runtime::dof {
namespace {

using role = operators::dof::retained_shader_role;
using result = scheduler_result;

struct pass_state_carriers {
    ID3D11SamplerState *color_sampler = nullptr;
    ID3D11SamplerState *depth_sampler = nullptr;
    bool ready = false;
};

struct sequence_state {
    host_dof_inputs host{};
    pass_state_carriers pass_state{};
    std::size_t next_pass = 0u;
    ID3D11ShaderResourceView *terminal = nullptr;
    bool active = false;
    bool complete = false;
};

core::renderer_core *g_core = nullptr;
std::atomic_bool g_opt_in{false};
std::atomic_bool g_armed{false};
std::atomic_bool g_quarantined{false};
std::atomic_bool g_registered{false};

std::atomic<std::uint64_t> g_first_pass_hits{0u};
std::atomic<std::uint64_t> g_sequences_started{0u};
std::atomic<std::uint64_t> g_sequences_completed{0u};
std::atomic<std::uint64_t> g_sequence_failures{0u};
std::atomic<std::uint64_t> g_dry_run_pass{0u};
std::atomic<std::uint64_t> g_visible_handoffs{0u};
std::atomic<std::uint64_t> g_tonemap_fallbacks{0u};
std::atomic<std::uint64_t> g_missing_host_pass10{0u};
std::atomic<std::uint64_t> g_restore_failures{0u};
std::atomic<std::uint64_t> g_present_count{0u};

std::atomic<std::uint64_t> g_exact_role_draws{0u};
std::atomic<std::uint64_t> g_depth_msaa_draws{0u};
std::atomic<std::uint64_t> g_pass01_scope_draws{0u};
std::atomic<std::uint64_t> g_depth_msaa_pass01_overlap{0u};
std::atomic<std::uint32_t> g_depth_msaa_last_thread{0u};
std::atomic<std::uint32_t> g_pass01_role_mask{0u};
std::atomic<std::uint32_t> g_pass0d_role_mask{0u};
std::array<std::atomic<std::uint64_t>,
           static_cast<std::size_t>(role::count)> g_role_draw_counts{};
std::array<std::atomic<std::uint32_t>,
           static_cast<std::size_t>(role::count)> g_role_last_thread{};
constexpr std::size_t k_role_timeline_capacity = 32u;
std::array<std::atomic<std::uint8_t>, k_role_timeline_capacity>
    g_frame_role_timeline{};
std::atomic<std::uint32_t> g_frame_role_timeline_count{0u};
std::atomic_bool g_role_timeline_logged{false};
std::atomic<std::uint32_t> g_role_resource_logged_mask{0u};

thread_local sequence_state g_sequence{};
thread_local bool g_internal_replay = false;

#ifdef DSRRL_DOF_PROFILE
constexpr std::uint32_t k_dof_profile_sample_period = 1024u;

enum class dof_profile_path : std::uint8_t {
    early = 0,
    idle_scan,
    begin_sequence,
    advance_sequence,
    tonemap,
    count
};

std::atomic<std::uint64_t> g_dof_profile_samples{0u};
std::atomic<std::uint64_t> g_dof_profile_ticks{0u};
std::atomic<std::uint64_t> g_dof_profile_max_ticks{0u};
std::array<std::atomic<std::uint64_t>,
           static_cast<std::size_t>(dof_profile_path::count)>
    g_dof_profile_paths{};
thread_local std::uint64_t g_dof_profile_sequence = 0u;

std::uint64_t dof_profile_qpc() noexcept
{
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value)
        ? static_cast<std::uint64_t>(value.QuadPart)
        : 0u;
}

void dof_profile_update_max(
    std::atomic<std::uint64_t> &target,
    std::uint64_t value) noexcept
{
    auto observed =
        target.load(std::memory_order_relaxed);
    while (observed < value &&
           !target.compare_exchange_weak(
               observed,
               value,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

class dof_profile_scope {
public:
    dof_profile_scope() noexcept
    {
        const auto seq =
            ++g_dof_profile_sequence;
        active_ =
            (seq & (k_dof_profile_sample_period - 1u)) == 0u;
        if (active_)
            start_ = dof_profile_qpc();
    }

    ~dof_profile_scope() noexcept
    {
        if (!active_)
            return;
        const auto end =
            dof_profile_qpc();
        if (end < start_)
            return;
        const auto elapsed = end - start_;
        g_dof_profile_ticks.fetch_add(
            elapsed,
            std::memory_order_relaxed);
        dof_profile_update_max(
            g_dof_profile_max_ticks,
            elapsed);
        g_dof_profile_samples.fetch_add(
            1u,
            std::memory_order_relaxed);
        g_dof_profile_paths[
            static_cast<std::size_t>(path_)]
            .fetch_add(1u, std::memory_order_relaxed);
    }

    void set_path(dof_profile_path path) noexcept
    {
        path_ = path;
    }

private:
    bool active_ = false;
    std::uint64_t start_ = 0u;
    dof_profile_path path_ = dof_profile_path::early;
};

void log_dof_profile() noexcept
{
    const auto samples =
        g_dof_profile_samples.load(
            std::memory_order_relaxed);
    if (samples == 0u)
        return;

    LARGE_INTEGER frequency{};
    if (!QueryPerformanceFrequency(&frequency) ||
        frequency.QuadPart <= 0)
        return;

    const auto ticks =
        g_dof_profile_ticks.load(
            std::memory_order_relaxed);
    const auto max_ticks =
        g_dof_profile_max_ticks.load(
            std::memory_order_relaxed);
    const auto avg_us =
        (static_cast<double>(ticks) * 1000000.0) /
        (static_cast<double>(frequency.QuadPart) *
         static_cast<double>(samples));
    const auto max_us =
        (static_cast<double>(max_ticks) * 1000000.0) /
        static_cast<double>(frequency.QuadPart);

    char line[512]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL DoF PERF] DOF sample=1/%u n=%llu total_us=%.3f max_total_us=%.3f paths=early:%llu scan:%llu begin:%llu advance:%llu tonemap:%llu",
        k_dof_profile_sample_period,
        static_cast<unsigned long long>(samples),
        avg_us,
        max_us,
        static_cast<unsigned long long>(
            g_dof_profile_paths[
                static_cast<std::size_t>(
                    dof_profile_path::early)].load()),
        static_cast<unsigned long long>(
            g_dof_profile_paths[
                static_cast<std::size_t>(
                    dof_profile_path::idle_scan)].load()),
        static_cast<unsigned long long>(
            g_dof_profile_paths[
                static_cast<std::size_t>(
                    dof_profile_path::begin_sequence)].load()),
        static_cast<unsigned long long>(
            g_dof_profile_paths[
                static_cast<std::size_t>(
                    dof_profile_path::advance_sequence)].load()),
        static_cast<unsigned long long>(
            g_dof_profile_paths[
                static_cast<std::size_t>(
                    dof_profile_path::tonemap)].load()));
    reshade::log::message(
        reshade::log::level::info,
        line);
}
#endif

bool environment_opt_in() noexcept
{
#ifdef DSRRL_DOF_DEFAULT_ON
    return true;
#else
    char value[8]{};
    const DWORD size =
        GetEnvironmentVariableA(
            "DSRRL_EXPERIMENTAL_PTDE_DOF",
            value,
            static_cast<DWORD>(sizeof(value)));

    return
        size == 1u &&
        value[0] == '1';
#endif
}

void set_authored_feature(bool enabled) noexcept
{
    if (g_core != nullptr)
        (void)g_core->features().set(
            core::operator_id::post_dof_ptde,
            enabled);
}

void release_pass_state(
    pass_state_carriers &state) noexcept
{
    if (state.depth_sampler != nullptr)
        state.depth_sampler->Release();
    if (state.color_sampler != nullptr)
        state.color_sampler->Release();
    state = {};
}

void default_blend_desc(
    D3D11_BLEND_DESC &desc) noexcept
{
    desc = {};
    desc.AlphaToCoverageEnable = FALSE;
    desc.IndependentBlendEnable = FALSE;
    for (auto &rt : desc.RenderTarget) {
        rt.BlendEnable = FALSE;
        rt.SrcBlend = D3D11_BLEND_ONE;
        rt.DestBlend = D3D11_BLEND_ZERO;
        rt.BlendOp = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt.DestBlendAlpha = D3D11_BLEND_ZERO;
        rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
}

bool capture_pass_state(
    ID3D11DeviceContext *context,
    pass_state_carriers &out) noexcept
{
    release_pass_state(out);
    if (context == nullptr)
        return false;

    std::array<ID3D11SamplerState *, 2> samplers{};
    context->PSGetSamplers(
        0u,
        static_cast<UINT>(samplers.size()),
        samplers.data());

    if (samplers[0] == nullptr ||
        samplers[1] == nullptr) {
        for (auto *sampler : samplers)
            if (sampler != nullptr)
                sampler->Release();
        return false;
    }

    // Production prefix is opaque postprocess. Accept only the authentic
    // host cut when it is also opaque/full-write; private write-mask states
    // themselves are cached once per D3D11 device by the scheduler.
    ID3D11BlendState *base_blend = nullptr;
    std::array<FLOAT, 4> blend_factor{};
    UINT sample_mask = 0xffffffffu;
    context->OMGetBlendState(
        &base_blend,
        blend_factor.data(),
        &sample_mask);

    D3D11_BLEND_DESC base_desc{};
    if (base_blend != nullptr)
        base_blend->GetDesc(&base_desc);
    else
        default_blend_desc(base_desc);

    const bool opaque_full_write =
        base_desc.RenderTarget[0].BlendEnable == FALSE &&
        base_desc.RenderTarget[0].RenderTargetWriteMask ==
            D3D11_COLOR_WRITE_ENABLE_ALL;

    if (base_blend != nullptr)
        base_blend->Release();

    if (!opaque_full_write) {
        samplers[0]->Release();
        samplers[1]->Release();
        return false;
    }

    out.color_sampler = samplers[0];
    out.depth_sampler = samplers[1];
    out.ready = true;
    return true;
}

void release_sequence() noexcept
{
    if (g_sequence.terminal != nullptr) {
        g_sequence.terminal->Release();
        g_sequence.terminal = nullptr;
    }

    release_host_dof_inputs(
        g_sequence.host);
    release_pass_state(
        g_sequence.pass_state);

    g_sequence = {};
}

void disable_visible_bridge() noexcept
{
    set_authored_feature(false);
    g_armed.store(
        false,
        std::memory_order_release);
}


struct view_resource_desc {
    void *resource = nullptr;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    std::uint32_t format = 0u;
    std::uint32_t samples = 0u;
};

view_resource_desc describe_view(ID3D11View *view) noexcept
{
    view_resource_desc out{};
    if (view == nullptr)
        return out;

    ID3D11Resource *resource = nullptr;
    view->GetResource(&resource);
    if (resource == nullptr)
        return out;

    out.resource = resource;

    ID3D11Texture2D *texture = nullptr;
    if (SUCCEEDED(resource->QueryInterface(
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void **>(&texture))) &&
        texture != nullptr) {
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        out.width = desc.Width;
        out.height = desc.Height;
        out.format = static_cast<std::uint32_t>(desc.Format);
        out.samples = desc.SampleDesc.Count;
        texture->Release();
    }

    resource->Release();
    return out;
}

void log_role_resources_once(
    reshade::api::command_list *cmd_list,
    role selected) noexcept
{
    if (cmd_list == nullptr ||
        selected == role::count)
        return;

    const auto role_index =
        static_cast<std::uint32_t>(selected);
    if (role_index >= 32u)
        return;

    const auto bit = 1u << role_index;
    const auto previous =
        g_role_resource_logged_mask.fetch_or(
            bit,
            std::memory_order_acq_rel);
    if ((previous & bit) != 0u)
        return;

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (context == nullptr)
        return;

    ID3D11RenderTargetView *rtv = nullptr;
    ID3D11DepthStencilView *dsv = nullptr;
    context->OMGetRenderTargets(1u, &rtv, &dsv);

    std::array<ID3D11ShaderResourceView *, 5> srvs{};
    context->PSGetShaderResources(
        0u,
        static_cast<UINT>(srvs.size()),
        srvs.data());

    const auto rt =
        describe_view(rtv);
    std::array<view_resource_desc, 5> src{};
    for (std::size_t i = 0u; i < srvs.size(); ++i)
        src[i] = describe_view(srvs[i]);

    char line[2048]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL DoF resource] role=r%u tid=%u rt=%p:%ux%u:f%u:s%u t0=%p:%ux%u:f%u:s%u t1=%p:%ux%u:f%u:s%u t2=%p:%ux%u:f%u:s%u t3=%p:%ux%u:f%u:s%u t4=%p:%ux%u:f%u:s%u",
        role_index,
        static_cast<std::uint32_t>(GetCurrentThreadId()),
        rt.resource, rt.width, rt.height, rt.format, rt.samples,
        src[0].resource, src[0].width, src[0].height, src[0].format, src[0].samples,
        src[1].resource, src[1].width, src[1].height, src[1].format, src[1].samples,
        src[2].resource, src[2].width, src[2].height, src[2].format, src[2].samples,
        src[3].resource, src[3].width, src[3].height, src[3].format, src[3].samples,
        src[4].resource, src[4].width, src[4].height, src[4].format, src[4].samples);

    reshade::log::message(
        reshade::log::level::info,
        line);

    for (auto *srv : srvs)
        if (srv != nullptr)
            srv->Release();
    if (dsv != nullptr)
        dsv->Release();
    if (rtv != nullptr)
        rtv->Release();
}

void fail_sequence(
    bool quarantine) noexcept
{
    ++g_sequence_failures;

    if (g_armed.load(
            std::memory_order_acquire))
        disable_visible_bridge();

    release_sequence();

    if (quarantine)
        g_quarantined.store(
            true,
            std::memory_order_release);
}

scheduler_external_inputs
scheduler_inputs() noexcept
{
    scheduler_external_inputs inputs{};
    inputs.scene_source_t0 =
        g_sequence.host.source_68;
    inputs.depth_support_t1 =
        g_sequence.host.dofrate_support_t1;

    inputs.color_sampler =
        g_sequence.pass_state.color_sampler;
    inputs.depth_sampler =
        g_sequence.pass_state.depth_sampler;

    inputs.scene_source_verified =
        g_sequence.host.ready &&
        inputs.scene_source_t0 != nullptr;
    inputs.depth_support_verified =
        g_sequence.host.ready &&
        inputs.depth_support_t1 != nullptr;
    inputs.pass_state_verified =
        g_sequence.pass_state.ready &&
        inputs.color_sampler != nullptr &&
        inputs.depth_sampler != nullptr;
    return inputs;
}

operators::dof::activation_context
activation_context() noexcept
{
    operators::dof::activation_context activation{};

    const auto authored =
        authored_state_status();
    const auto preflight =
        telemetry();
    const auto tone =
        tonemap_handoff_status();

    activation.enabled =
        g_opt_in.load(
            std::memory_order_acquire) &&
        !g_quarantined.load(
            std::memory_order_acquire);

    activation.exact_imageprocess_dof_flat =
        g_sequence.host.ready;
    activation.mode =
        g_sequence.host.mode;
    activation.carrier =
        operators::dof::carrier_mode::native_rate_ptde_seed;

    activation.graph_complete =
        operators::dof::
            production_seed_graph_is_structurally_closed();

    activation.ptde_dofbank_payload_ready =
        authored.hook_ready;
    activation.ptde_dofbank_route_verified =
        authored.hook_ready &&
        authored.route_matches != 0u;

    activation.q8_scene_history_ready = false;
    activation.ptde_seed_adapter_ready =
        g_sequence.host.ready &&
        g_sequence.host.source_68 != nullptr &&
        g_sequence.host.dofrate_support_t1 != nullptr &&
        g_sequence.pass_state.ready;

    activation.pass_state_transaction_ready =
        g_sequence.pass_state.ready;

    activation.retained_flat_pipeline_set_ready =
        preflight.active_flat_set_seen;

    activation.private_depth_sidecar_ready =
        g_sequence.host.dofrate_support_t1 != nullptr;
    activation.retained_plain_dofrate_ready =
        ptde_scheduler_plain_rate_ready();
    activation.fixed_raster_chain_ready =
        private_resources_ready();

    activation.tonemap_dof_continuation_verified =
        tone.hook_ready;
    activation.output_cut_verified =
        tone.hook_ready;

    activation.writes = {};
    return activation;
}

bool is_dof_family_role(role selected) noexcept
{
    switch (selected) {
    case role::depth_copy:
    case role::depth_copy_fragment0:
    case role::depth_copy_fragment1:
    case role::depth_copy_msaa:
    case role::depth_copy_single_fragment:
    case role::dof_composite:
    case role::blur_upsample:
    case role::dof_composite_cb:
    case role::dof_rate_plain:
    case role::dof_rate_cb:
    case role::downsample:
    case role::gauss_x:
    case role::gauss_x_adv:
    case role::gauss_y:
    case role::gauss_y_adv:
    case role::near_rate:
    case role::unfocus_3x3:
    case role::unfocus_near_rate_3x3:
        return true;
    default:
        return false;
    }
}

bool issue_native_draw(
    ID3D11DeviceContext *context,
    const scheduler_draw_shape &shape) noexcept
{
    if (context == nullptr ||
        shape.instance_count == 0u)
        return false;

    if (shape.indexed) {
        if (shape.index_count == 0u)
            return false;

        if (shape.instance_count == 1u &&
            shape.first_instance == 0u)
            context->DrawIndexed(
                shape.index_count,
                shape.first_index,
                shape.vertex_offset);
        else
            context->DrawIndexedInstanced(
                shape.index_count,
                shape.instance_count,
                shape.first_index,
                shape.vertex_offset,
                shape.first_instance);

        return true;
    }

    if (shape.vertex_count == 0u)
        return false;

    if (shape.instance_count == 1u &&
        shape.first_instance == 0u)
        context->Draw(
            shape.vertex_count,
            shape.first_vertex);
    else
        context->DrawInstanced(
            shape.vertex_count,
            shape.instance_count,
            shape.first_vertex,
            shape.first_instance);

    return true;
}

bool acquire_terminal(
    const operators::dof::activation_context &activation) noexcept
{
    if (!authorize_private_resources(
            activation))
        return false;

    ID3D11ShaderResourceView *terminal = nullptr;
    const bool ok =
        acquire_private_shader_resource(
            operators::dof::ptde_surface_role::full_terminal,
            &terminal) &&
        terminal != nullptr;

    revoke_private_resources();

    if (!ok) {
        if (terminal != nullptr)
            terminal->Release();
        return false;
    }

    if (g_sequence.terminal != nullptr)
        g_sequence.terminal->Release();

    g_sequence.terminal = terminal;
    return true;
}

bool begin_sequence(
    reshade::api::command_list *cmd_list,
    const scheduler_draw_shape &shape) noexcept
{
    // Runtime evidence proved that the exact host pass01 scope and retained
    // DSR DoF role draws are disjoint. Treat pass01 only as the authoritative
    // source/depth semantic cut and run the PTDE-private prefix independently.
    if (cmd_list == nullptr ||
        !inside_exact_dof_pass01())
        return false;

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (context == nullptr)
        return false;

    host_dof_inputs captured{};
    if (!capture_host_dof_inputs(
            context,
            captured))
        return false;

    pass_state_carriers captured_state{};
    if (!capture_pass_state(
            context,
            captured_state)) {
        release_host_dof_inputs(captured);
        return false;
    }

    release_sequence();
    g_sequence.host = captured;
    captured = {};
    g_sequence.pass_state = captured_state;
    captured_state = {};
    g_sequence.active = true;
    g_sequence.next_pass = 0u;
    ++g_first_pass_hits;
    ++g_sequences_started;

    const auto activation =
        activation_context();

    // Synthetic scene seed: one retained SAMPLE into fixed 1024x720
    // BGRA8 writes RGB only. No DofBank/pass00 state is required.
    if (execute_ptde_pass(
            cmd_list,
            activation,
            scheduler_inputs(),
            shape,
            0u) != result::executed) {
        fail_sequence(false);
        return false;
    }

    // PTDE pass00 DofRate executes at the same 1024x720 raster and writes
    // alpha only into the seeded RGB target.
    if (!push_host_pass00_state()) {
        fail_sequence(true);
        return false;
    }

    const auto pass00 =
        execute_ptde_pass(
            cmd_list,
            activation,
            scheduler_inputs(),
            shape,
            1u);

    const bool state_restored =
        pop_host_pass00_state();

    if (!state_restored ||
        pass00 != result::executed) {
        fail_sequence(!state_restored);
        return false;
    }

    // Pass2 is the first 1024x720 -> 512x360 reduction. Passes3..8 preserve
    // the PTDE half/quarter blur-rate topology. Terminal pass9 is deferred to
    // the authentic retained DSR FRPG_Fil_Dof scope.
    for (std::size_t pass_index = 2u;
         pass_index < 9u;
         ++pass_index) {
        g_sequence.next_pass = pass_index;
        const auto executed =
            execute_ptde_pass(
                cmd_list,
                activation,
                scheduler_inputs(),
                shape,
                pass_index);
        if (executed != result::executed) {
            fail_sequence(false);
            return false;
        }
    }

    g_sequence.next_pass = 9u;
    return true;
}

void advance_sequence(
    reshade::api::command_list *cmd_list,
    const scheduler_draw_shape &shape,
    role selected) noexcept
{
    if (!g_sequence.active ||
        g_sequence.complete ||
        g_sequence.next_pass != 9u)
        return;

    // Execute the private terminal only inside the authentic retained DSR
    // FRPG_Fil_Dof draw. This preserves the host terminal CB/sampler/t5/OM
    // state as the carrier while private PTDE t0..t4 + target are substituted.
    if (selected != role::dof_composite)
        return;

    const auto activation =
        activation_context();

    if (execute_ptde_pass(
            cmd_list,
            activation,
            scheduler_inputs(),
            shape,
            9u) != result::executed) {
        fail_sequence(false);
        return;
    }

    g_sequence.next_pass =
        operators::dof::production_seed_passes.size();

    if (!acquire_terminal(
            activation)) {
        fail_sequence(false);
        return;
    }

    g_sequence.complete = true;
    ++g_sequences_completed;
}

bool visible_tonemap_handoff(
    reshade::api::command_list *cmd_list,
    const scheduler_draw_shape &shape) noexcept
{
    if (cmd_list == nullptr ||
        g_sequence.terminal == nullptr)
        return false;

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (context == nullptr)
        return false;

    ID3D11ShaderResourceView *old_t0 = nullptr;
    context->PSGetShaderResources(
        0u,
        1u,
        &old_t0);

    ID3D11ShaderResourceView *terminal =
        g_sequence.terminal;
    context->PSSetShaderResources(
        0u,
        1u,
        &terminal);

    g_internal_replay = true;
    const bool drawn =
        issue_native_draw(
            context,
            shape);
    g_internal_replay = false;

    context->PSSetShaderResources(
        0u,
        1u,
        &old_t0);

    ID3D11ShaderResourceView *verify = nullptr;
    context->PSGetShaderResources(
        0u,
        1u,
        &verify);

    const bool restored =
        verify == old_t0;

    if (verify != nullptr)
        verify->Release();
    if (old_t0 != nullptr)
        old_t0->Release();

    if (!restored) {
        ++g_restore_failures;
        disable_visible_bridge();
        g_quarantined.store(
            true,
            std::memory_order_release);
    }

    return drawn;
}

bool handle_tonemap(
    reshade::api::command_list *cmd_list,
    const scheduler_draw_shape &shape) noexcept
{
    if (!inside_exact_tonemap_dof_handoff())
        return false;

    if (!g_sequence.active ||
        !g_sequence.complete ||
        g_sequence.terminal == nullptr) {
        if (g_sequence.active &&
            g_sequence.next_pass == 9u)
            ++g_missing_host_pass10;

        ++g_tonemap_fallbacks;

        if (g_sequence.active)
            fail_sequence(false);

        return false;
    }

    if (!g_armed.load(
            std::memory_order_acquire)) {
        if (g_core == nullptr ||
            !g_core->features().set(
                core::operator_id::post_dof_ptde,
                true)) {
            fail_sequence(true);
            return false;
        }

        g_armed.store(
            true,
            std::memory_order_release);
        ++g_dry_run_pass;
        release_sequence();

        reshade::log::message(
            reshade::log::level::info,
            "DSRRL DoF: full private PTDE graph dry-run PASS; PTDE authored state armed for subsequent frames.");
        return false;
    }

    const bool replayed =
        visible_tonemap_handoff(
            cmd_list,
            shape);

    if (!replayed) {
        ++g_tonemap_fallbacks;
        fail_sequence(false);
        return false;
    }

    ++g_visible_handoffs;
    release_sequence();

    // The native ToneMap draw above used private PTDE +0x84 as t0.
    // Suppress only this exact original draw; all downstream stock DSR
    // processing remains untouched.
    return true;
}

bool handle_draw(
    reshade::api::command_list *cmd_list,
    const scheduler_draw_shape &shape) noexcept
{
#ifdef DSRRL_DOF_PROFILE
    dof_profile_scope profile_scope;
#endif

    if (g_internal_replay ||
        !g_opt_in.load(
            std::memory_order_acquire) ||
        g_quarantined.load(
            std::memory_order_acquire) ||
        cmd_list == nullptr)
        return false;

    // Idle hot path: do not query retained-role state, touch role telemetry,
    // or inspect ToneMap unless the exact host pass01 scope is live. This
    // keeps enabled-but-inactive DoF essentially free on ordinary draws.
    if (!g_sequence.active) {
        const bool pass01_scope =
            inside_exact_dof_pass01();
        if (!pass01_scope)
            return false;

        g_pass01_scope_draws.fetch_add(
            1u,
            std::memory_order_relaxed);

#ifdef DSRRL_DOF_PROFILE
        profile_scope.set_path(
            dof_profile_path::begin_sequence);
#endif
        (void)begin_sequence(
            cmd_list,
            shape);
        return false;
    }

    if (inside_exact_tonemap_dof_handoff()) {
#ifdef DSRRL_DOF_PROFILE
        profile_scope.set_path(
            dof_profile_path::tonemap);
#endif
        return handle_tonemap(
            cmd_list,
            shape);
    }

    role selected = role::count;
    const bool exact_role =
        bound_retained_role(
            cmd_list,
            selected);
    if (!exact_role)
        return false;

    g_exact_role_draws.fetch_add(
        1u,
        std::memory_order_relaxed);
    const auto role_index =
        static_cast<std::size_t>(selected);
    if (role_index < g_role_draw_counts.size()) {
        g_role_draw_counts[role_index].fetch_add(
            1u,
            std::memory_order_relaxed);
        g_role_last_thread[role_index].store(
            static_cast<std::uint32_t>(
                GetCurrentThreadId()),
            std::memory_order_relaxed);
    }

    if (selected == role::depth_copy_msaa) {
        g_depth_msaa_draws.fetch_add(
            1u,
            std::memory_order_relaxed);
        g_depth_msaa_last_thread.store(
            static_cast<std::uint32_t>(
                GetCurrentThreadId()),
            std::memory_order_relaxed);
    }

#ifdef DSRRL_DOF_PROFILE
    profile_scope.set_path(
        dof_profile_path::advance_sequence);
#endif
    advance_sequence(
        cmd_list,
        shape,
        selected);

    return false;
}

void log_status(
    const char *tag) noexcept
{
    const auto status =
        ptde_draw_bridge_status();

    const auto host =
        host_depth_route_status();

    char line[1024]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL DoF bridge] %s optin=%u armed=%u quarantine=%u first=%llu start=%llu complete=%llu fail=%llu dry=%llu visible=%llu fallback=%llu no_pass10=%llu restore_fail=%llu exact_draw=%llu msaa_draw=%llu pass01_scope_draw=%llu msaa_pass01_overlap=%llu msaa_tid=%u host01_calls=%llu host0d_calls=%llu host01_tid=%u host0d_tid=%u host_first=%llu src=%llu/%llu support=%llu/%llu abi_reject=%llu p01_role_mask=0x%x p0d_role_mask=0x%x hook=%u/%u",
        tag,
        status.opt_in ? 1u : 0u,
        status.armed ? 1u : 0u,
        status.quarantined ? 1u : 0u,
        static_cast<unsigned long long>(
            status.first_pass_hits),
        static_cast<unsigned long long>(
            status.sequences_started),
        static_cast<unsigned long long>(
            status.sequences_completed),
        static_cast<unsigned long long>(
            status.sequence_failures),
        static_cast<unsigned long long>(
            status.dry_run_pass),
        static_cast<unsigned long long>(
            status.visible_handoffs),
        static_cast<unsigned long long>(
            status.tonemap_fallbacks),
        static_cast<unsigned long long>(
            status.missing_host_pass10),
        static_cast<unsigned long long>(
            status.restore_failures),
        static_cast<unsigned long long>(
            g_exact_role_draws.load()),
        static_cast<unsigned long long>(
            g_depth_msaa_draws.load()),
        static_cast<unsigned long long>(
            g_pass01_scope_draws.load()),
        static_cast<unsigned long long>(
            g_depth_msaa_pass01_overlap.load()),
        g_depth_msaa_last_thread.load(),
        static_cast<unsigned long long>(
            host.pass01_calls),
        static_cast<unsigned long long>(
            host.pass0d_calls),
        host.pass01_last_thread,
        host.pass0d_last_thread,
        static_cast<unsigned long long>(
            host.first_pass01_hits),
        static_cast<unsigned long long>(
            host.source_capture_ok),
        static_cast<unsigned long long>(
            host.source_capture_fail),
        static_cast<unsigned long long>(
            host.support_capture_ok),
        static_cast<unsigned long long>(
            host.support_capture_fail),
        static_cast<unsigned long long>(
            host.abi_reject),
        g_pass01_role_mask.load(std::memory_order_relaxed),
        g_pass0d_role_mask.load(std::memory_order_relaxed),
        host.pass01_hook_ready ? 1u : 0u,
        host.pass0d_hook_ready ? 1u : 0u);

    reshade::log::message(
        reshade::log::level::info,
        line);

    char roles_line[1024]{};
    int used = std::snprintf(
        roles_line,
        sizeof(roles_line),
        "[DSRRL DoF roles] %s",
        tag);
    for (std::size_t i = 0u;
         i < g_role_draw_counts.size() &&
         used > 0 &&
         static_cast<std::size_t>(used) < sizeof(roles_line);
         ++i) {
        const auto count =
            g_role_draw_counts[i].load(
                std::memory_order_relaxed);
        if (count == 0u)
            continue;
        const int wrote = std::snprintf(
            roles_line + used,
            sizeof(roles_line) -
                static_cast<std::size_t>(used),
            " r%zu=%llu@t%u",
            i,
            static_cast<unsigned long long>(count),
            g_role_last_thread[i].load(
                std::memory_order_relaxed));
        if (wrote <= 0)
            break;
        used += wrote;
    }
    reshade::log::message(
        reshade::log::level::info,
        roles_line);
}

void on_present(
    reshade::api::command_queue *,
    reshade::api::swapchain *,
    const reshade::api::rect *,
    const reshade::api::rect *,
    std::uint32_t,
    const reshade::api::rect *)
{
    if (g_sequence.active) {
        ++g_sequence_failures;
        if (g_armed.load(
                std::memory_order_acquire))
            disable_visible_bridge();
        release_sequence();
    }

    const auto present =
        ++g_present_count;

    if (!g_role_timeline_logged.load(
            std::memory_order_acquire)) {
        const auto count =
            g_frame_role_timeline_count.exchange(
                0u,
                std::memory_order_acq_rel);
        if (count != 0u) {
            char timeline[512]{};
            int used = std::snprintf(
                timeline,
                sizeof(timeline),
                "[DSRRL DoF timeline] first_nonempty_present roles=");
            const auto limit =
                std::min<std::uint32_t>(
                    count,
                    static_cast<std::uint32_t>(
                        g_frame_role_timeline.size()));
            for (std::uint32_t i = 0u;
                 i < limit &&
                 used > 0 &&
                 static_cast<std::size_t>(used) <
                     sizeof(timeline);
                 ++i) {
                const auto role_id =
                    g_frame_role_timeline[i].load(
                        std::memory_order_relaxed);
                const int wrote = std::snprintf(
                    timeline + used,
                    sizeof(timeline) -
                        static_cast<std::size_t>(used),
                    "%sr%u",
                    i == 0u ? "" : ">",
                    static_cast<unsigned>(role_id));
                if (wrote <= 0)
                    break;
                used += wrote;
            }
            reshade::log::message(
                reshade::log::level::info,
                timeline);
            g_role_timeline_logged.store(
                true,
                std::memory_order_release);
        }
    }

    if (g_opt_in.load(
            std::memory_order_acquire) &&
        (present == 1u ||
         (present % 300u) == 0u)) {
        log_status("LIVE");
#ifdef DSRRL_DOF_PROFILE
        log_dof_profile();
#endif
    }
}

} // namespace

bool handle_draw_event(
    reshade::api::command_list *cmd_list,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept
{
    scheduler_draw_shape shape{};
    shape.indexed = false;
    shape.vertex_count = vertex_count;
    shape.instance_count = instance_count;
    shape.first_vertex = first_vertex;
    shape.first_instance = first_instance;
    return handle_draw(cmd_list, shape);
}

bool handle_draw_indexed_event(
    reshade::api::command_list *cmd_list,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept
{
    scheduler_draw_shape shape{};
    shape.indexed = true;
    shape.index_count = index_count;
    shape.instance_count = instance_count;
    shape.first_index = first_index;
    shape.vertex_offset = vertex_offset;
    shape.first_instance = first_instance;
    return handle_draw(cmd_list, shape);
}

bool register_ptde_draw_bridge_runtime(
    core::renderer_core &core) noexcept
{
    if (g_registered.load(
            std::memory_order_acquire))
        return true;

    g_core = &core;
    g_opt_in.store(
        environment_opt_in(),
        std::memory_order_release);
    g_armed.store(
        false,
        std::memory_order_release);
    g_quarantined.store(
        false,
        std::memory_order_release);
    g_present_count.store(0u);
    g_exact_role_draws.store(0u);
    g_depth_msaa_draws.store(0u);
    g_pass01_scope_draws.store(0u);
    g_depth_msaa_pass01_overlap.store(0u);
    g_depth_msaa_last_thread.store(0u);
    g_pass01_role_mask.store(0u);
    g_pass0d_role_mask.store(0u);
    for (auto &v : g_role_draw_counts)
        v.store(0u, std::memory_order_relaxed);
    for (auto &v : g_role_last_thread)
        v.store(0u, std::memory_order_relaxed);
    for (auto &v : g_frame_role_timeline)
        v.store(0u, std::memory_order_relaxed);
    g_frame_role_timeline_count.store(0u);
    g_role_timeline_logged.store(false);
    g_role_resource_logged_mask.store(0u);

    set_authored_feature(false);
    release_sequence();

    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL DoF PTDE1024] carrier=NATIVE_RATE_PTDE_SEED seed=1024x720 seed_history=SNAPSHOT_ONLY private_prefix=1024_512_256 stock_branch=PASS10_ONLY tonemap=PTDE_TERMINAL_HANDOFF");

    if (!g_opt_in.load(
            std::memory_order_acquire)) {
        reshade::log::message(
            reshade::log::level::info,
            "DSRRL DoF: experimental PTDE bridge OFF. Set DSRRL_EXPERIMENTAL_PTDE_DOF=1 before launch to run the guarded private island.");
    }

    try {
        reshade::register_event<
            reshade::addon_event::present>(
                on_present);

        g_registered.store(
            true,
            std::memory_order_release);
        return true;
    } catch (...) {
        g_core = nullptr;
        g_opt_in.store(false);
        return false;
    }
}

void unregister_ptde_draw_bridge_runtime() noexcept
{
    if (g_registered.exchange(
            false,
            std::memory_order_acq_rel)) {
        reshade::unregister_event<
            reshade::addon_event::present>(
                on_present);
    }

    disable_visible_bridge();
    release_sequence();

    if (g_opt_in.load(
            std::memory_order_acquire))
        log_status("UNLOAD");

    g_core = nullptr;
    g_opt_in.store(false);
}

draw_bridge_telemetry
ptde_draw_bridge_status() noexcept
{
    return {
        g_first_pass_hits.load(),
        g_sequences_started.load(),
        g_sequences_completed.load(),
        g_sequence_failures.load(),
        g_dry_run_pass.load(),
        g_visible_handoffs.load(),
        g_tonemap_fallbacks.load(),
        g_missing_host_pass10.load(),
        g_restore_failures.load(),
        g_opt_in.load(
            std::memory_order_acquire),
        g_armed.load(
            std::memory_order_acquire),
        g_quarantined.load(
            std::memory_order_acquire)
    };
}

} // namespace dsrrl::runtime::dof
