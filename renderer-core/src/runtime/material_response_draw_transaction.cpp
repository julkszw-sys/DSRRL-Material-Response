#include "dsrrl/runtime/material_response_draw_transaction.hpp"
#include "dsrrl/runtime/d3d11_cb_window.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <d3d11_1.h>

#include <array>

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

void release_classes(
    std::array<ID3D11ClassInstance *, 256> &classes) noexcept
{
    for (auto *instance : classes)
        if (instance != nullptr)
            instance->Release();
}

bool full_material_response_decision(
    const operators::material_response::decision &decision) noexcept
{
    using namespace operators::material_response;

    constexpr std::uint32_t required =
        diffuse_material_domain_linear |
        specular_factor_c101;

    return
        decision.active &&
        (decision.certified_operations & required) == required;
}

} // namespace

material_response_draw_runtime::material_response_draw_runtime(
    core::renderer_core &core) noexcept
    : core_(core)
{
}

material_response_draw_runtime::~material_response_draw_runtime()
{
    release_resources();
}

void material_response_draw_runtime::release_resources() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &entry : replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    replacements_.clear();

    for (auto &entry : b12_by_route_) {
        auto *buffer = entry.second;
        if (buffer != nullptr)
            buffer->Release();
    }
    b12_by_route_.clear();

    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

void material_response_draw_runtime::on_init_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(device->get_native());
    if (native == nullptr)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ == nullptr) {
        native->AddRef();
        device_ = native;
        return;
    }

    if (device_ != native)
        quarantined_.store(true);
}

void material_response_draw_runtime::on_destroy_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(device->get_native());

    std::lock_guard<std::mutex> lock(mutex_);
    if (native != device_)
        return;

    for (auto &entry : replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    replacements_.clear();

    for (auto &entry : b12_by_route_) {
        auto *buffer = entry.second;
        if (buffer != nullptr)
            buffer->Release();
    }
    b12_by_route_.clear();

    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

bool material_response_draw_runtime::register_receiver_replacement(
    std::uint32_t receiver_id,
    const void *dxbc,
    std::size_t dxbc_size,
    core::operator_mask composed_owners) noexcept
{
    const core::operator_mask forbidden_owners =
        core::operator_bit(core::operator_id::material_response) |
        core::operator_bit(core::operator_id::diffuse_material_domain);

    if (receiver_id == 0u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        (composed_owners & ~core::all_operator_bits) != 0u ||
        (composed_owners & forbidden_owners) != 0u ||
        quarantined_.load()) {
        ++replacement_register_fail_;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ == nullptr) {
        ++replacement_register_fail_;
        return false;
    }

    ID3D11PixelShader *shader = nullptr;
    if (FAILED(device_->CreatePixelShader(
            dxbc,
            dxbc_size,
            nullptr,
            &shader)) ||
        shader == nullptr) {
        ++replacement_register_fail_;
        return false;
    }

    const replacement_record record{
        shader,
        composed_owners
    };

    const auto found = replacements_.find(receiver_id);
    if (found != replacements_.end()) {
        if (found->second.shader != nullptr)
            found->second.shader->Release();
        found->second = record;
    } else {
        replacements_.emplace(receiver_id, record);
    }

    ++replacement_register_ok_;
    return true;
}

bool material_response_draw_runtime::has_receiver_replacement(
    std::uint32_t receiver_id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = replacements_.find(receiver_id);
    return
        found != replacements_.end() &&
        found->second.shader != nullptr;
}

ID3D11Buffer *material_response_draw_runtime::realize_b12(
    const operators::material_response::decision &decision) noexcept
{
    if (!full_material_response_decision(decision) ||
        quarantined_.load())
        return nullptr;

    std::lock_guard<std::mutex> lock(mutex_);

    if (device_ == nullptr)
        return nullptr;

    const auto found =
        b12_by_route_.find(decision.route_index);

    if (found != b12_by_route_.end() &&
        found->second != nullptr) {
        found->second->AddRef();
        ++b12_hit_;
        return found->second;
    }

    struct alignas(16) f4 {
        float x;
        float y;
        float z;
        float w;
    };

    const std::array<f4, 4> payload{{
        {
            decision.c101_f0q[0],
            decision.c101_f0q[1],
            decision.c101_f0q[2],
            1.0f
        },
        {
            decision.c100[0],
            decision.c100[1],
            decision.c100[2],
            1.0f
        },
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f}
    }};

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>(sizeof(payload));
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = payload.data();

    ID3D11Buffer *buffer = nullptr;
    if (FAILED(device_->CreateBuffer(
            &desc,
            &init,
            &buffer)) ||
        buffer == nullptr)
        return nullptr;

    b12_by_route_.emplace(
        decision.route_index,
        buffer);

    buffer->AddRef();
    ++b12_create_;
    return buffer;
}

