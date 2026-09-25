#include "dsrrl/runtime/draw_state_transaction.hpp"
#include "dsrrl/runtime/d3d11_cb_window.hpp"
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

} // namespace

draw_state_transaction_runtime::draw_state_transaction_runtime(
    core::renderer_core &core) noexcept
    : core_(core)
{
}

bool draw_state_transaction_runtime::validate_mutation(
    const draw_tx_mutation &mutation) const noexcept
{
    if (mutation.owners == 0u ||
        (mutation.owners & ~core::all_operator_bits) != 0u ||
        (mutation.shader_owners & ~mutation.owners) != 0u ||
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

    const bool has_resources =
        mutation.srv_count != 0u ||
        mutation.sampler_count != 0u;

    if (has_resources !=
        (mutation.resource_owners != 0u))
        return false;

    if (mutation.carrier_owners != 0u &&
        mutation.constant_buffer_count == 0u)
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

    state = {};
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
        ++begin_fail_;
        return false;
    }

    auto *ctx =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (ctx == nullptr) {
        ++begin_fail_;
        return false;
    }

    ID3D11DeviceContext1 *ctx1 = nullptr;
    (void)ctx->QueryInterface(
        __uuidof(ID3D11DeviceContext1),
        reinterpret_cast<void **>(&ctx1));

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
        if (ctx1 != nullptr)
            ctx1->Release();
        release_state(state);
        ++begin_fail_;
        return false;
    }

    state.old_class_count = class_count;
    for (std::uint32_t i = 0;
         i < state.old_class_count;
         ++i)
        state.old_classes[i] = classes[i];

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
            if (ctx1 != nullptr)
                ctx1->Release();
            release_state(state);
            ++begin_fail_;
            return false;
        }
    }

    state.srv_count = mutation.srv_count;
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
            if (ctx1 != nullptr)
                ctx1->Release();
            release_state(state);
            ++begin_fail_;
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
            if (ctx1 != nullptr)
                ctx1->Release();
            release_state(state);
            ++begin_fail_;
            return false;
        }

        plan.patches[plan.patch_count++] = {
            op,
            carrier_mask,
            (mutation.shader_owners & bit) != 0u,
            (mutation.resource_owners & bit) != 0u
        };
        plan.carrier_write_mask |= carrier_mask;
    }

    state.command = command_key(cmd_list);
    if (!core_.transactions().begin(
            state.command,
            ++draw_serial_,
            context_kind_of(ctx),
            plan)) {
        if (ctx1 != nullptr)
            ctx1->Release();
        release_state(state);
        ++begin_fail_;
        return false;
    }
    state.core_started = true;

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

    for (std::uint32_t i = 0;
         i < mutation.srv_count;
         ++i) {
        auto *srv = mutation.srvs[i].srv;
        ctx->PSSetShaderResources(
            mutation.srvs[i].slot,
            1u,
            &srv);
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

    if (ctx1 != nullptr)
        ctx1->Release();

    if (!bound) {
        ++bind_fail_;
        if (!restore(cmd_list, state))
            quarantined_.store(true);
        ++begin_fail_;
        return false;
    }

    ++begin_ok_;
    return true;
}

bool draw_state_transaction_runtime::restore(
    reshade::api::command_list *cmd_list,
    transaction_state &state) noexcept
{
    bool core_restored = true;

    if (cmd_list == nullptr ||
        state.old_shader == nullptr) {
        if (state.core_started && state.command != 0u)
            core_restored =
                core_.transactions().restore(
                    state.command);
        release_state(state);
        ++restore_fail_;
        quarantined_.store(true);
        return false;
    }

    auto *ctx =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (ctx == nullptr) {
        if (state.core_started && state.command != 0u)
            core_restored =
                core_.transactions().restore(
                    state.command);
        (void)core_restored;
        release_state(state);
        ++restore_fail_;
        quarantined_.store(true);
        return false;
    }

    ID3D11DeviceContext1 *ctx1 = nullptr;
    (void)ctx->QueryInterface(
        __uuidof(ID3D11DeviceContext1),
        reinterpret_cast<void **>(&ctx1));

    std::array<ID3D11ClassInstance *,
               draw_tx_max_class_instances> classes{};
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

    for (std::uint32_t i = 0;
         i < state.srv_count;
         ++i) {
        auto *srv = state.srvs[i].srv;
        ctx->PSSetShaderResources(
            state.srvs[i].slot,
            1u,
            &srv);
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

    if (ctx1 != nullptr)
        ctx1->Release();

    if (state.core_started && state.command != 0u)
        core_restored =
            core_.transactions().restore(
                state.command);

    release_state(state);

    if (!native_restored || !core_restored) {
        ++restore_fail_;
        quarantined_.store(true);
        return false;
    }

    ++restore_ok_;
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
    transaction_state state{};
    if (!begin(cmd_list, mutation, state))
        return draw_tx_result::not_issued;

    auto *ctx =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());

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

    ++draws_issued_;

    return restore(cmd_list, state)
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
    transaction_state state{};
    if (!begin(cmd_list, mutation, state))
        return draw_tx_result::not_issued;

    auto *ctx =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());

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

    ++draws_issued_;

    return restore(cmd_list, state)
        ? draw_tx_result::issued_restored
        : draw_tx_result::issued_restore_failed;
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
        quarantined_.load()
    };
}

bool draw_state_transaction_runtime::quarantined() const noexcept
{
    return quarantined_.load();
}

void draw_state_transaction_runtime::reset() noexcept
{
    draw_serial_.store(0);
    begin_ok_.store(0);
    begin_fail_.store(0);
    bind_fail_.store(0);
    draws_issued_.store(0);
    restore_ok_.store(0);
    restore_fail_.store(0);
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
