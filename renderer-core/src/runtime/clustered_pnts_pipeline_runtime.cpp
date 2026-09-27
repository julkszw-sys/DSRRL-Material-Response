#include "dsrrl/runtime/clustered_pnts_pipeline_runtime.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <d3d11.h>

#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

namespace hashing = operators::legacy_plan::hashing;

struct clustered_pnts_pipeline_runtime::record {
    digest_key host{};
    std::array<std::uint8_t,32> replacement_sha{};
    ID3D11PixelShader *shader = nullptr;
    bool spc = false;
    bool blended_material = false;
    std::uint32_t representative_shader_index = 0u;

    ~record()
    {
        if (shader != nullptr)
            shader->Release();
    }
};

thread_local clustered_pnts_pipeline_runtime::bound_tls_state
    clustered_pnts_pipeline_runtime::bound_tls_{};

clustered_pnts_pipeline_runtime::~clustered_pnts_pipeline_runtime()
{
    reset();
}

std::size_t
clustered_pnts_pipeline_runtime::digest_hash::operator()(
    const digest_key &key) const noexcept
{
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const auto byte : key.sha) {
        h ^= byte;
        h *= 0x100000001b3ULL;
    }
    auto size = key.size;
    for (std::size_t i = 0u; i < sizeof(size); ++i) {
        h ^= static_cast<std::uint8_t>(
            size >> (i * 8u));
        h *= 0x100000001b3ULL;
    }
    return static_cast<std::size_t>(
        h ^ (h >> 32u));
}

const reshade::api::shader_desc *
clustered_pnts_pipeline_runtime::find_pixel_shader(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects) noexcept
{
    if (subobjects == nullptr)
        return nullptr;

    for (std::uint32_t i = 0u; i < subobject_count; ++i) {
        if (subobjects[i].type !=
                reshade::api::pipeline_subobject_type::pixel_shader ||
            subobjects[i].count != 1u ||
            subobjects[i].data == nullptr)
            continue;
        return static_cast<const reshade::api::shader_desc *>(
            subobjects[i].data);
    }
    return nullptr;
}

bool clustered_pnts_pipeline_runtime::register_candidate(
    reshade::api::device *device,
    const operators::point_light::
        clustered_pnts_direct_materialize_outcome &outcome,
    const std::uint8_t *attested_host,
    std::size_t attested_host_size,
    const std::uint8_t *replacement,
    std::size_t replacement_size) noexcept
{
    telemetry::hot_count(candidates_seen_);

    using result =
        operators::point_light::
            clustered_pnts_direct_materialize_result;

    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11 ||
        outcome.result != result::applied ||
        attested_host == nullptr ||
        attested_host_size == 0u ||
        replacement == nullptr ||
        replacement_size == 0u ||
        replacement_size != outcome.replacement_size ||
        hashing::sha256(
            replacement,
            replacement_size) !=
                outcome.replacement_sha256) {
        telemetry::hot_count(candidate_create_fail_);
        return false;
    }

    const digest_key key{
        hashing::sha256(
            attested_host,
            attested_host_size),
        attested_host_size
    };

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (quarantined_.load()) {
            telemetry::hot_count(candidate_create_fail_);
            return false;
        }
        if (device_ != nullptr &&
            device_ != device) {
            quarantined_.store(true);
            telemetry::hot_count(candidate_create_fail_);
            return false;
        }

        const auto found = candidates_.find(key);
        if (found != candidates_.end()) {
            const auto &existing = found->second;
            if (existing == nullptr ||
                existing->replacement_sha !=
                    outcome.replacement_sha256 ||
                existing->spc != outcome.spc ||
                existing->blended_material !=
                    outcome.blended_material ||
                existing->representative_shader_index !=
                    outcome.representative_shader_index) {
                quarantined_.store(true);
                telemetry::hot_count(candidate_create_fail_);
                return false;
            }
            return true;
        }
    }

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());
    if (native == nullptr) {
        telemetry::hot_count(candidate_create_fail_);
        return false;
    }

    ID3D11PixelShader *shader = nullptr;
    if (FAILED(native->CreatePixelShader(
            replacement,
            replacement_size,
            nullptr,
            &shader)) ||
        shader == nullptr) {
        telemetry::hot_count(candidate_create_fail_);
        return false;
    }

    try {
        auto mutable_record =
            std::make_shared<record>();
        mutable_record->host = key;
        mutable_record->replacement_sha =
            outcome.replacement_sha256;
        mutable_record->shader = shader;
        mutable_record->spc = outcome.spc;
        mutable_record->blended_material =
            outcome.blended_material;
        mutable_record->representative_shader_index =
            outcome.representative_shader_index;

        std::shared_ptr<const record> value =
            mutable_record;

        std::lock_guard<std::mutex> lock(mutex_);
        if (quarantined_.load() ||
            (device_ != nullptr &&
             device_ != device)) {
            if (device_ != nullptr &&
                device_ != device)
                quarantined_.store(true);
            telemetry::hot_count(candidate_create_fail_);
            return false;
        }

        const auto [it, inserted] =
            candidates_.emplace(key, value);
        if (!inserted) {
            const auto &existing = it->second;
            if (existing == nullptr ||
                existing->replacement_sha !=
                    outcome.replacement_sha256 ||
                existing->spc != outcome.spc ||
                existing->blended_material !=
                    outcome.blended_material ||
                existing->representative_shader_index !=
                    outcome.representative_shader_index) {
                quarantined_.store(true);
                telemetry::hot_count(candidate_create_fail_);
                return false;
            }
            return true;
        }

        device_ = device;
        telemetry::hot_count(candidate_create_ok_);
        return true;
    } catch (...) {
        shader->Release();
        telemetry::hot_count(candidate_create_fail_);
        return false;
    }
}