void material_response_draw_runtime::release_transaction(
    native_transaction &state) noexcept
{
    if (state.old_shader != nullptr)
        state.old_shader->Release();

    if (state.old_b12.base != nullptr)
        state.old_b12.base->Release();

    if (state.old_b12.window != nullptr)
        state.old_b12.window->Release();

    state = {};
}

bool material_response_draw_runtime::begin_native_transaction(
    reshade::api::command_list *cmd_list,
    const operators::material_response::decision &decision,
    const replacement_record &replacement,
    ID3D11Buffer *b12,
    native_transaction &state) noexcept
{
    state = {};

    if (cmd_list == nullptr ||
        replacement.shader == nullptr ||
        b12 == nullptr ||
        !full_material_response_decision(decision) ||
        quarantined_.load())
        return false;

    auto *ctx =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (ctx == nullptr)
        return false;

    ID3D11DeviceContext1 *ctx1 = nullptr;
    (void)ctx->QueryInterface(
        __uuidof(ID3D11DeviceContext1),
        reinterpret_cast<void **>(&ctx1));

    std::array<ID3D11ClassInstance *, 256> classes{};
    UINT class_count =
        static_cast<UINT>(classes.size());

    ctx->PSGetShader(
        &state.old_shader,
        classes.data(),
        &class_count);

    if (class_count != 0u) {
        release_classes(classes);
        if (ctx1 != nullptr)
            ctx1->Release();
        release_transaction(state);
        return false;
    }
    release_classes(classes);

    if (state.old_shader == nullptr) {
        if (ctx1 != nullptr)
            ctx1->Release();
        release_transaction(state);
        return false;
    }

    ctx->PSGetConstantBuffers(
        12u,
        1u,
        &state.old_b12.base);

    if (ctx1 != nullptr) {
        UINT first = 0u;
        UINT count = 0u;

        ctx1->PSGetConstantBuffers1(
            12u,
            1u,
            &state.old_b12.window,
            &first,
            &count);

        state.old_b12.first = first;
        state.old_b12.count = count;
        state.old_b12.coherent =
            state.old_b12.base ==
            state.old_b12.window;

        state.old_b12.explicit_window =
            state.old_b12.window != nullptr &&
            count >= 16u &&
            (first % 16u) == 0u &&
            (count % 16u) == 0u;
    }

    if (!state.old_b12.coherent) {
        if (ctx1 != nullptr)
            ctx1->Release();
        release_transaction(state);
        return false;
    }

    core::render_patch_plan plan{};

    plan.patches[plan.patch_count++] = {
        core::operator_id::material_response,
        0u,
        true,
        false
    };

    if ((decision.certified_operations &
         operators::material_response::
             diffuse_material_domain_linear) != 0u) {
        plan.patches[plan.patch_count++] = {
            core::operator_id::diffuse_material_domain,
            0u,
            true,
            false
        };
    }

    for (std::size_t i = 0;
         i < core::operator_count;
         ++i) {
        const auto op =
            static_cast<core::operator_id>(i);
        const auto bit = core::operator_bit(op);

        if ((replacement.composed_owners & bit) == 0u)
            continue;

        if (plan.patch_count >= plan.patches.size())
            return false;

        plan.patches[plan.patch_count++] = {
            op,
            0u,
            true,
            false
        };
    }

    const auto command = command_key(cmd_list);
    const auto serial = ++draw_serial_;

    if (!core_.transactions().begin(
            command,
            serial,
            context_kind_of(ctx),
            plan)) {
        if (ctx1 != nullptr)
            ctx1->Release();
        release_transaction(state);
        return false;
    }

    state.command = command;
    state.core_started = true;

    ctx->PSSetShader(
        replacement.shader,
        nullptr,
        0u);

    ID3D11Buffer *owned_b12 = b12;
    ctx->PSSetConstantBuffers(
        12u,
        1u,
        &owned_b12);

    ID3D11PixelShader *check_shader = nullptr;
    UINT check_class_count = 0u;
    ctx->PSGetShader(
        &check_shader,
        nullptr,
        &check_class_count);

    ID3D11Buffer *check_b12 = nullptr;
    ctx->PSGetConstantBuffers(
        12u,
        1u,
        &check_b12);

    const bool bound =
        check_shader == replacement.shader &&
        check_class_count == 0u &&
        check_b12 == b12;

    if (check_shader != nullptr)
        check_shader->Release();
    if (check_b12 != nullptr)
        check_b12->Release();

    if (ctx1 != nullptr)
        ctx1->Release();

    if (!bound) {
        ++b12_bind_fail_;
        (void)restore_native_transaction(
            cmd_list,
            state);
        return false;
    }

    return true;
}

