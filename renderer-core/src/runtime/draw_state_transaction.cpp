#include "dsrrl/runtime/draw_state_transaction.hpp"
#include "dsrrl/runtime/d3d11_cb_window.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/core/draw_transaction_policy.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <d3d11_1.h>

#include <array>
#include <cstddef>
#include <cstdio>

namespace dsrrl::runtime {
namespace {

core::context_kind context_kind_of(ID3D11DeviceContext *ctx) noexcept
{
    if (ctx == nullptr)
        return core::context_kind::unknown;

    return ctx->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED
        ? core::context_kind::deferred
        : core::context_kind::immediate;
}

std::uint64_t command_key(reshade::api::command_list *cmd_list) noexcept
{
    return static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(cmd_list));
}

struct context1_tls_entry {
    const draw_state_transaction_runtime *owner = nullptr;
    ID3D11DeviceContext *context = nullptr;
    ID3D11DeviceContext1 *context1 = nullptr;
    std::uint64_t epoch = 0u;
};

constexpr std::size_t k_context1_tls_cache_size = 8u;
static_assert(
    (k_context1_tls_cache_size &
     (k_context1_tls_cache_size - 1u)) == 0u);

thread_local std::array<
    context1_tls_entry,
    k_context1_tls_cache_size> g_context1_tls_cache{};

struct local_tx_state {
    const draw_state_transaction_runtime *owner = nullptr;
    std::uint64_t command = 0u;
    bool active = false;
};

thread_local local_tx_state g_local_tx{};

#ifdef DSRRL_POINTLIGHT_PROFILE
constexpr std::uint32_t k_pointlight_tx_profile_sample_period = 128u;
static_assert(
    (k_pointlight_tx_profile_sample_period &
     (k_pointlight_tx_profile_sample_period - 1u)) == 0u);

thread_local std::uint32_t g_pointlight_tx_profile_counter = 0u;

std::uint64_t pointlight_profile_qpc() noexcept
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return static_cast<std::uint64_t>(value.QuadPart);
}

std::uint64_t pointlight_profile_qpc_frequency() noexcept
{
    static const std::uint64_t frequency = []() noexcept {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return static_cast<std::uint64_t>(value.QuadPart);
    }();
    return frequency;
}

void pointlight_profile_max(
    std::atomic<std::uint64_t> &target,
    std::uint64_t value) noexcept
{
    auto observed = target.load(std::memory_order_relaxed);
    while (observed < value &&
           !target.compare_exchange_weak(
               observed,
               value,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}
#endif

bool restore_local_tx(
    const draw_state_transaction_runtime *owner,
    std::uint64_t command) noexcept
{
    if (!g_local_tx.active ||
        g_local_tx.owner != owner ||
        g_local_tx.command != command)
        return false;

    g_local_tx = {};
    return true;
}

bool synchronous_clustered_transaction_shape(
    const draw_tx_mutation &mutation) noexcept
{
    if (!mutation.synchronous_core_transaction ||
        !mutation.replace_pixel_shader ||
        mutation.pixel_shader == nullptr ||
        mutation.constant_buffer_count != 1u ||
        mutation.constant_buffers[0].slot != 12u ||
        mutation.srv_count != 2u ||
        mutation.srvs[0].slot != 18u ||
        mutation.srvs[1].slot != 19u ||
        mutation.sampler_count != 0u)
        return false;

    const auto point =
        core::operator_bit(
            core::operator_id::point_light);
    const auto attenuation =
        core::operator_bit(
            core::operator_id::pointlight_pnts_attenuation);
    const auto material =
        core::operator_bit(
            core::operator_id::material_response);
    const auto local =
        core::operator_bit(
            core::operator_id::local_specular_legacy);
    const auto diffuse_domain =
        core::operator_bit(
            core::operator_id::diffuse_material_domain);
    const auto sat =
        core::operator_bit(
            core::operator_id::terminal_sat_rgb);
    const auto env_delete =
        core::operator_bit(
            core::operator_id::envspec_nospc_delete);

    const auto allowed =
        point |
        attenuation |
        material |
        local |
        diffuse_domain |
        sat |
        env_delete;

    if ((mutation.owners & point) == 0u ||
        (mutation.owners & attenuation) == 0u ||
        (mutation.owners & material) == 0u ||
        (mutation.owners & diffuse_domain) == 0u ||
        (mutation.owners & sat) == 0u ||
        (mutation.owners & ~allowed) != 0u)
        return false;

    const bool spc =
        (mutation.owners & local) != 0u;
    const bool nospc_delete =
        (mutation.owners & env_delete) != 0u;

    // Certified families are mutually exclusive:
    // Spc carries legacy local specular; NoSpc carries EnvSpec deletion.
    return spc != nospc_delete;
}

bool synchronous_clustered_srv_pair(
    const draw_tx_mutation &mutation) noexcept
{
    return synchronous_clustered_transaction_shape(
        mutation);
}

std::size_t context1_tls_index(
    ID3D11DeviceContext *context) noexcept
{
    const auto value =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(context));
    return static_cast<std::size_t>(
        ((value >> 4u) ^ (value >> 12u)) &
        (k_context1_tls_cache_size - 1u));
}

template <typename T, std::size_t N>
bool unique_slots(
    const std::array<T, N> &bindings,
    std::uint32_t count) noexcept
{
    for (std::uint32_t i = 0; i < count; ++i)
        for (std::uint32_t j = i + 1u; j < count; ++j)
            if (bindings[i].slot == bindings[j].slot)
                return false;

    return true;
}

bool verify_native_readback_for(
    const draw_tx_mutation &mutation) noexcept
{
    core::upper_lower_readback_skip_shape shape{};
    shape.owners = mutation.owners;
    shape.shader_owners = mutation.shader_owners;
    shape.constant_buffer_owners =
        mutation.constant_buffer_owners;
    shape.carrier_owners =
        mutation.carrier_owners;
    shape.resource_owners =
        mutation.resource_owners;
    shape.constant_buffer_count =
        mutation.constant_buffer_count;
    shape.srv_count = mutation.srv_count;
    shape.sampler_count = mutation.sampler_count;

    if (mutation.constant_buffer_count == 1u) {
        shape.constant_buffer_slot =
            mutation.constant_buffers[0].slot;
        shape.constant_buffer_binding_owners =
            mutation.constant_buffers[0].owners;
    }

    return !core::upper_lower_native_readback_skip_allowed(
        shape);
}

} // namespace