void clustered_pnts_pipeline_runtime::on_init_pipeline(
    reshade::api::device *device,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects,
    reshade::api::pipeline pipeline) noexcept
{
    if (device == nullptr ||
        pipeline.handle == 0u ||
        quarantined_.load())
        return;

    const auto *pixel_shader =
        find_pixel_shader(
            subobject_count,
            subobjects);
    if (pixel_shader == nullptr ||
        pixel_shader->code == nullptr ||
        pixel_shader->code_size == 0u)
        return;

    const digest_key key{
        hashing::sha256(
            static_cast<const std::uint8_t *>(
                pixel_shader->code),
            pixel_shader->code_size),
        pixel_shader->code_size
    };

    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ != device)
        return;

    const auto found = candidates_.find(key);
    if (found == candidates_.end()) {
        telemetry::hot_count(init_miss_);
        return;
    }

    pipelines_[pipeline.handle] = found->second;
    telemetry::hot_count(init_attested_);
}

void clustered_pnts_pipeline_runtime::on_bind_pipeline(
    reshade::api::command_list *cmd_list,
    reshade::api::pipeline_stage stages,
    reshade::api::pipeline pipeline) noexcept
{
    if (cmd_list == nullptr)
        return;

    const bool pixel =
        (static_cast<std::uint32_t>(stages) &
         static_cast<std::uint32_t>(
             reshade::api::pipeline_stage::pixel_shader)) != 0u;
    if (!pixel)
        return;

    const auto command =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(
                cmd_list));

    std::lock_guard<std::mutex> lock(mutex_);
    const auto epoch =
        bound_epoch_.load(
            std::memory_order_relaxed);

    if (quarantined_.load()) {
        bound_.erase(command);
        bound_tls_ = {this,command,{},epoch,false};
        return;
    }

    const auto found =
        pipelines_.find(pipeline.handle);
    if (found == pipelines_.end()) {
        bound_.erase(command);
        bound_tls_ = {this,command,{},epoch,false};
        telemetry::hot_count(bind_misses_);
        return;
    }

    bound_[command] = found->second;
    bound_tls_ = {
        this,
        command,
        found->second,
        epoch,
        true
    };
    telemetry::hot_count(bind_hits_);
}