bool material_response_draw_runtime::restore_native_transaction(
    reshade::api::command_list *cmd_list,
    native_transaction &state) noexcept
{
    if (state.old_shader == nullptr) {
        if (state.core_started && state.command != 0u)
            (void)core_.transactions().restore(state.command);
        release_transaction(state);
        return false;
    }

    if (cmd_list == nullptr) {
        if (state.core_started && state.command != 0u)
            (void)core_.transactions().restore(state.command);
        release_transaction(state);
        return false;
    }

    auto *ctx =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (ctx == nullptr) {
        if (state.core_started && state.command != 0u)
            (void)core_.transactions().restore(state.command);
        release_transaction(state);
        return false;
    }

    ID3D11DeviceContext1 *ctx1 = nullptr;
    (void)ctx->QueryInterface(
        __uuidof(ID3D11DeviceContext1),
        reinterpret_cast<void **>(&ctx1));

    ctx->PSSetShader(
        state.old_shader,
        nullptr,
        0u);

    restore_ps_constant_buffer_window(
        ctx,
        ctx1,
        12u,
        state.old_b12.explicit_window
            ? state.old_b12.window
            : state.old_b12.base,
        state.old_b12.explicit_window,
        static_cast<UINT>(state.old_b12.first),
        static_cast<UINT>(state.old_b12.count));

    ID3D11PixelShader *check_shader = nullptr;
    UINT check_class_count = 0u;
    ctx->PSGetShader(
        &check_shader,
        nullptr,
        &check_class_count);

    bool native_restored =
        check_shader == state.old_shader &&
        check_class_count == 0u;

    if (check_shader != nullptr)
        check_shader->Release();

    ID3D11Buffer *check_base = nullptr;
    ctx->PSGetConstantBuffers(
        12u,
        1u,
        &check_base);

    native_restored =
        native_restored &&
        check_base == state.old_b12.base;

    if (check_base != nullptr)
        check_base->Release();

    if (ctx1 != nullptr) {
        ID3D11Buffer *check_window = nullptr;
        UINT check_first = 0u;
        UINT check_count = 0u;

        ctx1->PSGetConstantBuffers1(
            12u,
            1u,
            &check_window,
            &check_first,
            &check_count);

        native_restored =
            native_restored &&
            check_window == state.old_b12.window;

        if (state.old_b12.explicit_window) {
            native_restored =
                native_restored &&
                check_first == state.old_b12.first &&
                check_count == state.old_b12.count;
        }

        if (check_window != nullptr)
            check_window->Release();

        ctx1->Release();
    }

    bool core_restored = true;
    if (state.core_started) {
        core_restored =
            core_.transactions().restore(
                state.command);
    }

    release_transaction(state);

    if (!native_restored ||
        !core_restored) {
        ++restore_fail_;
        quarantined_.store(true);
        return false;
    }

    return true;
}