draw_state_transaction_runtime::draw_state_transaction_runtime(
    core::renderer_core &core) noexcept
    : core_(core)
{
}

ID3D11DeviceContext1 *
draw_state_transaction_runtime::context1_for(
    ID3D11DeviceContext *context) noexcept
{
    if (context == nullptr)
        return nullptr;

    const auto epoch =
        context1_cache_epoch_.load(
            std::memory_order_acquire);
    auto &tls =
        g_context1_tls_cache[
            context1_tls_index(context)];

    if (tls.owner == this &&
        tls.context == context &&
        tls.epoch == epoch)
        return tls.context1;

    std::lock_guard<std::mutex> lock(
        context1_cache_mutex_);

    const auto found =
        context1_cache_.find(context);
    if (found != context1_cache_.end()) {
        tls = {
            this,
            context,
            found->second.context1,
            context1_cache_epoch_.load(
                std::memory_order_relaxed)
        };
        return found->second.context1;
    }

    ID3D11DeviceContext1 *context1 = nullptr;
    (void)context->QueryInterface(
        __uuidof(ID3D11DeviceContext1),
        reinterpret_cast<void **>(&context1));

    ID3D11Device *device = nullptr;
    context->GetDevice(&device);

    try {
        const auto inserted =
            context1_cache_.emplace(
                context,
                context1_cache_record{
                    device,
                    context1});
        if (!inserted.second) {
            if (context1 != nullptr)
                context1->Release();
            if (device != nullptr)
                device->Release();
            context1 =
                inserted.first->second.context1;
        }
    } catch (...) {
        if (context1 != nullptr)
            context1->Release();
        if (device != nullptr)
            device->Release();
        context1 = nullptr;
    }

    tls = {
        this,
        context,
        context1,
        context1_cache_epoch_.load(
            std::memory_order_relaxed)
    };
    return context1;
}

void draw_state_transaction_runtime::
release_context1_cache() noexcept
{
    context1_cache_epoch_.fetch_add(
        1u,
        std::memory_order_acq_rel);

    std::lock_guard<std::mutex> lock(
        context1_cache_mutex_);
    for (auto &entry : context1_cache_) {
        if (entry.second.context1 != nullptr)
            entry.second.context1->Release();
        if (entry.second.device != nullptr)
            entry.second.device->Release();
    }
    context1_cache_.clear();
}

bool draw_state_transaction_runtime::validate_mutation(
    const draw_tx_mutation &mutation) const noexcept
{
    if (mutation.synchronous_core_transaction &&
        !synchronous_clustered_transaction_shape(
            mutation))
        return false;

    if (mutation.owners == 0u ||
        (mutation.owners & ~core::all_operator_bits) != 0u ||
        (mutation.shader_owners & ~mutation.owners) != 0u ||
        (mutation.constant_buffer_owners & ~mutation.owners) != 0u ||
        (mutation.resource_owners & ~mutation.owners) != 0u ||
        (mutation.carrier_owners & ~mutation.owners) != 0u)
        return false;

    if (mutation.replace_pixel_shader) {
        if (mutation.pixel_shader == nullptr ||
            mutation.shader_owners == 0u)
            return false;
    } else if (mutation.shader_owners != 0u) {
        return false;
    }

    const bool has_constant_buffers =
        mutation.constant_buffer_count != 0u;

    core::operator_mask binding_cb_owners = 0u;

    for (std::uint32_t i = 0u;
         i < mutation.constant_buffer_count;
         ++i) {
        const auto owners =
            mutation.constant_buffers[i].owners;

        if (owners == 0u ||
            (owners & ~mutation.owners) != 0u)
            return false;

        binding_cb_owners |= owners;
    }

    if (has_constant_buffers !=
            (mutation.constant_buffer_owners != 0u) ||
        binding_cb_owners !=
            mutation.constant_buffer_owners)
        return false;

    const bool has_resources =
        mutation.srv_count != 0u ||
        mutation.sampler_count != 0u;

    if (has_resources !=
        (mutation.resource_owners != 0u))
        return false;

    if (mutation.carrier_owners != 0u &&
        !has_constant_buffers)
        return false;

    for (std::size_t i = 0;
         i < core::operator_count;
         ++i) {
        const auto op =
            static_cast<core::operator_id>(i);
        const auto bit =
            core::operator_bit(op);

        if ((mutation.owners & bit) == 0u)
            continue;

        const auto &policy =
            core::draw_policy(op);

        if ((mutation.shader_owners & bit) != 0u &&
            (policy.allowed_mutation_mask &
             core::draw_mutation_shader) == 0u)
            return false;

        if ((mutation.constant_buffer_owners & bit) != 0u &&
            (policy.allowed_mutation_mask &
             core::draw_mutation_constant_buffer) == 0u)
            return false;

        if ((mutation.resource_owners & bit) != 0u &&
            (policy.allowed_mutation_mask &
             (core::draw_mutation_srv |
              core::draw_mutation_sampler)) == 0u)
            return false;

        if ((mutation.carrier_owners & bit) != 0u &&
            ((policy.allowed_mutation_mask &
              core::draw_mutation_constant_buffer) == 0u ||
             core::draw_policy_carrier_write_mask(op) == 0u))
            return false;
    }

    if (mutation.constant_buffer_count > draw_tx_max_cb ||
        mutation.srv_count > draw_tx_max_srv ||
        mutation.sampler_count > draw_tx_max_sampler)
        return false;

    if (!unique_slots(
            mutation.constant_buffers,
            mutation.constant_buffer_count) ||
        !unique_slots(mutation.srvs, mutation.srv_count) ||
        !unique_slots(mutation.samplers, mutation.sampler_count))
        return false;

    for (std::uint32_t i = 0;
         i < mutation.constant_buffer_count;
         ++i) {
        if (mutation.constant_buffers[i].slot >=
                D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT ||
            mutation.constant_buffers[i].buffer == nullptr)
            return false;
    }

    for (std::uint32_t i = 0;
         i < mutation.srv_count;
         ++i) {
        if (mutation.srvs[i].slot >=
            D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT)
            return false;
    }

    for (std::uint32_t i = 0;
         i < mutation.sampler_count;
         ++i) {
        if (mutation.samplers[i].slot >=
            D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT)
            return false;
    }

    return mutation.replace_pixel_shader ||
           mutation.constant_buffer_count != 0u ||
           mutation.srv_count != 0u ||
           mutation.sampler_count != 0u;
}

