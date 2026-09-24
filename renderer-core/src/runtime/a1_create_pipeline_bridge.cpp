#include "dsrrl/runtime/a1_create_pipeline_bridge.hpp"

#include <cstring>
#include <utility>

namespace dsrrl::runtime {

using operators::legacy_plan::a1_create_time_result;
using operators::legacy_plan::generated::
    a1_exact_patch_plan;

a1_create_pipeline_bridge::a1_create_pipeline_bridge(
    core::feature_registry &features) noexcept
    : features_(features)
{
    for (auto &seen : first_bind_seen_)
        seen.store(false);
}

reshade::api::shader_desc *
a1_create_pipeline_bridge::find_mutable_pixel_shader(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *
        subobjects) noexcept
{
    if (subobjects == nullptr)
        return nullptr;

    for (std::uint32_t i = 0;
         i < subobject_count;
         ++i) {
        if (subobjects[i].type !=
                reshade::api::
                    pipeline_subobject_type::
                        pixel_shader ||
            subobjects[i].count != 1u ||
            subobjects[i].data == nullptr)
            continue;

        return const_cast<
            reshade::api::shader_desc *>(
                static_cast<
                    const reshade::api::shader_desc *>(
                        subobjects[i].data));
    }

    return nullptr;
}

const reshade::api::shader_desc *
a1_create_pipeline_bridge::find_pixel_shader(
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *
        subobjects) noexcept
{
    return find_mutable_pixel_shader(
        subobject_count,
        subobjects);
}

std::uint16_t
a1_create_pipeline_bridge::plan_index_of(
    const a1_exact_patch_plan *plan) noexcept
{
    if (plan == nullptr)
        return 0xFFFFu;

    const auto *begin =
        operators::legacy_plan::generated::
            k_a1_exact_patch_plans_v1;

    const auto *end =
        begin +
        operators::legacy_plan::generated::
            k_a1_exact_patch_plan_count_v1;

    if (plan < begin || plan >= end)
        return 0xFFFFu;

    return static_cast<std::uint16_t>(
        plan - begin);
}

bool a1_create_pipeline_bridge::accepts_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11 ||
        quarantined_.load())
        return false;

    std::lock_guard<std::mutex> lock(mutex_);
    return device_ == nullptr || device_ == device;
}

void a1_create_pipeline_bridge::on_init_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11)
        return;

    std::lock_guard<std::mutex> lock(mutex_);

    if (device_ == nullptr) {
        device_ = device;
        return;
    }

    if (device_ != device)
        quarantined_.store(true);
}

void a1_create_pipeline_bridge::on_destroy_device(
    reshade::api::device *device) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (device != device_)
        return;

    pipeline_records_.clear();
    code_records_.clear();
    cache_.clear();
    device_ = nullptr;
}

std::shared_ptr<
    const a1_create_pipeline_bridge::
        replacement_record>
a1_create_pipeline_bridge::cache_replacement_record(
    std::uint16_t plan_index,
    core::operator_mask selected_owners,
    std::uint16_t selected_ops,
    bool full_plan_materialized,
    const operators::legacy_plan::hashing::
        sha256_digest &output_sha256,
    std::vector<std::uint8_t> replacement)
{
    if (plan_index == 0xFFFFu ||
        plan_index >= first_bind_seen_.size() ||
        replacement.empty() ||
        selected_owners == 0u ||
        selected_ops == 0u)
        return {};

    const cache_key key{
        plan_index,
        selected_owners
    };

    std::lock_guard<std::mutex> lock(mutex_);

    const auto existing = cache_.find(key);

    if (existing != cache_.end()) {
        const auto &record = existing->second;

        if (record == nullptr ||
            record->bytes == nullptr ||
            record->bytes->size() !=
                replacement.size() ||
            record->output_sha256 !=
                output_sha256) {
            quarantined_.store(true);
            return {};
        }

        return record;
    }

    auto mutable_bytes =
        std::make_shared<
            std::vector<std::uint8_t>>(
                std::move(replacement));

    auto mutable_record =
        std::make_shared<
            replacement_record>();

    mutable_record->bytes = mutable_bytes;
    mutable_record->output_sha256 =
        output_sha256;
    mutable_record->plan_index = plan_index;
    mutable_record->selected_ops =
        selected_ops;
    mutable_record->selected_owners =
        selected_owners;
    mutable_record->full_plan_materialized =
        full_plan_materialized;

    const std::shared_ptr<
        const replacement_record> record =
            mutable_record;

    cache_.emplace(key, record);
    code_records_.emplace(
        record->bytes->data(),
        record);

    return record;
}

