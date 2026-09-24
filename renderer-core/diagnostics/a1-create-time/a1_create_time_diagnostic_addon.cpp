#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL A1 create-time diagnostic requires ReShade Add-on API 20
#endif

#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace reshade::api;

namespace {

using dsrrl::core::operator_bit;
using dsrrl::core::operator_id;
using dsrrl::core::operator_mask;
using dsrrl::operators::legacy_plan::a1_create_time_outcome;
using dsrrl::operators::legacy_plan::a1_create_time_result;
using dsrrl::operators::legacy_plan::generated::a1_exact_patch_plan;
using dsrrl::operators::legacy_plan::hashing::sha256_digest;

constexpr operator_mask k_diagnostic_closed_mask =
    operator_bit(operator_id::terminal_sat_rgb) |
    operator_bit(operator_id::diffuse_material_domain) |
    operator_bit(operator_id::pointlight_pnts_attenuation) |
    operator_bit(operator_id::envspec_nospc_delete) |
    operator_bit(operator_id::fixed_postfog_identity);

static_assert(
    k_diagnostic_closed_mask ==
        dsrrl::operators::legacy_plan::a1_create_time_supported_operators,
    "Diagnostic enable mask must remain exactly the certified A1 create-time island set.");

struct cache_key {
    std::uint16_t plan_index = 0;
    operator_mask owners = 0;

    bool operator==(const cache_key &other) const noexcept
    {
        return
            plan_index == other.plan_index &&
            owners == other.owners;
    }
};

struct cache_key_hash {
    std::size_t operator()(const cache_key &key) const noexcept
    {
        const std::uint64_t packed =
            static_cast<std::uint64_t>(key.plan_index) |
            (static_cast<std::uint64_t>(key.owners) << 16u);
        return static_cast<std::size_t>(
            packed ^ (packed >> 33u));
    }
};

struct replacement_record {
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
    sha256_digest output_sha256{};
    std::uint16_t plan_index = 0;
    std::uint16_t selected_ops = 0;
    operator_mask selected_owners = 0;
    bool full_plan_materialized = false;
};

dsrrl::core::feature_registry g_features;

std::mutex g_state_mutex;
std::unordered_map<
    cache_key,
    std::shared_ptr<const replacement_record>,
    cache_key_hash> g_cache;
std::unordered_map<
    const void *,
    std::shared_ptr<const replacement_record>> g_code_records;
std::unordered_map<
    std::uint64_t,
    std::shared_ptr<const replacement_record>> g_pipeline_records;

device *g_device = nullptr;
std::atomic<bool> g_quarantined{false};

std::atomic<std::uint64_t> g_create_events{0};
std::atomic<std::uint64_t> g_candidate_size{0};
std::atomic<std::uint64_t> g_exact_hits{0};
std::atomic<std::uint64_t> g_materialized{0};
std::atomic<std::uint64_t> g_pass_unknown{0};
std::atomic<std::uint64_t> g_pass_no_owner{0};
std::atomic<std::uint64_t> g_fail_open{0};
std::atomic<std::uint64_t> g_init_attested{0};
std::atomic<std::uint64_t> g_init_mismatch{0};
std::atomic<std::uint64_t> g_target_binds{0};
std::atomic<std::uint64_t> g_present_count{0};

std::array<std::atomic_bool, 144> g_first_bind_logged{};

void log_message(
    reshade::log::level level,
    const char *text) noexcept
{
    reshade::log::message(level, text);
}

void log_state(const char *tag) noexcept
{
    char line[512]{};

    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL A1 CREATE] %s create=%llu candidate=%llu exact=%llu "
        "materialized=%llu unknown=%llu no_owner=%llu failopen=%llu "
        "init_ok=%llu init_bad=%llu binds=%llu quarantine=%u",
        tag,
        static_cast<unsigned long long>(g_create_events.load()),
        static_cast<unsigned long long>(g_candidate_size.load()),
        static_cast<unsigned long long>(g_exact_hits.load()),
        static_cast<unsigned long long>(g_materialized.load()),
        static_cast<unsigned long long>(g_pass_unknown.load()),
        static_cast<unsigned long long>(g_pass_no_owner.load()),
        static_cast<unsigned long long>(g_fail_open.load()),
        static_cast<unsigned long long>(g_init_attested.load()),
        static_cast<unsigned long long>(g_init_mismatch.load()),
        static_cast<unsigned long long>(g_target_binds.load()),
        g_quarantined.load() ? 1u : 0u);

    log_message(reshade::log::level::info, line);
}