void draw_state_transaction_runtime::release_state(
    transaction_state &state) noexcept
{
    if (state.old_shader != nullptr)
        state.old_shader->Release();

    for (std::uint32_t i = 0;
         i < state.old_class_count;
         ++i) {
        auto *instance =
            reinterpret_cast<ID3D11ClassInstance *>(
                state.old_classes[i]);
        if (instance != nullptr)
            instance->Release();
    }

    for (std::uint32_t i = 0; i < state.cb_count; ++i) {
        if (state.cbs[i].base != nullptr)
            state.cbs[i].base->Release();
        if (state.cbs[i].window != nullptr)
            state.cbs[i].window->Release();
    }

    for (std::uint32_t i = 0; i < state.srv_count; ++i)
        if (state.srvs[i].srv != nullptr)
            state.srvs[i].srv->Release();

    for (std::uint32_t i = 0;
         i < state.sampler_count;
         ++i)
        if (state.samplers[i].sampler != nullptr)
            state.samplers[i].sampler->Release();

    // context1 is borrowed from the runtime cache. The cache owns the COM
    // reference and releases it at device teardown/reset, so a replay never
    // pays AddRef/Release for the same recording context.
    state.context1 = nullptr;

    state = {};
}

// D3D11 PSGetShader AddRefs each returned class instance. On a
// failed/oversized capture the local array is not transferred to state,
// so release every acquired entry before taking the fail-open exit.
void release_unadopted_ps_classes(
    std::array<ID3D11ClassInstance *, draw_tx_max_class_instances> &classes) noexcept
{
    for (auto *&instance : classes) {
        if (instance != nullptr) {
            instance->Release();
            instance = nullptr;
        }
    }
}

