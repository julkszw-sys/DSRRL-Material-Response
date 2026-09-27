#include "dsrrl/runtime/material_response_draw_transaction.hpp"
#include "dsrrl/operators/material_response/material_response_b12_payload.hpp"
#include "dsrrl/runtime/island_draw_adapter.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/operators/material_response/generated_routes_v1.hpp"
#include "dsrrl/operators/material_response/generated_material_constants_v1.hpp"

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

struct b12_tls_cache_entry {
    const material_response_draw_runtime *runtime = nullptr;
    std::uint32_t route_index = 0u;
    std::uint64_t epoch = 0u;
    ID3D11Buffer *buffer = nullptr;

    ~b12_tls_cache_entry()
    {
        if (buffer != nullptr)
            buffer->Release();
    }

    void clear() noexcept
    {
        if (buffer != nullptr)
            buffer->Release();
        runtime = nullptr;
        route_index = 0u;
        epoch = 0u;
        buffer = nullptr;
    }

    void assign(
        const material_response_draw_runtime *owner,
        std::uint32_t route,
        std::uint64_t resource_epoch,
        ID3D11Buffer *value) noexcept
    {
        if (buffer == value &&
            runtime == owner &&
            route_index == route &&
            epoch == resource_epoch)
            return;

        clear();

        runtime = owner;
        route_index = route;
        epoch = resource_epoch;
        buffer = value;

        if (buffer != nullptr)
            buffer->AddRef();
    }
};

constexpr std::size_t k_b12_tls_cache_slots = 16u;
thread_local std::array<
    b12_tls_cache_entry,
    k_b12_tls_cache_slots>
    g_b12_tls_cache{};

std::size_t b12_tls_cache_index(
    std::uint32_t route_index) noexcept
{
    return static_cast<std::size_t>(
        route_index % k_b12_tls_cache_slots);
}

struct replacement_tls_cache_entry {
    const material_response_draw_runtime *runtime = nullptr;
    std::uint32_t receiver_id = 0u;
    std::uint8_t bank = 0u;
    std::uint64_t epoch = 0u;
    ID3D11PixelShader *shader = nullptr;
    core::operator_mask composed_owners = 0u;
    bool present = false;

    ~replacement_tls_cache_entry()
    {
        if (shader != nullptr)
            shader->Release();
    }

    void clear() noexcept
    {
        if (shader != nullptr)
            shader->Release();
        runtime = nullptr;
        receiver_id = 0u;
        bank = 0u;
        epoch = 0u;
        shader = nullptr;
        composed_owners = 0u;
        present = false;
    }

    void assign(
        const material_response_draw_runtime *owner,
        std::uint32_t receiver,
        std::uint8_t replacement_bank,
        std::uint64_t resource_epoch,
        ID3D11PixelShader *value,
        core::operator_mask owners,
        bool exists) noexcept
    {
        if (runtime == owner &&
            receiver_id == receiver &&
            bank == replacement_bank &&
            epoch == resource_epoch &&
            shader == value &&
            composed_owners == owners &&
            present == exists)
            return;

        clear();
        runtime = owner;
        receiver_id = receiver;
        bank = replacement_bank;
        epoch = resource_epoch;
        shader = value;
        composed_owners = owners;
        present = exists;

        if (shader != nullptr)
            shader->AddRef();
    }
};

constexpr std::size_t k_replacement_tls_cache_slots = 32u;
thread_local std::array<
    replacement_tls_cache_entry,
    k_replacement_tls_cache_slots>
    g_replacement_tls_cache{};

std::size_t replacement_tls_cache_index(
    std::uint32_t receiver_id,
    std::uint8_t bank) noexcept
{
    return static_cast<std::size_t>(
        (receiver_id * 7u + bank) %
        k_replacement_tls_cache_slots);
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
    draw_state_transaction_runtime &transactions) noexcept
    : transactions_(transactions)
{
}

material_response_draw_runtime::~material_response_draw_runtime()
{
    release_resources();
}

bool material_response_draw_runtime::acquire_replacement(
    replacement_bank bank,
    std::uint32_t receiver_id,
    replacement_record &replacement) const noexcept
{
    replacement = {};

    const auto bank_id =
        static_cast<std::uint8_t>(
            bank);
    const auto epoch =
        resource_epoch_.load(
            std::memory_order_acquire);
    auto &cached =
        g_replacement_tls_cache[
            replacement_tls_cache_index(
                receiver_id,
                bank_id)];

    if (cached.runtime == this &&
        cached.receiver_id == receiver_id &&
        cached.bank == bank_id &&
        cached.epoch == epoch) {
        if (!cached.present ||
            cached.shader == nullptr)
            return false;

        cached.shader->AddRef();
        replacement.shader =
            cached.shader;
        replacement.composed_owners =
            cached.composed_owners;
        return true;
    }

    std::lock_guard<std::mutex> lock(
        mutex_);

    const std::unordered_map<
        std::uint32_t,
        replacement_record> *source =
            nullptr;

    switch (bank) {
    case replacement_bank::stable:
        source = &replacements_;
        break;
    case replacement_bank::stable_spec:
        source = &spec_rgb_replacements_;
        break;
    case replacement_bank::lerp:
        source = &lerp_replacements_;
        break;
    case replacement_bank::lerp_spec:
        source = &lerp_spec_rgb_replacements_;
        break;
    case replacement_bank::lerp_upper_lower:
        source = &lerp_upper_lower_replacements_;
        break;
    case replacement_bank::lerp_upper_lower_spec:
        source = &lerp_upper_lower_spec_rgb_replacements_;
        break;
    case replacement_bank::upper_lower:
        source = &upper_lower_replacements_;
        break;
    case replacement_bank::upper_lower_spec:
        source = &upper_lower_spec_rgb_replacements_;
        break;
    }

    if (source == nullptr) {
        cached.assign(
            this,
            receiver_id,
            bank_id,
            resource_epoch_.load(
                std::memory_order_relaxed),
            nullptr,
            0u,
            false);
        return false;
    }

    const auto found =
        source->find(receiver_id);
    if (found == source->end() ||
        found->second.shader == nullptr) {
        cached.assign(
            this,
            receiver_id,
            bank_id,
            resource_epoch_.load(
                std::memory_order_relaxed),
            nullptr,
            0u,
            false);
        return false;
    }

    replacement =
        found->second;
    replacement.shader->AddRef();

    cached.assign(
        this,
        receiver_id,
        bank_id,
        resource_epoch_.load(
            std::memory_order_relaxed),
        replacement.shader,
        replacement.composed_owners,
        true);

    return true;
}