shader_desc *find_mutable_pixel_shader(
    std::uint32_t count,
    const pipeline_subobject *subobjects) noexcept
{
    if (subobjects == nullptr)
        return nullptr;

    for (std::uint32_t i = 0; i < count; ++i) {
        if (subobjects[i].type !=
                pipeline_subobject_type::pixel_shader ||
            subobjects[i].count != 1u ||
            subobjects[i].data == nullptr)
            continue;

        return const_cast<shader_desc *>(
            static_cast<const shader_desc *>(
                subobjects[i].data));
    }

    return nullptr;
}

const shader_desc *find_pixel_shader(
    std::uint32_t count,
    const pipeline_subobject *subobjects) noexcept
{
    return find_mutable_pixel_shader(count, subobjects);
}

bool enable_diagnostic_islands() noexcept
{
    return
        g_features.set(operator_id::terminal_sat_rgb, true) &&
        g_features.set(operator_id::diffuse_material_domain, true) &&
        g_features.set(operator_id::pointlight_pnts_attenuation, true) &&
        g_features.set(operator_id::envspec_nospc_delete, true) &&
        g_features.set(operator_id::fixed_postfog_identity, true);
}

std::uint16_t plan_index_of(
    const a1_exact_patch_plan *plan) noexcept
{
    if (plan == nullptr)
        return 0xFFFFu;

    const auto *begin =
        dsrrl::operators::legacy_plan::generated::
            k_a1_exact_patch_plans_v1;
    const auto *end =
        begin +
        dsrrl::operators::legacy_plan::generated::
            k_a1_exact_patch_plan_count_v1;

    if (plan < begin || plan >= end)
        return 0xFFFFu;

    return static_cast<std::uint16_t>(plan - begin);
}

bool digest_equal(
    const sha256_digest &a,
    const sha256_digest &b) noexcept
{
    return a == b;
}

std::shared_ptr<const replacement_record> cache_replacement(
    const a1_create_time_outcome &outcome,
    std::vector<std::uint8_t> replacement)
{
    const std::uint16_t plan_index =
        plan_index_of(outcome.plan);

    if (plan_index == 0xFFFFu ||
        replacement.empty() ||
        outcome.selected_owners == 0u ||
        outcome.selected_ops == 0u)
        return {};

    const cache_key key{
        plan_index,
        outcome.selected_owners
    };

    std::lock_guard<std::mutex> lock(g_state_mutex);

    const auto existing = g_cache.find(key);
    if (existing != g_cache.end()) {
        const auto &record = existing->second;

        if (record == nullptr ||
            record->bytes == nullptr ||
            record->bytes->size() != replacement.size() ||
            !digest_equal(
                record->output_sha256,
                outcome.output_sha256)) {
            g_quarantined.store(true);
            return {};
        }

        return record;
    }

    auto mutable_bytes =
        std::make_shared<std::vector<std::uint8_t>>(
            std::move(replacement));

    auto record =
        std::make_shared<replacement_record>();

    record->bytes = mutable_bytes;
    record->output_sha256 = outcome.output_sha256;
    record->plan_index = plan_index;
    record->selected_ops = outcome.selected_ops;
    record->selected_owners = outcome.selected_owners;
    record->full_plan_materialized =
        outcome.full_plan_materialized;

    const std::shared_ptr<const replacement_record>
        stable_record = record;

    g_cache.emplace(key, stable_record);
    g_code_records.emplace(
        stable_record->bytes->data(),
        stable_record);

    return stable_record;
}

void on_init_device(device *d)
{
    if (d == nullptr ||
        d->get_api() != device_api::d3d11)
        return;

    std::lock_guard<std::mutex> lock(g_state_mutex);

    if (g_device == nullptr) {
        g_device = d;
        return;
    }

    if (g_device != d) {
        g_quarantined.store(true);
        log_message(
            reshade::log::level::error,
            "[DSRRL A1 CREATE] multiple D3D11 devices observed; quarantined.");
    }
}