bool draw_state_transaction_runtime::begin(
    reshade::api::command_list *cmd_list,
    const draw_tx_mutation &mutation,
    transaction_state &state) noexcept
{
    state = {};

    if (cmd_list == nullptr ||
        quarantined_.load() ||
        !validate_mutation(mutation)) {
        telemetry::hot_count(begin_fail_);
        return false;
    }

    auto *ctx =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    state.context = ctx;
    if (ctx == nullptr) {
        telemetry::hot_count(begin_fail_);
        return false;
    }

    ID3D11DeviceContext1 *ctx1 = nullptr;
    if (mutation.constant_buffer_count != 0u) {
        ctx1 = context1_for(ctx);
        state.context1 = ctx1;
    }

    state.verify_native_readback =
        telemetry::native_state_verification_enabled() &&
        verify_native_readback_for(mutation);

    if (mutation.replace_pixel_shader) {
        std::array<ID3D11ClassInstance *,
                   draw_tx_max_class_instances> classes{};
        UINT class_count =
            static_cast<UINT>(classes.size());

        ctx->PSGetShader(
            &state.old_shader,
            classes.data(),
            &class_count);

        if (state.old_shader == nullptr ||
            class_count > classes.size()) {
            release_unadopted_ps_classes(classes);
            release_state(state);
            telemetry::hot_count(begin_fail_);
            return false;
        }

        state.old_class_count = class_count;
        for (std::uint32_t i = 0;
             i < state.old_class_count;
             ++i)
            state.old_classes[i] = classes[i];

        state.shader_captured = true;
    }

    state.cb_count = mutation.constant_buffer_count;
    for (std::uint32_t i = 0;
         i < mutation.constant_buffer_count;
         ++i) {
        auto &capture = state.cbs[i];
        capture.slot = mutation.constant_buffers[i].slot;

        ctx->PSGetConstantBuffers(
            capture.slot,
            1u,
            &capture.base);

        if (ctx1 != nullptr) {
            UINT first = 0u;
            UINT count = 0u;
            ctx1->PSGetConstantBuffers1(
                capture.slot,
                1u,
                &capture.window,
                &first,
                &count);

            capture.first = first;
            capture.count = count;
            capture.coherent =
                capture.base == capture.window;
            capture.explicit_window =
                capture.window != nullptr &&
                count >= 16u &&
                (first % 16u) == 0u &&
                (count % 16u) == 0u;
        }

        if (!capture.coherent) {
            release_state(state);
            telemetry::hot_count(begin_fail_);
            return false;
        }
    }

    state.srv_count = mutation.srv_count;
    const bool batch_clustered_srvs =
        synchronous_clustered_srv_pair(
            mutation);

    if (batch_clustered_srvs) {
        state.srvs[0].slot = 18u;
        state.srvs[1].slot = 19u;

        ID3D11ShaderResourceView *captured[2]{};
        ctx->PSGetShaderResources(
            18u,
            2u,
            captured);

        state.srvs[0].srv = captured[0];
        state.srvs[1].srv = captured[1];
    } else {
        for (std::uint32_t i = 0;
             i < mutation.srv_count;
             ++i) {
            auto &capture = state.srvs[i];
            capture.slot = mutation.srvs[i].slot;
            ctx->PSGetShaderResources(
                capture.slot,
                1u,
                &capture.srv);
        }
    }

    state.sampler_count = mutation.sampler_count;
    for (std::uint32_t i = 0;
         i < mutation.sampler_count;
         ++i) {
        auto &capture = state.samplers[i];
        capture.slot = mutation.samplers[i].slot;
        ctx->PSGetSamplers(
            capture.slot,
            1u,
            &capture.sampler);
    }

    core::render_patch_plan plan{};
    for (std::size_t i = 0;
         i < core::operator_count;
         ++i) {
        const auto op =
            static_cast<core::operator_id>(i);
        if ((mutation.owners &
             core::operator_bit(op)) == 0u)
            continue;

        if (plan.patch_count >= plan.patches.size()) {
            release_state(state);
            telemetry::hot_count(begin_fail_);
            return false;
        }

        const auto bit =
            core::operator_bit(op);

        const auto carrier_mask =
            (mutation.carrier_owners & bit) != 0u
                ? core::draw_policy_carrier_write_mask(op)
                : 0u;

        if ((mutation.carrier_owners & bit) != 0u &&
            carrier_mask == 0u) {
            release_state(state);
            telemetry::hot_count(begin_fail_);
            return false;
        }

        if ((plan.carrier_write_mask &
             carrier_mask) != 0u) {
            release_state(state);
            telemetry::hot_count(begin_fail_);
            return false;
        }

        plan.patches[plan.patch_count++] = {
            op,
            carrier_mask,
            (mutation.shader_owners & bit) != 0u,
            (mutation.resource_owners & bit) != 0u,
            (mutation.constant_buffer_owners & bit) != 0u
        };
        plan.carrier_write_mask |= carrier_mask;
    }

    state.command = command_key(cmd_list);

    if (mutation.synchronous_core_transaction) {
        if (state.command == 0u ||
            plan.empty() ||
            g_local_tx.active) {
            release_state(state);
            telemetry::hot_count(begin_fail_);
            return false;
        }

        g_local_tx = {
            this,
            state.command,
            true
        };
        state.local_started = true;
    } else {
        const auto serial =
            draw_serial_.fetch_add(
                1u,
                std::memory_order_relaxed) + 1u;

        if (!core_.transactions().begin(
                state.command,
                serial,
                context_kind_of(ctx),
                plan)) {
            release_state(state);
            telemetry::hot_count(begin_fail_);
            return false;
        }
        state.core_started = true;
    }

    if (mutation.replace_pixel_shader) {
        ctx->PSSetShader(
            mutation.pixel_shader,
            nullptr,
            0u);
    }

    for (std::uint32_t i = 0;
         i < mutation.constant_buffer_count;
         ++i) {
        auto *buffer =
            mutation.constant_buffers[i].buffer;
        ctx->PSSetConstantBuffers(
            mutation.constant_buffers[i].slot,
            1u,
            &buffer);
    }

    if (batch_clustered_srvs) {
        ID3D11ShaderResourceView *srvs[2]{
            mutation.srvs[0].srv,
            mutation.srvs[1].srv
        };
        ctx->PSSetShaderResources(
            18u,
            2u,
            srvs);
    } else {
        for (std::uint32_t i = 0;
             i < mutation.srv_count;
             ++i) {
            auto *srv = mutation.srvs[i].srv;
            ctx->PSSetShaderResources(
                mutation.srvs[i].slot,
                1u,
                &srv);
        }
    }

    for (std::uint32_t i = 0;
         i < mutation.sampler_count;
         ++i) {
        auto *sampler =
            mutation.samplers[i].sampler;
        ctx->PSSetSamplers(
            mutation.samplers[i].slot,
            1u,
            &sampler);
    }

    bool bound = true;

    if (state.verify_native_readback) {
        if (mutation.replace_pixel_shader) {
            ID3D11PixelShader *shader = nullptr;
            std::array<ID3D11ClassInstance *, 1> linked{};
            UINT linked_count =
                static_cast<UINT>(linked.size());
            ctx->PSGetShader(
                &shader,
                linked.data(),
                &linked_count);
            bound =
                shader == mutation.pixel_shader &&
                linked_count == 0u;
            if (shader != nullptr)
                shader->Release();
            for (auto *instance : linked)
                if (instance != nullptr)
                    instance->Release();
        }
    
        for (std::uint32_t i = 0;
             bound &&
             i < mutation.constant_buffer_count;
             ++i) {
            ID3D11Buffer *buffer = nullptr;
            ctx->PSGetConstantBuffers(
                mutation.constant_buffers[i].slot,
                1u,
                &buffer);
            bound =
                buffer ==
                mutation.constant_buffers[i].buffer;
            if (buffer != nullptr)
                buffer->Release();
        }
    
        if (bound &&
            batch_clustered_srvs) {
            ID3D11ShaderResourceView *srvs[2]{};
            ctx->PSGetShaderResources(
                18u,
                2u,
                srvs);

            bound =
                srvs[0] == mutation.srvs[0].srv &&
                srvs[1] == mutation.srvs[1].srv;

            if (srvs[0] != nullptr)
                srvs[0]->Release();
            if (srvs[1] != nullptr)
                srvs[1]->Release();
        } else {
            for (std::uint32_t i = 0;
                 bound && i < mutation.srv_count;
                 ++i) {
                ID3D11ShaderResourceView *srv = nullptr;
                ctx->PSGetShaderResources(
                    mutation.srvs[i].slot,
                    1u,
                    &srv);
                bound = srv == mutation.srvs[i].srv;
                if (srv != nullptr)
                    srv->Release();
            }
        }
    
        for (std::uint32_t i = 0;
             bound && i < mutation.sampler_count;
             ++i) {
            ID3D11SamplerState *sampler = nullptr;
            ctx->PSGetSamplers(
                mutation.samplers[i].slot,
                1u,
                &sampler);
            bound =
                sampler ==
                mutation.samplers[i].sampler;
            if (sampler != nullptr)
                sampler->Release();
        }
    
    
    } else {
        telemetry::hot_count(native_readback_skipped_);
    }

    if (!bound) {
        telemetry::hot_count(bind_fail_);
        if (!restore(cmd_list, state))
            quarantined_.store(true);
        telemetry::hot_count(begin_fail_);
        return false;
    }

    telemetry::hot_count(begin_ok_);
    return true;
}