bool material_response_draw_runtime::replay_draw(
    reshade::api::command_list *cmd_list,
    const operators::material_response::decision &decision,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept
{
    ++eligible_draws_;

    if (!full_material_response_decision(decision))
        return false;

    replacement_record replacement{};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found =
            replacements_.find(decision.receiver_id);

        if (found != replacements_.end() &&
            found->second.shader != nullptr) {
            replacement = found->second;
            replacement.shader->AddRef();
        }
    }

    if (replacement.shader == nullptr) {
        ++replacement_miss_;
        return false;
    }

    ID3D11Buffer *b12 =
        realize_b12(decision);

    if (b12 == nullptr) {
        ++b12_bind_fail_;
        replacement.shader->Release();
        return false;
    }

    native_transaction state{};
    if (!begin_native_transaction(
            cmd_list,
            decision,
            replacement,
            b12,
            state)) {
        b12->Release();
        replacement.shader->Release();
        return false;
    }

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

    const bool restored =
        restore_native_transaction(
            cmd_list,
            state);

    b12->Release();
    replacement.shader->Release();

    if (!restored)
        return false;

    ++replay_ok_;
    return true;
}

bool material_response_draw_runtime::replay_draw_indexed(
    reshade::api::command_list *cmd_list,
    const operators::material_response::decision &decision,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept
{
    ++eligible_draws_;

    if (!full_material_response_decision(decision))
        return false;

    replacement_record replacement{};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found =
            replacements_.find(decision.receiver_id);

        if (found != replacements_.end() &&
            found->second.shader != nullptr) {
            replacement = found->second;
            replacement.shader->AddRef();
        }
    }

    if (replacement.shader == nullptr) {
        ++replacement_miss_;
        return false;
    }

    ID3D11Buffer *b12 =
        realize_b12(decision);

    if (b12 == nullptr) {
        ++b12_bind_fail_;
        replacement.shader->Release();
        return false;
    }

    native_transaction state{};
    if (!begin_native_transaction(
            cmd_list,
            decision,
            replacement,
            b12,
            state)) {
        b12->Release();
        replacement.shader->Release();
        return false;
    }

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

    const bool restored =
        restore_native_transaction(
            cmd_list,
            state);

    b12->Release();
    replacement.shader->Release();

    if (!restored)
        return false;

    ++replay_ok_;
    return true;
}

material_response_draw_telemetry
material_response_draw_runtime::telemetry() const noexcept
{
    return {
        replacement_register_ok_.load(),
        replacement_register_fail_.load(),
        b12_create_.load(),
        b12_hit_.load(),
        b12_bind_fail_.load(),
        eligible_draws_.load(),
        replacement_miss_.load(),
        replay_ok_.load(),
        restore_fail_.load(),
        quarantined_.load()
    };
}

void material_response_draw_runtime::reset() noexcept
{
    release_resources();

    draw_serial_.store(0);
    replacement_register_ok_.store(0);
    replacement_register_fail_.store(0);
    b12_create_.store(0);
    b12_hit_.store(0);
    b12_bind_fail_.store(0);
    eligible_draws_.store(0);
    replacement_miss_.store(0);
    replay_ok_.store(0);
    restore_fail_.store(0);
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