void on_destroy_device(device *d)
{
    std::lock_guard<std::mutex> lock(g_state_mutex);

    if (d != g_device)
        return;

    g_pipeline_records.clear();
    g_code_records.clear();
    g_cache.clear();
    g_device = nullptr;
}

bool accepts_device(device *d) noexcept
{
    if (d == nullptr ||
        d->get_api() != device_api::d3d11 ||
        g_quarantined.load())
        return false;

    std::lock_guard<std::mutex> lock(g_state_mutex);
    return g_device == nullptr || g_device == d;
}

bool on_create_pipeline(
    device *d,
    pipeline_layout,
    std::uint32_t count,
    const pipeline_subobject *subobjects)
{
    ++g_create_events;

    if (!accepts_device(d))
        return false;

    auto *ps =
        find_mutable_pixel_shader(count, subobjects);

    if (ps == nullptr ||
        ps->code == nullptr ||
        ps->code_size == 0u)
        return false;

    if (!dsrrl::operators::legacy_plan::
            a1_candidate_code_size(ps->code_size))
        return false;

    ++g_candidate_size;

    try {
        std::vector<std::uint8_t> replacement;

        const auto outcome =
            dsrrl::operators::legacy_plan::
                materialize_a1_create_time(
                    g_features,
                    static_cast<const std::uint8_t *>(
                        ps->code),
                    ps->code_size,
                    replacement);

        if (outcome.plan != nullptr)
            ++g_exact_hits;

        switch (outcome.result) {
        case a1_create_time_result::applied:
            break;

        case a1_create_time_result::
                pass_through_unknown_exact_sha:
            ++g_pass_unknown;
            return false;

        case a1_create_time_result::
                pass_through_no_enabled_owner:
            ++g_pass_no_owner;
            return false;

        case a1_create_time_result::
                pass_through_not_candidate_size:
            return false;

        default:
            ++g_fail_open;
            return false;
        }

        auto record =
            cache_replacement(
                outcome,
                std::move(replacement));

        if (record == nullptr ||
            record->bytes == nullptr ||
            record->bytes->empty()) {
            ++g_fail_open;
            return false;
        }

        ps->code = record->bytes->data();
        ps->code_size = record->bytes->size();

        ++g_materialized;
        return true;
    }
    catch (...) {
        ++g_fail_open;
        return false;
    }
}

void on_init_pipeline(
    device *d,
    pipeline_layout,
    std::uint32_t count,
    const pipeline_subobject *subobjects,
    pipeline p)
{
    if (!accepts_device(d) ||
        p.handle == 0u)
        return;

    const auto *ps =
        find_pixel_shader(count, subobjects);

    if (ps == nullptr ||
        ps->code == nullptr ||
        ps->code_size == 0u)
        return;

    std::shared_ptr<const replacement_record> record;

    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        const auto found =
            g_code_records.find(ps->code);

        if (found == g_code_records.end())
            return;

        record = found->second;
    }

    if (record == nullptr ||
        record->bytes == nullptr ||
        record->bytes->size() != ps->code_size) {
        ++g_init_mismatch;
        g_quarantined.store(true);
        log_message(
            reshade::log::level::error,
            "[DSRRL A1 CREATE] create->init size attestation mismatch; quarantined.");
        return;
    }

    const auto digest =
        dsrrl::operators::legacy_plan::hashing::
            sha256(
                static_cast<const std::uint8_t *>(
                    ps->code),
                ps->code_size);

    if (!digest_equal(
            digest,
            record->output_sha256)) {
        ++g_init_mismatch;
        g_quarantined.store(true);
        log_message(
            reshade::log::level::error,
            "[DSRRL A1 CREATE] create->init SHA-256 attestation mismatch; quarantined.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        g_pipeline_records[p.handle] = record;
    }

    ++g_init_attested;
}