bool draw_state_transaction_runtime::restore(
    reshade::api::command_list *cmd_list,
    transaction_state &state) noexcept
{
    bool core_restored = true;

    if (cmd_list == nullptr ||
        (state.shader_captured &&
         state.old_shader == nullptr)) {
        if (state.local_started &&
            state.command != 0u)
            core_restored =
                restore_local_tx(
                    this,
                    state.command);
        else if (state.core_started &&
                 state.command != 0u)
            core_restored =
                core_.transactions().restore(
                    state.command);
        release_state(state);
        telemetry::hot_count(restore_fail_);
        quarantined_.store(true);
        return false;
    }

    auto *ctx = state.context;
    if (ctx == nullptr) {
        if (state.local_started &&
            state.command != 0u)
            core_restored =
                restore_local_tx(
                    this,
                    state.command);
        else if (state.core_started &&
                 state.command != 0u)
            core_restored =
                core_.transactions().restore(
                    state.command);
        (void)core_restored;
        release_state(state);
        telemetry::hot_count(restore_fail_);
        quarantined_.store(true);
        return false;
    }

    auto *ctx1 = state.context1;

    std::array<ID3D11ClassInstance *,
               draw_tx_max_class_instances> classes{};
    if (state.shader_captured) {
        for (std::uint32_t i = 0;
             i < state.old_class_count;
             ++i)
            classes[i] =
                reinterpret_cast<ID3D11ClassInstance *>(
                    state.old_classes[i]);

        ctx->PSSetShader(
            state.old_shader,
            classes.data(),
            state.old_class_count);
    }

    for (std::uint32_t i = 0;
         i < state.cb_count;
         ++i) {
        const auto &capture = state.cbs[i];
        restore_ps_constant_buffer_window(
            ctx,
            ctx1,
            capture.slot,
            capture.explicit_window
                ? capture.window
                : capture.base,
            capture.explicit_window,
            static_cast<UINT>(capture.first),
            static_cast<UINT>(capture.count));
    }

    if (state.local_started &&
        state.srv_count == 2u &&
        state.srvs[0].slot == 18u &&
        state.srvs[1].slot == 19u) {
        ID3D11ShaderResourceView *srvs[2]{
            state.srvs[0].srv,
            state.srvs[1].srv
        };
        ctx->PSSetShaderResources(
            18u,
            2u,
            srvs);
    } else {
        for (std::uint32_t i = 0;
             i < state.srv_count;
             ++i) {
            auto *srv = state.srvs[i].srv;
            ctx->PSSetShaderResources(
                state.srvs[i].slot,
                1u,
                &srv);
        }
    }

    for (std::uint32_t i = 0;
         i < state.sampler_count;
         ++i) {
        auto *sampler = state.samplers[i].sampler;
        ctx->PSSetSamplers(
            state.samplers[i].slot,
            1u,
            &sampler);
    }

    bool native_restored = true;

    if (state.verify_native_readback) {
        if (state.shader_captured) {
            ID3D11PixelShader *shader = nullptr;
            std::array<ID3D11ClassInstance *,
                       draw_tx_max_class_instances> check_classes{};
            UINT check_class_count =
                static_cast<UINT>(check_classes.size());
            ctx->PSGetShader(
                &shader,
                check_classes.data(),
                &check_class_count);

            native_restored =
                shader == state.old_shader &&
                check_class_count == state.old_class_count;

            if (native_restored) {
                for (std::uint32_t i = 0;
                     i < state.old_class_count;
                     ++i) {
                    if (check_classes[i] != classes[i]) {
                        native_restored = false;
                        break;
                    }
                }
            }

            if (shader != nullptr)
                shader->Release();
            for (std::uint32_t i = 0;
                 i < check_class_count &&
                 i < check_classes.size();
                 ++i)
                if (check_classes[i] != nullptr)
                    check_classes[i]->Release();
        }

        for (std::uint32_t i = 0;
             native_restored && i < state.cb_count;
             ++i) {
            ID3D11Buffer *buffer = nullptr;
            ctx->PSGetConstantBuffers(
                state.cbs[i].slot,
                1u,
                &buffer);
            native_restored =
                buffer == state.cbs[i].base;
            if (buffer != nullptr)
                buffer->Release();
    
            if (native_restored &&
                ctx1 != nullptr &&
                state.cbs[i].explicit_window) {
                ID3D11Buffer *window = nullptr;
                UINT first = 0u;
                UINT count = 0u;
                ctx1->PSGetConstantBuffers1(
                    state.cbs[i].slot,
                    1u,
                    &window,
                    &first,
                    &count);
                native_restored =
                    window == state.cbs[i].window &&
                    first == state.cbs[i].first &&
                    count == state.cbs[i].count;
                if (window != nullptr)
                    window->Release();
            }
        }
    
        for (std::uint32_t i = 0;
             native_restored && i < state.srv_count;
             ++i) {
            ID3D11ShaderResourceView *srv = nullptr;
            ctx->PSGetShaderResources(
                state.srvs[i].slot,
                1u,
                &srv);
            native_restored =
                srv == state.srvs[i].srv;
            if (srv != nullptr)
                srv->Release();
        }
    
        for (std::uint32_t i = 0;
             native_restored &&
             i < state.sampler_count;
             ++i) {
            ID3D11SamplerState *sampler = nullptr;
            ctx->PSGetSamplers(
                state.samplers[i].slot,
                1u,
                &sampler);
            native_restored =
                sampler == state.samplers[i].sampler;
            if (sampler != nullptr)
                sampler->Release();
        }
    
    
    }

    if (state.local_started &&
        state.command != 0u)
        core_restored =
            restore_local_tx(
                this,
                state.command);
    else if (state.core_started &&
             state.command != 0u)
        core_restored =
            core_.transactions().restore(
                state.command);

    release_state(state);

    if (!native_restored || !core_restored) {
        telemetry::hot_count(restore_fail_);
        quarantined_.store(true);
        return false;
    }

    telemetry::hot_count(restore_ok_);
    return true;
}