void material_response_draw_runtime::release_resources() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    resource_epoch_.fetch_add(
        1u,
        std::memory_order_release);

    for (auto &entry : replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    replacements_.clear();

    for (auto &entry : spec_rgb_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    spec_rgb_replacements_.clear();

    for (auto &entry : lerp_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    lerp_replacements_.clear();

    for (auto &entry : lerp_spec_rgb_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    lerp_spec_rgb_replacements_.clear();

    for (auto &entry : lerp_upper_lower_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    lerp_upper_lower_replacements_.clear();

    for (auto &entry : lerp_upper_lower_spec_rgb_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    lerp_upper_lower_spec_rgb_replacements_.clear();

    for (auto &entry : upper_lower_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    upper_lower_replacements_.clear();

    for (auto &entry : upper_lower_spec_rgb_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    upper_lower_spec_rgb_replacements_.clear();

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
        local_quarantine_.store(true);
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

    resource_epoch_.fetch_add(
        1u,
        std::memory_order_release);

    for (auto &entry : replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    replacements_.clear();

    for (auto &entry : spec_rgb_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    spec_rgb_replacements_.clear();

    for (auto &entry : lerp_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    lerp_replacements_.clear();

    for (auto &entry : lerp_spec_rgb_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    lerp_spec_rgb_replacements_.clear();

    for (auto &entry : lerp_upper_lower_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    lerp_upper_lower_replacements_.clear();

    for (auto &entry : lerp_upper_lower_spec_rgb_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    lerp_upper_lower_spec_rgb_replacements_.clear();

    for (auto &entry : upper_lower_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    upper_lower_replacements_.clear();

    for (auto &entry : upper_lower_spec_rgb_replacements_) {
        auto *shader = entry.second.shader;
        if (shader != nullptr)
            shader->Release();
    }
    upper_lower_spec_rgb_replacements_.clear();

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
        core::operator_bit(core::operator_id::diffuse_material_domain) |
        core::operator_bit(core::operator_id::spec_rgb);

    if (receiver_id == 0u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        (composed_owners & ~core::all_operator_bits) != 0u ||
        (composed_owners & forbidden_owners) != 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined()) {
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
        replacements_.emplace(
            receiver_id,
            record);
    }

    resource_epoch_.fetch_add(
        1u,
        std::memory_order_release);
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

bool material_response_draw_runtime::register_receiver_spec_rgb_replacement(
    std::uint32_t receiver_id,
    const void *dxbc,
    std::size_t dxbc_size,
    core::operator_mask composed_owners) noexcept
{
    const auto spec_owner =
        core::operator_bit(core::operator_id::spec_rgb);
    const core::operator_mask forbidden_owners =
        core::operator_bit(core::operator_id::material_response) |
        core::operator_bit(core::operator_id::diffuse_material_domain);

    if (receiver_id == 0u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        (composed_owners & spec_owner) == 0u ||
        (composed_owners & ~core::all_operator_bits) != 0u ||
        (composed_owners & forbidden_owners) != 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined()) {
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
            dxbc, dxbc_size, nullptr, &shader)) ||
        shader == nullptr) {
        ++replacement_register_fail_;
        return false;
    }

    const replacement_record record{shader, composed_owners};
    const auto found = spec_rgb_replacements_.find(receiver_id);
    if (found != spec_rgb_replacements_.end()) {
        if (found->second.shader != nullptr)
            found->second.shader->Release();
        found->second = record;
    } else {
        spec_rgb_replacements_.emplace(receiver_id, record);
    }

    resource_epoch_.fetch_add(
        1u,
        std::memory_order_release);
    ++replacement_register_ok_;
    return true;
}

bool material_response_draw_runtime::has_receiver_spec_rgb_replacement(
    std::uint32_t receiver_id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = spec_rgb_replacements_.find(receiver_id);
    return
        found != spec_rgb_replacements_.end() &&
        found->second.shader != nullptr;
}

bool material_response_draw_runtime::register_lerp_receiver_replacement(
    std::uint32_t receiver_id,
    const void *dxbc,
    std::size_t dxbc_size,
    core::operator_mask composed_owners) noexcept
{
    const core::operator_mask forbidden_owners =
        core::operator_bit(core::operator_id::material_response) |
        core::operator_bit(core::operator_id::diffuse_material_domain) |
        core::operator_bit(core::operator_id::upper_lower) |
        core::operator_bit(core::operator_id::spec_rgb);

    if (receiver_id < 24u ||
        receiver_id > 47u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        (composed_owners & ~core::all_operator_bits) != 0u ||
        (composed_owners & forbidden_owners) != 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined()) {
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

    const auto found =
        lerp_replacements_.find(receiver_id);

    if (found != lerp_replacements_.end()) {
        if (found->second.shader != nullptr)
            found->second.shader->Release();
        found->second = record;
    } else {
        lerp_replacements_.emplace(
            receiver_id,
            record);
    }

    resource_epoch_.fetch_add(
        1u,
        std::memory_order_release);
    ++replacement_register_ok_;
    return true;
}

bool material_response_draw_runtime::has_lerp_receiver_replacement(
    std::uint32_t receiver_id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found =
        lerp_replacements_.find(receiver_id);
    return
        found != lerp_replacements_.end() &&
        found->second.shader != nullptr;
}

bool material_response_draw_runtime::
register_lerp_receiver_spec_rgb_replacement(
    std::uint32_t receiver_id,
    const void *dxbc,
    std::size_t dxbc_size,
    core::operator_mask composed_owners) noexcept
{
    const auto spec_owner =
        core::operator_bit(core::operator_id::spec_rgb);
    const core::operator_mask forbidden_owners =
        core::operator_bit(core::operator_id::material_response) |
        core::operator_bit(core::operator_id::diffuse_material_domain) |
        core::operator_bit(core::operator_id::upper_lower);

    if (receiver_id < 24u ||
        receiver_id > 47u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        (composed_owners & spec_owner) == 0u ||
        (composed_owners & ~core::all_operator_bits) != 0u ||
        (composed_owners & forbidden_owners) != 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined()) {
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
            dxbc, dxbc_size, nullptr, &shader)) ||
        shader == nullptr) {
        ++replacement_register_fail_;
        return false;
    }

    const replacement_record record{shader, composed_owners};
    const auto found =
        lerp_spec_rgb_replacements_.find(receiver_id);
    if (found != lerp_spec_rgb_replacements_.end()) {
        if (found->second.shader != nullptr)
            found->second.shader->Release();
        found->second = record;
    } else {
        lerp_spec_rgb_replacements_.emplace(receiver_id, record);
    }

    resource_epoch_.fetch_add(
        1u,
        std::memory_order_release);
    ++replacement_register_ok_;
    return true;
}

bool material_response_draw_runtime::
has_lerp_receiver_spec_rgb_replacement(
    std::uint32_t receiver_id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found =
        lerp_spec_rgb_replacements_.find(receiver_id);
    return
        found != lerp_spec_rgb_replacements_.end() &&
        found->second.shader != nullptr;
}

bool material_response_draw_runtime::
register_lerp_receiver_upper_lower_replacement(
    std::uint32_t receiver_id,
    const void *dxbc,
    std::size_t dxbc_size,
    core::operator_mask composed_owners) noexcept
{
    const core::operator_mask forbidden_owners =
        core::operator_bit(core::operator_id::material_response) |
        core::operator_bit(core::operator_id::diffuse_material_domain) |
        core::operator_bit(core::operator_id::upper_lower) |
        core::operator_bit(core::operator_id::spec_rgb);

    if (receiver_id < 24u ||
        receiver_id > 47u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        (composed_owners & ~core::all_operator_bits) != 0u ||
        (composed_owners & forbidden_owners) != 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined()) {
        ++combined_ul_register_fail_;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ == nullptr) {
        ++combined_ul_register_fail_;
        return false;
    }

    ID3D11PixelShader *shader = nullptr;
    if (FAILED(device_->CreatePixelShader(
            dxbc, dxbc_size, nullptr, &shader)) ||
        shader == nullptr) {
        ++combined_ul_register_fail_;
        return false;
    }

    const replacement_record record{shader, composed_owners};
    const auto found =
        lerp_upper_lower_replacements_.find(receiver_id);
    if (found != lerp_upper_lower_replacements_.end()) {
        if (found->second.shader != nullptr)
            found->second.shader->Release();
        found->second = record;
    } else {
        lerp_upper_lower_replacements_.emplace(
            receiver_id, record);
    }

    resource_epoch_.fetch_add(
        1u,
        std::memory_order_release);
    ++combined_ul_register_ok_;
    return true;
}

bool material_response_draw_runtime::
has_lerp_receiver_upper_lower_replacement(
    std::uint32_t receiver_id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found =
        lerp_upper_lower_replacements_.find(receiver_id);
    return found != lerp_upper_lower_replacements_.end() &&
        found->second.shader != nullptr;
}

bool material_response_draw_runtime::
register_lerp_receiver_upper_lower_spec_rgb_replacement(
    std::uint32_t receiver_id,
    const void *dxbc,
    std::size_t dxbc_size,
    core::operator_mask composed_owners) noexcept
{
    const auto spec_owner =
        core::operator_bit(core::operator_id::spec_rgb);
    const core::operator_mask forbidden_owners =
        core::operator_bit(core::operator_id::material_response) |
        core::operator_bit(core::operator_id::diffuse_material_domain) |
        core::operator_bit(core::operator_id::upper_lower);

    if (receiver_id < 24u ||
        receiver_id > 47u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        (composed_owners & spec_owner) == 0u ||
        (composed_owners & ~core::all_operator_bits) != 0u ||
        (composed_owners & forbidden_owners) != 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined()) {
        ++combined_ul_register_fail_;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ == nullptr) {
        ++combined_ul_register_fail_;
        return false;
    }

    ID3D11PixelShader *shader = nullptr;
    if (FAILED(device_->CreatePixelShader(
            dxbc, dxbc_size, nullptr, &shader)) ||
        shader == nullptr) {
        ++combined_ul_register_fail_;
        return false;
    }

    const replacement_record record{shader, composed_owners};
    const auto found =
        lerp_upper_lower_spec_rgb_replacements_.find(receiver_id);
    if (found != lerp_upper_lower_spec_rgb_replacements_.end()) {
        if (found->second.shader != nullptr)
            found->second.shader->Release();
        found->second = record;
    } else {
        lerp_upper_lower_spec_rgb_replacements_.emplace(
            receiver_id, record);
    }

    resource_epoch_.fetch_add(
        1u,
        std::memory_order_release);
    ++combined_ul_register_ok_;
    return true;
}

bool material_response_draw_runtime::
has_lerp_receiver_upper_lower_spec_rgb_replacement(
    std::uint32_t receiver_id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found =
        lerp_upper_lower_spec_rgb_replacements_.find(receiver_id);
    return found != lerp_upper_lower_spec_rgb_replacements_.end() &&
        found->second.shader != nullptr;
}

bool material_response_draw_runtime::
register_receiver_upper_lower_replacement(
    std::uint32_t receiver_id,
    const void *dxbc,
    std::size_t dxbc_size,
    core::operator_mask composed_owners) noexcept
{
    const core::operator_mask forbidden_owners =
        core::operator_bit(core::operator_id::material_response) |
        core::operator_bit(core::operator_id::diffuse_material_domain) |
        core::operator_bit(core::operator_id::upper_lower) |
        core::operator_bit(core::operator_id::spec_rgb);

    if (receiver_id < 24u ||
        receiver_id > 47u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        (composed_owners & ~core::all_operator_bits) != 0u ||
        (composed_owners & forbidden_owners) != 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined()) {
        ++combined_ul_register_fail_;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ == nullptr) {
        ++combined_ul_register_fail_;
        return false;
    }

    ID3D11PixelShader *shader = nullptr;
    if (FAILED(device_->CreatePixelShader(
            dxbc,
            dxbc_size,
            nullptr,
            &shader)) ||
        shader == nullptr) {
        ++combined_ul_register_fail_;
        return false;
    }

    const replacement_record record{
        shader,
        composed_owners
    };

    const auto found =
        upper_lower_replacements_.find(
            receiver_id);

    if (found !=
        upper_lower_replacements_.end()) {
        if (found->second.shader != nullptr)
            found->second.shader->Release();
        found->second = record;
    } else {
        upper_lower_replacements_.emplace(
            receiver_id,
            record);
    }

    resource_epoch_.fetch_add(
        1u,
        std::memory_order_release);
    ++combined_ul_register_ok_;
    return true;
}

bool material_response_draw_runtime::
has_receiver_upper_lower_replacement(
    std::uint32_t receiver_id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found =
        upper_lower_replacements_.find(
            receiver_id);

    return
        found !=
            upper_lower_replacements_.end() &&
        found->second.shader != nullptr;
}

bool material_response_draw_runtime::
register_receiver_upper_lower_spec_rgb_replacement(
    std::uint32_t receiver_id,
    const void *dxbc,
    std::size_t dxbc_size,
    core::operator_mask composed_owners) noexcept
{
    const auto spec_owner =
        core::operator_bit(core::operator_id::spec_rgb);
    const core::operator_mask forbidden_owners =
        core::operator_bit(core::operator_id::material_response) |
        core::operator_bit(core::operator_id::diffuse_material_domain) |
        core::operator_bit(core::operator_id::upper_lower);

    if (receiver_id < 24u ||
        receiver_id > 47u ||
        dxbc == nullptr ||
        dxbc_size == 0u ||
        (composed_owners & spec_owner) == 0u ||
        (composed_owners & ~core::all_operator_bits) != 0u ||
        (composed_owners & forbidden_owners) != 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined()) {
        ++combined_ul_register_fail_;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ == nullptr) {
        ++combined_ul_register_fail_;
        return false;
    }

    ID3D11PixelShader *shader = nullptr;
    if (FAILED(device_->CreatePixelShader(
            dxbc, dxbc_size, nullptr, &shader)) ||
        shader == nullptr) {
        ++combined_ul_register_fail_;
        return false;
    }

    const replacement_record record{shader, composed_owners};
    const auto found =
        upper_lower_spec_rgb_replacements_.find(receiver_id);
    if (found != upper_lower_spec_rgb_replacements_.end()) {
        if (found->second.shader != nullptr)
            found->second.shader->Release();
        found->second = record;
    } else {
        upper_lower_spec_rgb_replacements_.emplace(receiver_id, record);
    }

    resource_epoch_.fetch_add(
        1u,
        std::memory_order_release);
    ++combined_ul_register_ok_;
    return true;
}

bool material_response_draw_runtime::
has_receiver_upper_lower_spec_rgb_replacement(
    std::uint32_t receiver_id) const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found =
        upper_lower_spec_rgb_replacements_.find(receiver_id);
    return
        found != upper_lower_spec_rgb_replacements_.end() &&
        found->second.shader != nullptr;
}

bool material_response_draw_runtime::prepare_draw_request(
    const operators::material_response::decision &decision,
    prepared_material_response_draw &prepared) noexcept
{
    prepared = {};
    telemetry::hot_count(eligible_draws_);

    if (!full_material_response_decision(decision) ||
        local_quarantine_.load() ||
        transactions_.quarantined())
        return false;

    replacement_record replacement{};
    if (!acquire_replacement(
            replacement_bank::stable,
            decision.receiver_id,
            replacement)) {
        telemetry::hot_count(replacement_miss_);
        return false;
    }

    auto *b12 = realize_b12(decision);
    if (b12 == nullptr) {
        telemetry::hot_count(b12_bind_fail_);
        replacement.shader->Release();
        return false;
    }

    prepared.shader = replacement.shader;
    prepared.b12 = b12;
    prepared.receiver_id = decision.receiver_id;
    prepared.replacement_composed_owners =
        replacement.composed_owners;
    prepared.family =
        material_response_replacement_family::stable;
    prepared.request.primary =
        core::operator_id::material_response;
    prepared.request.additional_owners =
        core::operator_bit(
            core::operator_id::diffuse_material_domain) |
        replacement.composed_owners;
    prepared.request.additional_shader_owners =
        prepared.request.additional_owners;
    prepared.request.receiver_verified = true;
    prepared.request.material_verified = true;
    prepared.request.pixel_shader =
        replacement.shader;
    prepared.request.replace_pixel_shader = true;
    prepared.request.constant_buffers[0] = {
        12u,
        b12,
        core::operator_bit(
            core::operator_id::material_response)
    };
    prepared.request.constant_buffer_count = 1u;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            prepared.request,
            verify) !=
        island_draw_adapter_result::ready) {
        telemetry::hot_count(b12_bind_fail_);
        release_prepared_draw(prepared);
        return false;
    }

    prepared.ready = true;
    return true;
}

bool material_response_draw_runtime::
prepare_draw_request_with_upper_lower(
    const operators::material_response::decision &decision,
    ID3D11Buffer *b13,
    prepared_material_response_draw &prepared) noexcept
{
    prepared = {};
    telemetry::hot_count(eligible_draws_);
    telemetry::hot_count(combined_ul_prepare_);

    if (!full_material_response_decision(decision) ||
        decision.receiver_id < 24u ||
        decision.receiver_id > 47u ||
        b13 == nullptr ||
        local_quarantine_.load() ||
        transactions_.quarantined())
        return false;

    replacement_record replacement{};
    if (!acquire_replacement(
            replacement_bank::upper_lower,
            decision.receiver_id,
            replacement)) {
        telemetry::hot_count(combined_ul_miss_);
        return false;
    }

    auto *b12 = realize_b12(decision);
    if (b12 == nullptr) {
        telemetry::hot_count(b12_bind_fail_);
        replacement.shader->Release();
        return false;
    }

    const auto ul_owner =
        core::operator_bit(
            core::operator_id::upper_lower);

    prepared.shader = replacement.shader;
    prepared.b12 = b12;
    prepared.receiver_id = decision.receiver_id;
    prepared.replacement_composed_owners =
        replacement.composed_owners;
    prepared.family =
        material_response_replacement_family::stable_upper_lower;
    prepared.request.primary =
        core::operator_id::material_response;
    prepared.request.additional_owners =
        core::operator_bit(
            core::operator_id::diffuse_material_domain) |
        ul_owner |
        replacement.composed_owners;
    prepared.request.additional_shader_owners =
        prepared.request.additional_owners;
    prepared.request.additional_constant_buffer_owners =
        ul_owner;
    prepared.request.additional_carrier_owners =
        ul_owner;
    prepared.request.receiver_verified = true;
    prepared.request.material_verified = true;
    prepared.request.pixel_shader =
        replacement.shader;
    prepared.request.replace_pixel_shader = true;
    prepared.request.constant_buffers[0] = {
        12u,
        b12,
        core::operator_bit(
            core::operator_id::material_response)
    };
    prepared.request.constant_buffers[1] = {
        13u,
        b13,
        ul_owner
    };
    prepared.request.constant_buffer_count = 2u;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            prepared.request,
            verify) !=
        island_draw_adapter_result::ready) {
        telemetry::hot_count(b12_bind_fail_);
        release_prepared_draw(prepared);
        return false;
    }

    prepared.ready = true;
    return true;
}


bool material_response_draw_runtime::
prepare_lerp_draw_request(
    const operators::material_response::decision &decision,
    prepared_material_response_draw &prepared) noexcept
{
    prepared = {};
    telemetry::hot_count(eligible_draws_);

    if (!full_material_response_decision(decision) ||
        decision.receiver_id < 24u ||
        decision.receiver_id > 47u ||
        local_quarantine_.load() ||
        transactions_.quarantined())
        return false;

    replacement_record replacement{};
    if (!acquire_replacement(
            replacement_bank::lerp_upper_lower,
            decision.receiver_id,
            replacement)) {
        telemetry::hot_count(replacement_miss_);
        return false;
    }

    auto *b12 = realize_b12(decision);
    if (b12 == nullptr) {
        telemetry::hot_count(b12_bind_fail_);
        replacement.shader->Release();
        return false;
    }

    prepared.shader = replacement.shader;
    prepared.b12 = b12;
    prepared.receiver_id = decision.receiver_id;
    prepared.replacement_composed_owners =
        replacement.composed_owners;
    prepared.family =
        material_response_replacement_family::hemenvlerp;
    prepared.request.primary =
        core::operator_id::material_response;
    prepared.request.additional_owners =
        core::operator_bit(
            core::operator_id::diffuse_material_domain) |
        replacement.composed_owners;
    prepared.request.additional_shader_owners =
        prepared.request.additional_owners;
    prepared.request.receiver_verified = true;
    prepared.request.material_verified = true;
    prepared.request.pixel_shader =
        replacement.shader;
    prepared.request.replace_pixel_shader = true;
    prepared.request.constant_buffers[0] = {
        12u,
        b12,
        core::operator_bit(
            core::operator_id::material_response)
    };
    prepared.request.constant_buffer_count = 1u;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            prepared.request,
            verify) !=
        island_draw_adapter_result::ready) {
        telemetry::hot_count(b12_bind_fail_);
        release_prepared_draw(prepared);
        return false;
    }

    prepared.ready = true;
    return true;
}

bool material_response_draw_runtime::
prepare_lerp_draw_request_with_upper_lower(
    const operators::material_response::decision &decision,
    ID3D11Buffer *b13,
    prepared_material_response_draw &prepared) noexcept
{
    prepared = {};
    telemetry::hot_count(eligible_draws_);
    telemetry::hot_count(combined_ul_prepare_);

    if (!full_material_response_decision(decision) ||
        decision.receiver_id < 24u ||
        decision.receiver_id > 47u ||
        b13 == nullptr ||
        local_quarantine_.load() ||
        transactions_.quarantined())
        return false;

    replacement_record replacement{};
    if (!acquire_replacement(
            replacement_bank::lerp,
            decision.receiver_id,
            replacement)) {
        telemetry::hot_count(combined_ul_miss_);
        return false;
    }

    auto *b12 = realize_b12(decision);
    if (b12 == nullptr) {
        telemetry::hot_count(b12_bind_fail_);
        replacement.shader->Release();
        return false;
    }

    const auto ul_owner =
        core::operator_bit(
            core::operator_id::upper_lower);

    prepared.shader = replacement.shader;
    prepared.b12 = b12;
    prepared.receiver_id = decision.receiver_id;
    prepared.replacement_composed_owners =
        replacement.composed_owners;
    prepared.family =
        material_response_replacement_family::hemenvlerp_upper_lower;
    prepared.request.primary =
        core::operator_id::material_response;
    prepared.request.additional_owners =
        core::operator_bit(
            core::operator_id::diffuse_material_domain) |
        ul_owner |
        replacement.composed_owners;
    prepared.request.additional_shader_owners =
        prepared.request.additional_owners;
    prepared.request.additional_constant_buffer_owners =
        ul_owner;
    prepared.request.additional_carrier_owners =
        ul_owner;
    prepared.request.receiver_verified = true;
    prepared.request.material_verified = true;
    prepared.request.pixel_shader =
        replacement.shader;
    prepared.request.replace_pixel_shader = true;
    prepared.request.constant_buffers[0] = {
        12u,
        b12,
        core::operator_bit(
            core::operator_id::material_response)
    };
    prepared.request.constant_buffers[1] = {
        13u,
        b13,
        ul_owner
    };
    prepared.request.constant_buffer_count = 2u;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            prepared.request,
            verify) !=
        island_draw_adapter_result::ready) {
        telemetry::hot_count(b12_bind_fail_);
        release_prepared_draw(prepared);
        return false;
    }

    prepared.ready = true;
    return true;
}

bool material_response_draw_runtime::has_paired_spec_rgb_replacement(
    const prepared_material_response_draw &prepared) const noexcept
{
    if (!prepared.ready ||
        prepared.receiver_id == 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined())
        return false;

    replacement_bank bank =
        replacement_bank::stable_spec;

    switch (prepared.family) {
    case material_response_replacement_family::stable:
        bank =
            replacement_bank::stable_spec;
        break;
    case material_response_replacement_family::stable_upper_lower:
        bank =
            replacement_bank::upper_lower_spec;
        break;
    case material_response_replacement_family::hemenvlerp_upper_lower:
        bank =
            replacement_bank::lerp_spec;
        break;
    }

    replacement_record replacement{};
    if (!acquire_replacement(
            bank,
            prepared.receiver_id,
            replacement))
        return false;

    const bool match =
        material_response_spec_rgb_pair_owners_match(
            prepared.replacement_composed_owners,
            replacement.composed_owners);

    replacement.shader->Release();
    return match;
}

bool material_response_draw_runtime::promote_prepared_draw_to_spec_rgb(
    prepared_material_response_draw &prepared) noexcept
{
    if (!prepared.ready ||
        prepared.shader == nullptr ||
        prepared.receiver_id == 0u ||
        local_quarantine_.load() ||
        transactions_.quarantined())
        return false;

    if (prepared.spec_rgb_consumer)
        return true;

    const auto spec_owner =
        core::operator_bit(core::operator_id::spec_rgb);

    replacement_bank bank =
        replacement_bank::stable_spec;

    switch (prepared.family) {
    case material_response_replacement_family::stable:
        bank =
            replacement_bank::stable_spec;
        break;
    case material_response_replacement_family::stable_upper_lower:
        bank =
            replacement_bank::upper_lower_spec;
        break;
    case material_response_replacement_family::hemenvlerp_upper_lower:
        bank =
            replacement_bank::lerp_spec;
        break;
    }

    replacement_record replacement{};
    if (!acquire_replacement(
            bank,
            prepared.receiver_id,
            replacement))
        return false;

    const bool pair_matches =
        material_response_spec_rgb_pair_owners_match(
            prepared.replacement_composed_owners,
            replacement.composed_owners);

    if (!pair_matches) {
        replacement.shader->Release();
        return false;
    }

    auto upgraded = prepared.request;
    upgraded.pixel_shader = replacement.shader;
    upgraded.additional_owners |= spec_owner;
    upgraded.additional_shader_owners |= spec_owner;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            upgraded,
            verify) != island_draw_adapter_result::ready) {
        replacement.shader->Release();
        return false;
    }

    auto *old_shader = prepared.shader;
    prepared.shader = replacement.shader;
    prepared.request = upgraded;
    prepared.spec_rgb_consumer = true;

    if (old_shader != nullptr)
        old_shader->Release();

    return true;
}

bool material_response_draw_runtime::prepare_prevalidated_route_request(
    std::uint32_t receiver_id,
    std::uint32_t route_index,
    prepared_material_response_draw &prepared) noexcept
{
    namespace mr = operators::material_response;

    const mr::generated::route_seed *seed = nullptr;
    for (const auto &candidate :
         mr::generated::k_material_routes_v1) {
        if (candidate.route_index != route_index)
            continue;

        const bool receiver_match =
            receiver_id == candidate.receiver0 ||
            receiver_id == candidate.receiver1 ||
            receiver_id == candidate.receiver2;

        if (!receiver_match)
            return false;

        if (seed != nullptr)
            return false;

        seed = &candidate;
    }

    if (seed == nullptr)
        return false;

    const auto *constants =
        mr::generated::find_material_response_constants(
            route_index);
    if (constants == nullptr)
        return false;

    mr::decision decision{};
    decision.active = true;
    decision.reason = mr::decision_reason::active;
    decision.receiver_id = receiver_id;
    decision.route_index = route_index;
    decision.certified_operations =
        mr::diffuse_material_domain_linear |
        mr::specular_factor_c101;
    decision.c101 = seed->c101;
    decision.lod_min = seed->lod_min;
    decision.lod_max = seed->lod_max;
    decision.c100 = constants->c100;
    decision.c101_f0q = constants->c101_f0q;
    decision.ptde_specular_power =
        constants->ptde_specular_power;
    decision.ptde_specular_power_verified =
        constants->ptde_specular_power_verified;

    return prepare_draw_request(
        decision,
        prepared);
}

bool material_response_draw_runtime::
prepare_prevalidated_route_request_with_upper_lower(
    std::uint32_t receiver_id,
    std::uint32_t route_index,
    ID3D11Buffer *b13,
    prepared_material_response_draw &prepared) noexcept
{
    namespace mr = operators::material_response;

    if (b13 == nullptr)
        return false;

    const mr::generated::route_seed *seed = nullptr;
    for (const auto &candidate :
         mr::generated::k_material_routes_v1) {
        if (candidate.route_index != route_index)
            continue;

        const bool receiver_match =
            receiver_id == candidate.receiver0 ||
            receiver_id == candidate.receiver1 ||
            receiver_id == candidate.receiver2;

        if (!receiver_match)
            return false;

        if (seed != nullptr)
            return false;

        seed = &candidate;
    }

    if (seed == nullptr)
        return false;

    const auto *constants =
        mr::generated::find_material_response_constants(
            route_index);
    if (constants == nullptr)
        return false;

    mr::decision decision{};
    decision.active = true;
    decision.reason = mr::decision_reason::active;
    decision.receiver_id = receiver_id;
    decision.route_index = route_index;
    decision.certified_operations =
        mr::diffuse_material_domain_linear |
        mr::specular_factor_c101;
    decision.c101 = seed->c101;
    decision.lod_min = seed->lod_min;
    decision.lod_max = seed->lod_max;
    decision.c100 = constants->c100;
    decision.c101_f0q = constants->c101_f0q;
    decision.ptde_specular_power =
        constants->ptde_specular_power;
    decision.ptde_specular_power_verified =
        constants->ptde_specular_power_verified;

    return prepare_draw_request_with_upper_lower(
        decision,
        b13,
        prepared);
}

void material_response_draw_runtime::release_prepared_draw(
    prepared_material_response_draw &prepared) noexcept
{
    if (prepared.b12 != nullptr)
        prepared.b12->Release();
    if (prepared.shader != nullptr)
        prepared.shader->Release();
    prepared = {};
}

bool material_response_draw_runtime::prepare_b12_carrier(
    const operators::material_response::decision &decision,
    ID3D11Buffer *&b12) noexcept
{
    b12 = realize_b12(decision);
    if (b12 == nullptr) {
        telemetry::hot_count(b12_bind_fail_);
        return false;
    }
    return true;
}

void material_response_draw_runtime::account_dispatch_result(
    draw_tx_result result) noexcept
{
    if (result ==
        draw_tx_result::issued_restored) {
        telemetry::hot_count(replay_ok_);
    } else if (
        result ==
        draw_tx_result::issued_restore_failed) {
        telemetry::hot_count(replay_restore_fail_);
    }
}

ID3D11Buffer *material_response_draw_runtime::realize_b12(
    const operators::material_response::decision &decision) noexcept
{
    if (!full_material_response_decision(decision) ||
        local_quarantine_.load() ||
        transactions_.quarantined())
        return nullptr;

    const auto epoch =
        resource_epoch_.load(
            std::memory_order_acquire);
    auto &cached =
        g_b12_tls_cache[
            b12_tls_cache_index(
                decision.route_index)];

    if (cached.runtime == this &&
        cached.route_index ==
            decision.route_index &&
        cached.epoch == epoch &&
        cached.buffer != nullptr) {
        cached.buffer->AddRef();
        telemetry::hot_count(b12_hit_);
        return cached.buffer;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ == nullptr) {
        if (cached.runtime == this)
            cached.clear();
        return nullptr;
    }

    const auto found =
        b12_by_route_.find(
            decision.route_index);

    if (found != b12_by_route_.end() &&
        found->second != nullptr) {
        auto *buffer =
            found->second;
        buffer->AddRef();

        cached.assign(
            this,
            decision.route_index,
            resource_epoch_.load(
                std::memory_order_relaxed),
            buffer);

        telemetry::hot_count(b12_hit_);
        return buffer;
    }

    const auto payload =
        operators::material_response::
            make_material_response_b12_payload(
                decision);

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth =
        static_cast<UINT>(
            sizeof(payload));
    desc.Usage =
        D3D11_USAGE_IMMUTABLE;
    desc.BindFlags =
        D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem =
        payload.data();

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

    // One reference is owned by b12_by_route_, one by the prepared caller,
    // and the TLS cache retains its own reference so a concurrent device
    // teardown cannot turn a stale cache entry into a dangling COM pointer.
    buffer->AddRef();
    cached.assign(
        this,
        decision.route_index,
        resource_epoch_.load(
            std::memory_order_relaxed),
        buffer);

    ++b12_create_;
    return buffer;
}

bool material_response_draw_runtime::replay_draw(
    reshade::api::command_list *cmd_list,
    const operators::material_response::decision &decision,
    std::uint32_t vertex_count,
    std::uint32_t instance_count,
    std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept
{
    prepared_material_response_draw prepared{};
    if (!prepare_draw_request(
            decision,
            prepared))
        return false;

    const auto dispatch =
        dispatch_island_draw(
            transactions_,
            cmd_list,
            prepared.request,
            vertex_count,
            instance_count,
            first_vertex,
            first_instance);

    release_prepared_draw(prepared);

    if (dispatch.adapter !=
        island_draw_adapter_result::ready)
        return false;

    account_dispatch_result(
        dispatch.transaction);

    return draw_tx_issued(
        dispatch.transaction);
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
    prepared_material_response_draw prepared{};
    if (!prepare_draw_request(
            decision,
            prepared))
        return false;

    const auto dispatch =
        dispatch_island_draw_indexed(
            transactions_,
            cmd_list,
            prepared.request,
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance);

    release_prepared_draw(prepared);

    if (dispatch.adapter !=
        island_draw_adapter_result::ready)
        return false;

    account_dispatch_result(
        dispatch.transaction);

    return draw_tx_issued(
        dispatch.transaction);
}

material_response_draw_telemetry
material_response_draw_runtime::telemetry() const noexcept
{
    return {
        replacement_register_ok_.load(),
        replacement_register_fail_.load(),
        combined_ul_register_ok_.load(),
        combined_ul_register_fail_.load(),
        combined_ul_prepare_.load(),
        combined_ul_miss_.load(),
        b12_create_.load(),
        b12_hit_.load(),
        b12_bind_fail_.load(),
        eligible_draws_.load(),
        replacement_miss_.load(),
        replay_ok_.load(),
        replay_restore_fail_.load(),
        local_quarantine_.load() ||
            transactions_.quarantined()
    };
}

void material_response_draw_runtime::reset() noexcept
{
    release_resources();

    replacement_register_ok_.store(0);
    replacement_register_fail_.store(0);
    combined_ul_register_ok_.store(0);
    combined_ul_register_fail_.store(0);
    combined_ul_prepare_.store(0);
    combined_ul_miss_.store(0);
    b12_create_.store(0);
    b12_hit_.store(0);
    b12_bind_fail_.store(0);
    eligible_draws_.store(0);
    replacement_miss_.store(0);
    replay_ok_.store(0);
    replay_restore_fail_.store(0);
    local_quarantine_.store(false);
}

} // namespace dsrrl::runtime