void on_destroy_pipeline(
    device *d,
    pipeline p)
{
    if (d == nullptr ||
        d->get_api() != device_api::d3d11 ||
        p.handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(g_state_mutex);
    g_pipeline_records.erase(p.handle);
}

void on_bind_pipeline(
    command_list *,
    pipeline_stage stages,
    pipeline p)
{
    if ((static_cast<std::uint32_t>(stages) &
         static_cast<std::uint32_t>(
             pipeline_stage::pixel_shader)) == 0u ||
        p.handle == 0u)
        return;

    std::shared_ptr<const replacement_record> record;

    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        const auto found =
            g_pipeline_records.find(p.handle);

        if (found == g_pipeline_records.end())
            return;

        record = found->second;
    }

    if (record == nullptr ||
        record->plan_index >=
            g_first_bind_logged.size())
        return;

    ++g_target_binds;

    if (!g_first_bind_logged[record->plan_index].exchange(true)) {
        char line[256]{};

        std::snprintf(
            line,
            sizeof(line),
            "[DSRRL A1 CREATE] FIRST_BIND plan=%u owners=0x%08X ops=%u full=%u",
            static_cast<unsigned>(record->plan_index),
            static_cast<unsigned>(
                record->selected_owners),
            static_cast<unsigned>(
                record->selected_ops),
            record->full_plan_materialized ? 1u : 0u);

        log_message(reshade::log::level::info, line);
    }
}

void on_present(
    command_queue *,
    swapchain *,
    const rect *,
    const rect *,
    std::uint32_t,
    const rect *)
{
    const auto present =
        ++g_present_count;

    if (present == 1u ||
        (present % 300u) == 0u)
        log_state("LIVE");
}

void register_events()
{
    reshade::register_event<
        reshade::addon_event::init_device>(
            on_init_device);
    reshade::register_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);
    reshade::register_event<
        reshade::addon_event::create_pipeline>(
            on_create_pipeline);
    reshade::register_event<
        reshade::addon_event::init_pipeline>(
            on_init_pipeline);
    reshade::register_event<
        reshade::addon_event::destroy_pipeline>(
            on_destroy_pipeline);
    reshade::register_event<
        reshade::addon_event::bind_pipeline>(
            on_bind_pipeline);
    reshade::register_event<
        reshade::addon_event::present>(
            on_present);
}

void unregister_events()
{
    reshade::unregister_event<
        reshade::addon_event::present>(
            on_present);
    reshade::unregister_event<
        reshade::addon_event::bind_pipeline>(
            on_bind_pipeline);
    reshade::unregister_event<
        reshade::addon_event::destroy_pipeline>(
            on_destroy_pipeline);
    reshade::unregister_event<
        reshade::addon_event::init_pipeline>(
            on_init_pipeline);
    reshade::unregister_event<
        reshade::addon_event::create_pipeline>(
            on_create_pipeline);
    reshade::unregister_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);
    reshade::unregister_event<
        reshade::addon_event::init_device>(
            on_init_device);
}

} // namespace

extern "C" __declspec(dllexport)
const char *NAME =
    "DSRRL Renderer Core A1 Create-Time Diagnostic";

extern "C" __declspec(dllexport)
const char *AUTHOR =
    "DSR Restored Lighting";

extern "C" __declspec(dllexport)
const char *DESCRIPTION =
    "Diagnostic exact-SHA create-time materialization for five closed A1 PTDE operator islands.";

extern "C" __declspec(dllexport)
bool AddonInit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    if (!reshade::register_addon(
            addon_module,
            reshade_module))
        return false;

    if (!enable_diagnostic_islands()) {
        reshade::unregister_addon(
            addon_module,
            reshade_module);
        return false;
    }

    register_events();

    log_message(
        reshade::log::level::info,
        "[DSRRL A1 CREATE] READY API20 D3D11 DIAGNOSTIC; exact SHA-256; five closed islands enabled; runtime/pixel unvalidated.");

    return true;
}

extern "C" __declspec(dllexport)
void AddonUninit(
    HMODULE addon_module,
    HMODULE reshade_module)
{
    unregister_events();

    log_state("UNLOAD");

    {
        std::lock_guard<std::mutex> lock(g_state_mutex);
        g_pipeline_records.clear();
        g_code_records.clear();
        g_cache.clear();
        g_device = nullptr;
    }

    reshade::unregister_addon(
        addon_module,
        reshade_module);
}