std::shared_ptr<
    const a1_create_pipeline_bridge::
        replacement_record>
a1_create_pipeline_bridge::cache_replacement(
    const operators::legacy_plan::
        a1_create_time_outcome &outcome,
    std::vector<std::uint8_t> replacement)
{
    const std::uint16_t plan_index =
        plan_index_of(outcome.plan);

    return cache_replacement_record(
        plan_index,
        outcome.selected_owners,
        outcome.selected_ops,
        outcome.full_plan_materialized,
        outcome.output_sha256,
        std::move(replacement));
}

bool a1_create_pipeline_bridge::on_create_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *
        subobjects) noexcept
{
    ++create_events_;

    if (!accepts_device(device))
        return false;

    auto *pixel_shader =
        find_mutable_pixel_shader(
            subobject_count,
            subobjects);

    if (pixel_shader == nullptr ||
        pixel_shader->code == nullptr ||
        pixel_shader->code_size == 0u)
        return false;

    const bool historical_candidate =
        operators::legacy_plan::
            a1_candidate_code_size(
                pixel_shader->code_size);

    const bool build151_candidate =
        operators::legacy_plan::build151::
            candidate_code_size(
                pixel_shader->code_size);

    if (!historical_candidate &&
        !build151_candidate)
        return false;

    ++candidate_size_hits_;

    try {
        const auto *source =
            static_cast<const std::uint8_t *>(
                pixel_shader->code);

        std::vector<std::uint8_t> replacement;

        if (historical_candidate) {
            const auto outcome =
                operators::legacy_plan::
                    materialize_a1_create_time(
                        features_,
                        source,
                        pixel_shader->code_size,
                        replacement);

            if (outcome.plan != nullptr)
                ++exact_identity_hits_;

            switch (outcome.result) {
            case a1_create_time_result::applied: {
                auto record =
                    cache_replacement(
                        outcome,
                        std::move(replacement));

                if (record == nullptr ||
                    record->bytes == nullptr ||
                    record->bytes->empty()) {
                    ++fail_open_;
                    return false;
                }

                pixel_shader->code =
                    record->bytes->data();
                pixel_shader->code_size =
                    record->bytes->size();

                ++materialized_;
                return true;
            }

            case a1_create_time_result::
                    pass_through_no_enabled_owner:
                ++pass_no_enabled_owner_;
                return false;

            case a1_create_time_result::
                    pass_through_unknown_exact_sha:
            case a1_create_time_result::
                    pass_through_not_candidate_size:
                break;

            default:
                ++fail_open_;
                return false;
            }
        }

        replacement.clear();

        const auto ext =
            operators::legacy_plan::build151::
                materialize(
                    features_,
                    source,
                    pixel_shader->code_size,
                    replacement);

        using ext_result =
            operators::legacy_plan::build151::
                nospc_result;

        if (ext.plan_index != 0xFFFFu) {
            ++exact_identity_hits_;
            ++build151_nospc_exact_hits_;
        }

        switch (ext.result) {
        case ext_result::applied:
            break;

        case ext_result::
                pass_through_no_enabled_owner:
            ++pass_no_enabled_owner_;
            return false;

        case ext_result::
                pass_through_unknown_exact_sha:
        case ext_result::
                pass_through_not_candidate_size:
            ++pass_unknown_identity_;
            return false;

        default:
            ++fail_open_;
            return false;
        }

        auto record =
            cache_replacement_record(
                ext.plan_index,
                core::operator_bit(
                    core::operator_id::
                        envspec_nospc_delete),
                1u,
                true,
                ext.output_sha256,
                std::move(replacement));

        if (record == nullptr ||
            record->bytes == nullptr ||
            record->bytes->empty()) {
            ++fail_open_;
            return false;
        }

        pixel_shader->code =
            record->bytes->data();
        pixel_shader->code_size =
            record->bytes->size();

        ++materialized_;
        ++build151_nospc_materialized_;
        return true;
    }
    catch (...) {
        ++fail_open_;
        return false;
    }
}

