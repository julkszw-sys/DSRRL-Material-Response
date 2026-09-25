#include "dsrrl/runtime/material_response_draw_transaction.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>

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

} // namespace

material_response_draw_runtime::material_response_draw_runtime(
    core::renderer_core &core) noexcept
    : core_(core)
{
}

material_response_draw_runtime::~material_response_draw_runtime()
{
    release_replacements();
}

void material_response_draw_runtime::release_replacements() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &[_, shader] : replacements_) {
        if (shader != nullptr)
            shader->Release();
    }
    replacements_.clear();

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

    auto *native = reinterpret_cast<ID3D11Device *>(device->get_native());
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

    auto *native = reinterpret_cast<ID3D11Device *>(device->get_native());

    std::lock_guard<std::mutex> lock(mutex_);
    if (native != device_)
        return;

    for (auto &[_, shader] : replacements_) {
        if (shader != nullptr)
            shader->Release();
    }
    replacements_.clear();

    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }
}

bool material_response_draw_runtime::register_receiver_replacement(
    std::uint32_t receiver_id,
    const void *dxbc,
    std::size_t dxbc_size) noexcept
{
    if (receiver_id == 0u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
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

    const auto found = replacements_.find(receiver_id);
    if (found != replacements_.end()) {
        if (found->second != nullptr)
            found->second->Release();
        found->second = shader;
    } else {
        replacements_.emplace(receiver_id, shader);
    }

    ++replacement_register_ok_;
    return true;
}

bool material_response_draw_runtime::has_receiver_replacement(
    std::uint32_t receiver_id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = replacements_.find(receiver_id);
    return found != replacements_.end() && found->second != nullptr;
}

bool material_response_draw_runtime::accepts_command(
    reshade::api::command_list *cmd_list,
    ID3D11PixelShader *&replacement) noexcept
{
    replacement = nullptr;

    if (cmd_list == nullptr ||
        quarantined_.load())
        return false;

    auto *ctx = reinterpret_cast<ID3D11DeviceContext *>(
        cmd_list->get_native());
    if (ctx == nullptr)
        return false;

    return true;
}

bool material_response_draw_runtime::begin_native_transaction(
    reshade::api::command_list *cmd_list,
    std::uint32_t receiver_id,
    ID3D11PixelShader *replacement,
    ID3D11PixelShader *&old_shader) noexcept
{
    old_shader = nullptr;
    if (cmd_list == nullptr || replacement == nullptr)
        return false;

    auto *ctx = reinterpret_cast<ID3D11DeviceContext *>(
        cmd_list->get_native());
    if (ctx == nullptr)
        return false;

    std::array<ID3D11ClassInstance *, 256> classes{};
    UINT class_count = static_cast<UINT>(classes.size());
    ctx->PSGetShader(
        &old_shader,
        classes.data(),
        &class_count);

    if (class_count != 0u) {
        release_classes(classes);
        if (old_shader != nullptr) {
            old_shader->Release();
            old_shader = nullptr;
        }
        return false;
    }
    release_classes(classes);

    if (old_shader == nullptr)
        return false;

    core::render_patch_plan plan{};
    plan.patches[0] = {
        core::operator_id::material_response,
        0u,
        true,
        false
    };
    plan.patch_count = 1u;
    plan.carrier_write_mask = 0u;

    const auto command = command_key(cmd_list);
    const auto serial = ++draw_serial_;

    if (!core_.transactions().begin(
            command,
            serial,
            context_kind_of(ctx),
            plan)) {
        old_shader->Release();
        old_shader = nullptr;
        return false;
    }

    ctx->PSSetShader(replacement, nullptr, 0u);

    ID3D11PixelShader *check = nullptr;
    UINT check_count = 0u;
    ctx->PSGetShader(&check, nullptr, &check_count);
    const bool bound = check == replacement && check_count == 0u;
    if (check != nullptr)
        check->Release();

    if (!bound) {
        ctx->PSSetShader(old_shader, nullptr, 0u);
        (void)core_.transactions().restore(command);
        old_shader->Release();
        old_shader = nullptr;
        return false;
    }

    (void)receiver_id;
    return true;
}

bool material_response_draw_runtime::restore_native_transaction(
    reshade::api::command_list *cmd_list,
    ID3D11PixelShader *old_shader) noexcept
{
    if (cmd_list == nullptr || old_shader == nullptr)
        return false;

    auto *ctx = reinterpret_cast<ID3D11DeviceContext *>(
        cmd_list->get_native());
    if (ctx == nullptr)
        return false;

    ctx->PSSetShader(old_shader, nullptr, 0u);

    ID3D11PixelShader *check = nullptr;
    UINT check_count = 0u;
    ctx->PSGetShader(&check, nullptr, &check_count);
    const bool native_restored =
        check == old_shader && check_count == 0u;
    if (check != nullptr)
        check->Release();

    const bool core_restored =
        core_.transactions().restore(command_key(cmd_list));

    old_shader->Release();

    if (!native_restored || !core_restored) {
        ++restore_fail_;
        quarantined_.store(true);
        return false;
    }

    return true;
}

bool material_response_draw_runtime::replay_draw(
    reshade::api::command_list *cmd_list,
    std::uint32_t receiver_id,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept
{
    ++eligible_draws_;

    ID3D11PixelShader *replacement = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = replacements_.find(receiver_id);
        if (found != replacements_.end() && found->second != nullptr) {
            replacement = found->second;
            replacement->AddRef();
        }
    }

    if (replacement == nullptr) {
        ++replacement_miss_;
        return false;
    }

    ID3D11PixelShader *unused = nullptr;
    if (!accepts_command(cmd_list, unused)) {
        replacement->Release();
        return false;
    }

    ID3D11PixelShader *old_shader = nullptr;
    if (!begin_native_transaction(
            cmd_list,
            receiver_id,
            replacement,
            old_shader)) {
        replacement->Release();
        return false;
    }

    auto *ctx = reinterpret_cast<ID3D11DeviceContext *>(
        cmd_list->get_native());

    if (instance_count == 1u && first_instance == 0u)
        ctx->Draw(vertex_count, first_vertex);
    else
        ctx->DrawInstanced(
            vertex_count,
            instance_count,
            first_vertex,
            first_instance);

    const bool restored =
        restore_native_transaction(cmd_list, old_shader);

    replacement->Release();

    if (!restored)
        return false;

    ++replay_ok_;
    return true;
}

bool material_response_draw_runtime::replay_draw_indexed(
    reshade::api::command_list *cmd_list,
    std::uint32_t receiver_id,
    std::uint32_t index_count,
    std::uint32_t instance_count,
    std::uint32_t first_index,
    std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept
{
    ++eligible_draws_;

    ID3D11PixelShader *replacement = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = replacements_.find(receiver_id);
        if (found != replacements_.end() && found->second != nullptr) {
            replacement = found->second;
            replacement->AddRef();
        }
    }

    if (replacement == nullptr) {
        ++replacement_miss_;
        return false;
    }

    ID3D11PixelShader *unused = nullptr;
    if (!accepts_command(cmd_list, unused)) {
        replacement->Release();
        return false;
    }

    ID3D11PixelShader *old_shader = nullptr;
    if (!begin_native_transaction(
            cmd_list,
            receiver_id,
            replacement,
            old_shader)) {
        replacement->Release();
        return false;
    }

    auto *ctx = reinterpret_cast<ID3D11DeviceContext *>(
        cmd_list->get_native());

    if (instance_count == 1u && first_instance == 0u)
        ctx->DrawIndexed(
            index_count,
            first_index,
            vertex_offset);
    else
        ctx->DrawIndexedInstanced(
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance);

    const bool restored =
        restore_native_transaction(cmd_list, old_shader);

    replacement->Release();

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
        eligible_draws_.load(),
        replacement_miss_.load(),
        replay_ok_.load(),
        restore_fail_.load(),
        quarantined_.load()
    };
}

void material_response_draw_runtime::reset() noexcept
{
    release_replacements();
    draw_serial_.store(0);
    replacement_register_ok_.store(0);
    replacement_register_fail_.store(0);
    eligible_draws_.store(0);
    replacement_miss_.store(0);
    replay_ok_.store(0);
    restore_fail_.store(0);
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