void clustered_pnts_pipeline_runtime::on_destroy_pipeline(
    reshade::api::pipeline pipeline) noexcept
{
    if (pipeline.handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    const auto found =
        pipelines_.find(pipeline.handle);
    if (found == pipelines_.end())
        return;

    const auto dead = found->second;
    pipelines_.erase(found);

    for (auto it = bound_.begin();
         it != bound_.end();) {
        if (it->second == dead)
            it = bound_.erase(it);
        else
            ++it;
    }

    bound_epoch_.fetch_add(
        1u,
        std::memory_order_release);
    bound_tls_ = {};
}

void clustered_pnts_pipeline_runtime::on_destroy_device(
    reshade::api::device *device) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (device_ != device)
        return;

    bound_.clear();
    pipelines_.clear();
    candidates_.clear();
    device_ = nullptr;
    bound_epoch_.fetch_add(
        1u,
        std::memory_order_release);
    bound_tls_ = {};
}

bool clustered_pnts_pipeline_runtime::pipeline_attested(
    std::uint64_t pipeline_handle) const noexcept
{
    if (pipeline_handle == 0u ||
        quarantined_.load())
        return false;

    std::lock_guard<std::mutex> lock(mutex_);
    return pipelines_.find(
        pipeline_handle) != pipelines_.end();
}

bool clustered_pnts_pipeline_runtime::prepare_bound_shader(
    reshade::api::command_list *cmd_list,
    prepared_clustered_pnts_shader &prepared) noexcept
{
    prepared = {};
    if (cmd_list == nullptr ||
        quarantined_.load())
        return false;

    const auto command =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(
                cmd_list));
    const auto epoch =
        bound_epoch_.load(
            std::memory_order_acquire);

    std::shared_ptr<const record> selected{};
    if (bound_tls_.runtime == this &&
        bound_tls_.command == command &&
        bound_tls_.epoch == epoch) {
        if (!bound_tls_.present)
            return false;
        selected = bound_tls_.selected;
    } else {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = bound_.find(command);
        if (found == bound_.end()) {
            bound_tls_ = {
                this,
                command,
                {},
                bound_epoch_.load(
                    std::memory_order_relaxed),
                false
            };
            return false;
        }
        selected = found->second;
        bound_tls_ = {
            this,
            command,
            selected,
            bound_epoch_.load(
                std::memory_order_relaxed),
            selected != nullptr
        };
    }

    if (selected == nullptr ||
        selected->shader == nullptr)
        return false;

    selected->shader->AddRef();
    prepared.shader = selected->shader;
    prepared.spc = selected->spc;
    prepared.blended_material =
        selected->blended_material;
    prepared.representative_shader_index =
        selected->representative_shader_index;
    prepared.ready = true;
    return true;
}

void clustered_pnts_pipeline_runtime::release_prepared_shader(
    prepared_clustered_pnts_shader &prepared) noexcept
{
    if (prepared.shader != nullptr)
        prepared.shader->Release();
    prepared = {};
}

clustered_pnts_pipeline_telemetry
clustered_pnts_pipeline_runtime::telemetry() const noexcept
{
    return {
        candidates_seen_.load(),
        candidate_create_ok_.load(),
        candidate_create_fail_.load(),
        init_attested_.load(),
        init_miss_.load(),
        bind_hits_.load(),
        bind_misses_.load(),
        quarantined_.load()
    };
}

void clustered_pnts_pipeline_runtime::reset() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    bound_.clear();
    pipelines_.clear();
    candidates_.clear();
    device_ = nullptr;
    bound_epoch_.fetch_add(
        1u,
        std::memory_order_release);
    bound_tls_ = {};

    candidates_seen_.store(0u);
    candidate_create_ok_.store(0u);
    candidate_create_fail_.store(0u);
    init_attested_.store(0u);
    init_miss_.store(0u);
    bind_hits_.store(0u);
    bind_misses_.store(0u);
    quarantined_.store(false);
}

} // namespace dsrrl::runtime