void a1_create_pipeline_bridge::on_init_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *
        subobjects,
    reshade::api::pipeline pipeline) noexcept
{
    try {
        if (!accepts_device(device) ||
            pipeline.handle == 0u)
            return;

        const auto *pixel_shader =
            find_pixel_shader(
                subobject_count,
                subobjects);

        if (pixel_shader == nullptr ||
            pixel_shader->code == nullptr ||
            pixel_shader->code_size == 0u)
            return;

        std::shared_ptr<
            const replacement_record> record;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            const auto found =
                code_records_.find(
                    pixel_shader->code);

            if (found == code_records_.end())
                return;

            record = found->second;
        }

        if (record == nullptr ||
            record->bytes == nullptr ||
            record->bytes->size() !=
                pixel_shader->code_size) {
            ++init_mismatch_;
            quarantined_.store(true);
            return;
        }

        const auto digest =
            operators::legacy_plan::hashing::
                sha256(
                    static_cast<
                        const std::uint8_t *>(
                            pixel_shader->code),
                    pixel_shader->code_size);

        if (digest != record->output_sha256) {
            ++init_mismatch_;
            quarantined_.store(true);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            pipeline_records_[pipeline.handle] =
                record;
        }

        ++init_attested_;
    } catch (...) {
        ++fail_open_;
        quarantined_.store(true);
    }
}

void a1_create_pipeline_bridge::on_destroy_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline pipeline) noexcept
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11 ||
        pipeline.handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    pipeline_records_.erase(pipeline.handle);
}

bool a1_create_pipeline_bridge::on_bind_pipeline(
    reshade::api::pipeline_stage stages,
    reshade::api::pipeline pipeline,
    std::uint16_t *
        first_bind_plan_index) noexcept
{
    if ((static_cast<std::uint32_t>(stages) &
         static_cast<std::uint32_t>(
             reshade::api::pipeline_stage::
                pixel_shader)) == 0u ||
        pipeline.handle == 0u)
        return false;

    std::shared_ptr<
        const replacement_record> record;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto found =
            pipeline_records_.find(
                pipeline.handle);

        if (found == pipeline_records_.end())
            return false;

        record = found->second;
    }

    if (record == nullptr ||
        record->plan_index >=
            first_bind_seen_.size())
        return false;

    ++target_binds_;

    if (record->plan_index >=
        operators::legacy_plan::build151::
            k_plan_index_base)
        ++build151_nospc_binds_;

    const bool first =
        !first_bind_seen_[
            record->plan_index].exchange(true);

    if (first &&
        first_bind_plan_index != nullptr)
        *first_bind_plan_index =
            record->plan_index;

    return true;
}

a1_runtime_telemetry
a1_create_pipeline_bridge::telemetry() const noexcept
{
    return {
        create_events_.load(),
        candidate_size_hits_.load(),
        exact_identity_hits_.load(),
        materialized_.load(),
        pass_unknown_identity_.load(),
        pass_no_enabled_owner_.load(),
        fail_open_.load(),
        init_attested_.load(),
        init_mismatch_.load(),
        target_binds_.load(),
        build151_nospc_exact_hits_.load(),
        build151_nospc_materialized_.load(),
        build151_nospc_binds_.load(),
        quarantined_.load()
    };
}

void a1_create_pipeline_bridge::reset() noexcept
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pipeline_records_.clear();
        code_records_.clear();
        cache_.clear();
        device_ = nullptr;
    }

    for (auto &seen : first_bind_seen_)
        seen.store(false);

    quarantined_.store(false);
    create_events_.store(0);
    candidate_size_hits_.store(0);
    exact_identity_hits_.store(0);
    materialized_.store(0);
    pass_unknown_identity_.store(0);
    pass_no_enabled_owner_.store(0);
    fail_open_.store(0);
    init_attested_.store(0);
    init_mismatch_.store(0);
    target_binds_.store(0);
    build151_nospc_exact_hits_.store(0);
    build151_nospc_materialized_.store(0);
    build151_nospc_binds_.store(0);
}

} // namespace dsrrl::runtime