draw_tx_result draw_state_transaction_runtime::replay_draw(
    reshade::api::command_list *cmd_list,
    const draw_tx_mutation &mutation,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept
{
#ifdef DSRRL_POINTLIGHT_PROFILE
    const bool profile_sample =
        mutation.synchronous_core_transaction &&
        ((++g_pointlight_tx_profile_counter &
          (k_pointlight_tx_profile_sample_period - 1u)) == 0u);
    const auto profile_total_start =
        profile_sample ? pointlight_profile_qpc() : 0u;
    const auto profile_begin_start = profile_total_start;
#endif

    transaction_state state{};
    if (!begin(cmd_list, mutation, state)) {
#ifdef DSRRL_POINTLIGHT_PROFILE
        if (profile_sample) {
            const auto end = pointlight_profile_qpc();
            const auto begin_ticks = end - profile_begin_start;
            const auto total_ticks = end - profile_total_start;
            const auto profile_sample_index =
                profile_samples_.fetch_add(
                    1u,
                    std::memory_order_relaxed) + 1u;
            profile_begin_ticks_.fetch_add(begin_ticks, std::memory_order_relaxed);
            profile_total_ticks_.fetch_add(total_ticks, std::memory_order_relaxed);
            pointlight_profile_max(profile_max_total_ticks_, total_ticks);
            maybe_log_pointlight_profile(profile_sample_index);
        }
#endif
        return draw_tx_result::not_issued;
    }

#ifdef DSRRL_POINTLIGHT_PROFILE
    if (profile_sample) {
        const auto after_begin = pointlight_profile_qpc();
        profile_begin_ticks_.fetch_add(
            after_begin - profile_begin_start,
            std::memory_order_relaxed);
    }
    const auto profile_draw_start =
        profile_sample ? pointlight_profile_qpc() : 0u;
#endif

    auto *ctx = state.context;

    if (instance_count == 1u &&
        first_instance == 0u) {
        ctx->Draw(
            vertex_count,
            first_vertex);
    } else {
        ctx->DrawInstanced(
            vertex_count,
            instance_count,
            first_vertex,
            first_instance);
    }

    telemetry::hot_count(draws_issued_);

#ifdef DSRRL_POINTLIGHT_PROFILE
    if (profile_sample) {
        const auto after_draw = pointlight_profile_qpc();
        profile_draw_ticks_.fetch_add(
            after_draw - profile_draw_start,
            std::memory_order_relaxed);
    }
    const auto profile_restore_start =
        profile_sample ? pointlight_profile_qpc() : 0u;
#endif

    const bool restored = restore(cmd_list, state);

#ifdef DSRRL_POINTLIGHT_PROFILE
    if (profile_sample) {
        const auto end = pointlight_profile_qpc();
        const auto restore_ticks = end - profile_restore_start;
        const auto total_ticks = end - profile_total_start;
        const auto profile_sample_index =
                profile_samples_.fetch_add(
                    1u,
                    std::memory_order_relaxed) + 1u;
        profile_restore_ticks_.fetch_add(
            restore_ticks,
            std::memory_order_relaxed);
        profile_total_ticks_.fetch_add(
            total_ticks,
            std::memory_order_relaxed);
        pointlight_profile_max(
            profile_max_total_ticks_,
            total_ticks);
        maybe_log_pointlight_profile(profile_sample_index);
    }
#endif

    return restored
        ? draw_tx_result::issued_restored
        : draw_tx_result::issued_restore_failed;
}

draw_tx_result
draw_state_transaction_runtime::replay_draw_indexed(
    reshade::api::command_list *cmd_list,
    const draw_tx_mutation &mutation,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept
{
#ifdef DSRRL_POINTLIGHT_PROFILE
    const bool profile_sample =
        mutation.synchronous_core_transaction &&
        ((++g_pointlight_tx_profile_counter &
          (k_pointlight_tx_profile_sample_period - 1u)) == 0u);
    const auto profile_total_start =
        profile_sample ? pointlight_profile_qpc() : 0u;
    const auto profile_begin_start = profile_total_start;
#endif

    transaction_state state{};
    if (!begin(cmd_list, mutation, state)) {
#ifdef DSRRL_POINTLIGHT_PROFILE
        if (profile_sample) {
            const auto end = pointlight_profile_qpc();
            const auto begin_ticks = end - profile_begin_start;
            const auto total_ticks = end - profile_total_start;
            const auto profile_sample_index =
                profile_samples_.fetch_add(
                    1u,
                    std::memory_order_relaxed) + 1u;
            profile_begin_ticks_.fetch_add(begin_ticks, std::memory_order_relaxed);
            profile_total_ticks_.fetch_add(total_ticks, std::memory_order_relaxed);
            pointlight_profile_max(profile_max_total_ticks_, total_ticks);
            maybe_log_pointlight_profile(profile_sample_index);
        }
#endif
        return draw_tx_result::not_issued;
    }

#ifdef DSRRL_POINTLIGHT_PROFILE
    if (profile_sample) {
        const auto after_begin = pointlight_profile_qpc();
        profile_begin_ticks_.fetch_add(
            after_begin - profile_begin_start,
            std::memory_order_relaxed);
    }
    const auto profile_draw_start =
        profile_sample ? pointlight_profile_qpc() : 0u;
#endif

    auto *ctx = state.context;

    if (instance_count == 1u &&
        first_instance == 0u) {
        ctx->DrawIndexed(
            index_count,
            first_index,
            vertex_offset);
    } else {
        ctx->DrawIndexedInstanced(
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance);
    }

    telemetry::hot_count(draws_issued_);

#ifdef DSRRL_POINTLIGHT_PROFILE
    if (profile_sample) {
        const auto after_draw = pointlight_profile_qpc();
        profile_draw_ticks_.fetch_add(
            after_draw - profile_draw_start,
            std::memory_order_relaxed);
    }
    const auto profile_restore_start =
        profile_sample ? pointlight_profile_qpc() : 0u;
#endif

    const bool restored = restore(cmd_list, state);

#ifdef DSRRL_POINTLIGHT_PROFILE
    if (profile_sample) {
        const auto end = pointlight_profile_qpc();
        const auto restore_ticks = end - profile_restore_start;
        const auto total_ticks = end - profile_total_start;
        const auto profile_sample_index =
                profile_samples_.fetch_add(
                    1u,
                    std::memory_order_relaxed) + 1u;
        profile_restore_ticks_.fetch_add(
            restore_ticks,
            std::memory_order_relaxed);
        profile_total_ticks_.fetch_add(
            total_ticks,
            std::memory_order_relaxed);
        pointlight_profile_max(
            profile_max_total_ticks_,
            total_ticks);
        maybe_log_pointlight_profile(profile_sample_index);
    }
#endif

    return restored
        ? draw_tx_result::issued_restored
        : draw_tx_result::issued_restore_failed;
}

bool draw_state_transaction_runtime::mutate_restore_only(
    reshade::api::command_list *cmd_list,
    const draw_tx_mutation &mutation) noexcept
{
    transaction_state state{};
    if (!begin(cmd_list, mutation, state))
        return false;
    return restore(cmd_list, state);
}

bool draw_state_transaction_runtime::capture_only(
    reshade::api::command_list *cmd_list,
    const draw_tx_mutation &mutation) noexcept
{
    if (cmd_list == nullptr ||
        quarantined_.load() ||
        !validate_mutation(mutation))
        return false;

    auto *ctx = reinterpret_cast<ID3D11DeviceContext *>(
        cmd_list->get_native());
    if (ctx == nullptr)
        return false;

    transaction_state state{};
    state.context = ctx;

    if (mutation.constant_buffer_count != 0u)
        state.context1 = context1_for(ctx);

    if (mutation.replace_pixel_shader) {
        std::array<ID3D11ClassInstance *,
                   draw_tx_max_class_instances> classes{};
        UINT class_count = static_cast<UINT>(classes.size());
        ctx->PSGetShader(
            &state.old_shader,
            classes.data(),
            &class_count);
        if (state.old_shader == nullptr ||
            class_count > classes.size()) {
            release_unadopted_ps_classes(classes);
            release_state(state);
            return false;
        }
        state.old_class_count = class_count;
        for (std::uint32_t i = 0; i < class_count; ++i)
            state.old_classes[i] = classes[i];
        state.shader_captured = true;
    }

    state.cb_count = mutation.constant_buffer_count;
    for (std::uint32_t i = 0; i < state.cb_count; ++i) {
        auto &capture = state.cbs[i];
        capture.slot = mutation.constant_buffers[i].slot;
        ctx->PSGetConstantBuffers(capture.slot, 1u, &capture.base);

        if (state.context1 != nullptr) {
            UINT first = 0u;
            UINT count = 0u;
            state.context1->PSGetConstantBuffers1(
                capture.slot,
                1u,
                &capture.window,
                &first,
                &count);
            capture.first = first;
            capture.count = count;
            capture.coherent = capture.base == capture.window;
            capture.explicit_window =
                capture.window != nullptr &&
                count >= 16u &&
                (first % 16u) == 0u &&
                (count % 16u) == 0u;
        }

        if (!capture.coherent) {
            release_state(state);
            return false;
        }
    }

    state.srv_count = mutation.srv_count;
    for (std::uint32_t i = 0; i < state.srv_count; ++i) {
        state.srvs[i].slot = mutation.srvs[i].slot;
        ctx->PSGetShaderResources(
            state.srvs[i].slot,
            1u,
            &state.srvs[i].srv);
    }

    state.sampler_count = mutation.sampler_count;
    for (std::uint32_t i = 0; i < state.sampler_count; ++i) {
        state.samplers[i].slot = mutation.samplers[i].slot;
        ctx->PSGetSamplers(
            state.samplers[i].slot,
            1u,
            &state.samplers[i].sampler);
    }

    release_state(state);
    return true;
}

bool draw_state_transaction_runtime::native_mutate_restore_only(
    reshade::api::command_list *cmd_list,
    const draw_tx_mutation &mutation) noexcept
{
    if (cmd_list == nullptr ||
        quarantined_.load() ||
        !validate_mutation(mutation))
        return false;

    auto *ctx = reinterpret_cast<ID3D11DeviceContext *>(
        cmd_list->get_native());
    if (ctx == nullptr)
        return false;

    transaction_state state{};
    state.context = ctx;

    if (mutation.constant_buffer_count != 0u)
        state.context1 = context1_for(ctx);

    if (mutation.replace_pixel_shader) {
        std::array<ID3D11ClassInstance *,
                   draw_tx_max_class_instances> classes{};
        UINT class_count = static_cast<UINT>(classes.size());
        ctx->PSGetShader(
            &state.old_shader,
            classes.data(),
            &class_count);
        if (state.old_shader == nullptr ||
            class_count > classes.size()) {
            release_unadopted_ps_classes(classes);
            release_state(state);
            return false;
        }
        state.old_class_count = class_count;
        for (std::uint32_t i = 0; i < class_count; ++i)
            state.old_classes[i] = classes[i];
        state.shader_captured = true;
    }

    state.cb_count = mutation.constant_buffer_count;
    for (std::uint32_t i = 0; i < state.cb_count; ++i) {
        auto &capture = state.cbs[i];
        capture.slot = mutation.constant_buffers[i].slot;
        ctx->PSGetConstantBuffers(capture.slot, 1u, &capture.base);
        if (state.context1 != nullptr) {
            UINT first = 0u, count = 0u;
            state.context1->PSGetConstantBuffers1(
                capture.slot, 1u, &capture.window, &first, &count);
            capture.first = first;
            capture.count = count;
            capture.coherent = capture.base == capture.window;
            capture.explicit_window =
                capture.window != nullptr &&
                count >= 16u &&
                (first % 16u) == 0u &&
                (count % 16u) == 0u;
        }
        if (!capture.coherent) {
            release_state(state);
            return false;
        }
    }

    state.srv_count = mutation.srv_count;
    for (std::uint32_t i = 0; i < state.srv_count; ++i) {
        state.srvs[i].slot = mutation.srvs[i].slot;
        ctx->PSGetShaderResources(
            state.srvs[i].slot, 1u, &state.srvs[i].srv);
    }

    state.sampler_count = mutation.sampler_count;
    for (std::uint32_t i = 0; i < state.sampler_count; ++i) {
        state.samplers[i].slot = mutation.samplers[i].slot;
        ctx->PSGetSamplers(
            state.samplers[i].slot, 1u, &state.samplers[i].sampler);
    }

    if (mutation.replace_pixel_shader)
        ctx->PSSetShader(mutation.pixel_shader, nullptr, 0u);
    for (std::uint32_t i = 0; i < mutation.constant_buffer_count; ++i) {
        auto *buffer = mutation.constant_buffers[i].buffer;
        ctx->PSSetConstantBuffers(
            mutation.constant_buffers[i].slot, 1u, &buffer);
    }
    for (std::uint32_t i = 0; i < mutation.srv_count; ++i) {
        auto *srv = mutation.srvs[i].srv;
        ctx->PSSetShaderResources(mutation.srvs[i].slot, 1u, &srv);
    }
    for (std::uint32_t i = 0; i < mutation.sampler_count; ++i) {
        auto *sampler = mutation.samplers[i].sampler;
        ctx->PSSetSamplers(mutation.samplers[i].slot, 1u, &sampler);
    }

    if (state.shader_captured) {
        std::array<ID3D11ClassInstance *,
                   draw_tx_max_class_instances> classes{};
        for (std::uint32_t i = 0; i < state.old_class_count; ++i)
            classes[i] = reinterpret_cast<ID3D11ClassInstance *>(
                state.old_classes[i]);
        ctx->PSSetShader(
            state.old_shader,
            classes.data(),
            state.old_class_count);
    }
    for (std::uint32_t i = 0; i < state.cb_count; ++i) {
        const auto &capture = state.cbs[i];
        restore_ps_constant_buffer_window(
            ctx,
            state.context1,
            capture.slot,
            capture.explicit_window ? capture.window : capture.base,
            capture.explicit_window,
            static_cast<UINT>(capture.first),
            static_cast<UINT>(capture.count));
    }
    for (std::uint32_t i = 0; i < state.srv_count; ++i) {
        auto *srv = state.srvs[i].srv;
        ctx->PSSetShaderResources(state.srvs[i].slot, 1u, &srv);
    }
    for (std::uint32_t i = 0; i < state.sampler_count; ++i) {
        auto *sampler = state.samplers[i].sampler;
        ctx->PSSetSamplers(state.samplers[i].slot, 1u, &sampler);
    }

    release_state(state);
    return true;
}

bool draw_state_transaction_runtime::core_transaction_only(
    reshade::api::command_list *cmd_list,
    const draw_tx_mutation &mutation) noexcept
{
    if (cmd_list == nullptr ||
        quarantined_.load() ||
        !validate_mutation(mutation))
        return false;

    auto *ctx = reinterpret_cast<ID3D11DeviceContext *>(
        cmd_list->get_native());
    if (ctx == nullptr)
        return false;

    core::render_patch_plan plan{};
    for (std::size_t i = 0; i < core::operator_count; ++i) {
        const auto op = static_cast<core::operator_id>(i);
        const auto bit = core::operator_bit(op);
        if ((mutation.owners & bit) == 0u)
            continue;

        const auto carrier_mask =
            (mutation.carrier_owners & bit) != 0u
                ? core::draw_policy_carrier_write_mask(op)
                : 0u;
        if (plan.patch_count >= plan.patches.size())
            return false;

        plan.patches[plan.patch_count++] = {
            op,
            carrier_mask,
            (mutation.shader_owners & bit) != 0u,
            (mutation.resource_owners & bit) != 0u,
            (mutation.constant_buffer_owners & bit) != 0u
        };
        plan.carrier_write_mask |= carrier_mask;
    }

    const auto command = command_key(cmd_list);
    const auto serial =
        draw_serial_.fetch_add(1u, std::memory_order_relaxed) + 1u;
    if (!core_.transactions().begin(
            command,
            serial,
            context_kind_of(ctx),
            plan))
        return false;

    return core_.transactions().restore(command);
}

bool draw_state_transaction_runtime::raw_replay_draw(
    reshade::api::command_list *cmd_list,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept
{
    if (cmd_list == nullptr)
        return false;

    auto *ctx =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (ctx == nullptr)
        return false;

    if (instance_count == 1u &&
        first_instance == 0u) {
        ctx->Draw(
            vertex_count,
            first_vertex);
    } else {
        ctx->DrawInstanced(
            vertex_count,
            instance_count,
            first_vertex,
            first_instance);
    }
    return true;
}

bool draw_state_transaction_runtime::raw_replay_draw_indexed(
    reshade::api::command_list *cmd_list,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept
{
    if (cmd_list == nullptr)
        return false;

    auto *ctx =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (ctx == nullptr)
        return false;

    if (instance_count == 1u &&
        first_instance == 0u) {
        ctx->DrawIndexed(
            index_count,
            first_index,
            vertex_offset);
    } else {
        ctx->DrawIndexedInstanced(
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance);
    }
    return true;
}

void draw_state_transaction_runtime::maybe_log_pointlight_profile(
    std::uint64_t sample_index) const noexcept
{
#ifdef DSRRL_POINTLIGHT_PROFILE
    if (sample_index != 1u &&
        (sample_index % 16u) != 0u)
        return;

    const auto profile = telemetry();
    if (profile.profile_samples == 0u ||
        profile.profile_qpc_frequency == 0u)
        return;

    const auto avg_us =
        [frequency = profile.profile_qpc_frequency,
         samples = profile.profile_samples](
            std::uint64_t ticks) noexcept {
            return (static_cast<double>(ticks) * 1000000.0) /
                   (static_cast<double>(frequency) *
                    static_cast<double>(samples));
        };
    const auto ticks_us =
        [frequency = profile.profile_qpc_frequency](
            std::uint64_t ticks) noexcept {
            return (static_cast<double>(ticks) * 1000000.0) /
                   static_cast<double>(frequency);
        };

    char line[768]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL PERF R32] POINTLIGHT_TX sample=1/%u n=%llu total_us=%.3f max_total_us=%.3f begin_capture_mutate_us=%.3f native_draw_us=%.3f restore_us=%.3f",
        profile.profile_sample_period,
        static_cast<unsigned long long>(
            profile.profile_samples),
        avg_us(profile.profile_total_ticks),
        ticks_us(profile.profile_max_total_ticks),
        avg_us(profile.profile_begin_ticks),
        avg_us(profile.profile_draw_ticks),
        avg_us(profile.profile_restore_ticks));
    reshade::log::message(
        reshade::log::level::info,
        line);
#else
    (void)sample_index;
#endif
}

draw_tx_telemetry
draw_state_transaction_runtime::telemetry() const noexcept
{
    return {
        begin_ok_.load(),
        begin_fail_.load(),
        bind_fail_.load(),
        draws_issued_.load(),
        restore_ok_.load(),
        restore_fail_.load(),
        native_readback_skipped_.load(),
#ifdef DSRRL_POINTLIGHT_PROFILE
        k_pointlight_tx_profile_sample_period,
        pointlight_profile_qpc_frequency(),
#else
        0u,
        0u,
#endif
        profile_samples_.load(),
        profile_begin_ticks_.load(),
        profile_draw_ticks_.load(),
        profile_restore_ticks_.load(),
        profile_total_ticks_.load(),
        profile_max_total_ticks_.load(),
        quarantined_.load()
    };
}

bool draw_state_transaction_runtime::quarantined() const noexcept
{
    return quarantined_.load();
}

void draw_state_transaction_runtime::on_destroy_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());
    if (native == nullptr)
        return;

    if (g_local_tx.owner == this)
        g_local_tx = {};

    // Invalidate thread-local borrowed pointers before releasing cached COM
    // references. ReShade device teardown is the lifetime boundary for all
    // recording contexts owned by this device.
    context1_cache_epoch_.fetch_add(
        1u,
        std::memory_order_acq_rel);

    std::lock_guard<std::mutex> lock(
        context1_cache_mutex_);
    for (auto it = context1_cache_.begin();
         it != context1_cache_.end();) {
        if (it->second.device != native) {
            ++it;
            continue;
        }

        if (it->second.context1 != nullptr)
            it->second.context1->Release();
        if (it->second.device != nullptr)
            it->second.device->Release();
        it = context1_cache_.erase(it);
    }
}

void draw_state_transaction_runtime::reset() noexcept
{
    if (g_local_tx.owner == this)
        g_local_tx = {};
    release_context1_cache();
    draw_serial_.store(0);
    begin_ok_.store(0);
    begin_fail_.store(0);
    bind_fail_.store(0);
    draws_issued_.store(0);
    restore_ok_.store(0);
    restore_fail_.store(0);
    native_readback_skipped_.store(0);
    profile_samples_.store(0);
    profile_begin_ticks_.store(0);
    profile_draw_ticks_.store(0);
    profile_restore_ticks_.store(0);
    profile_total_ticks_.store(0);
    profile_max_total_ticks_.store(0);
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
