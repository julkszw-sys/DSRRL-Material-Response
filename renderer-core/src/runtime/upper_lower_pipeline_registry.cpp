#include "dsrrl/runtime/upper_lower_pipeline_registry.hpp"
#include "dsrrl/operators/lightbank/generated_upper_lower_hemenv_v1.hpp"
#include "dsrrl/operators/lightbank/generated_upper_lower_hemenvlerp_v1.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace dsrrl::runtime {
namespace {

namespace generated =
    operators::lightbank::generated;
namespace generated_ul_lerp =
    operators::lightbank::generated_lerp;
namespace hashing =
    operators::legacy_plan::hashing;

struct created_code_record {
    std::size_t size = 0u;
    hashing::sha256_digest digest{};
    upper_lower_receiver_identity identity{};
};

struct bound_receiver {
    std::uint64_t pipeline_handle = 0u;
    upper_lower_receiver_identity identity{};
};

std::mutex g_mutex;
std::unordered_map<const void *, created_code_record>
    g_created_codes;
std::unordered_map<std::uint64_t, upper_lower_receiver_identity>
    g_pipeline_receivers;
std::unordered_set<std::uint64_t>
    g_ambiguous_pipelines;
std::unordered_map<const void *, bound_receiver>
    g_bound_receivers;

std::atomic<std::uint64_t> g_created_code_attested{0};
std::atomic<std::uint64_t> g_created_code_conflict{0};
std::atomic<std::uint64_t> g_pipeline_inits{0};
std::atomic<std::uint64_t> g_exact_nospc_hits{0};
std::atomic<std::uint64_t> g_exact_spc_hits{0};
std::atomic<std::uint64_t> g_init_mismatch{0};
std::atomic<std::uint64_t> g_pixel_binds{0};
std::atomic<std::uint64_t> g_nospc_binds{0};
std::atomic<std::uint64_t> g_spc_binds{0};
std::atomic<std::uint64_t> g_unknown_binds{0};
std::atomic<std::uint64_t> g_lookups{0};
std::atomic<std::uint64_t> g_lookup_hits{0};
std::atomic<std::uint64_t> g_lookup_misses{0};
std::atomic_bool g_quarantined{false};

upper_lower_receiver_identity identity_from_plan(
    const generated::upper_lower_hemenv_plan &plan) noexcept
{
    return {
        plan.plan_index,
        plan.shader_index,
        plan.stable_receiver_id,
        plan.stratum ==
                generated::upper_lower_hemenv_stratum::spc
            ? operators::lightbank::
                  upper_lower_hemenv_stratum::spc
            : operators::lightbank::
                  upper_lower_hemenv_stratum::nospc,
        operators::lightbank::
            upper_lower_hemenv_family::hemenv
    };
}

upper_lower_receiver_identity identity_from_plan(
    const generated_ul_lerp::upper_lower_hemenvlerp_plan &plan) noexcept
{
    return {
        plan.plan_index,
        plan.shader_index,
        plan.stable_receiver_id,
        plan.stratum ==
                generated_ul_lerp::
                    upper_lower_hemenvlerp_stratum::spc
            ? operators::lightbank::
                  upper_lower_hemenv_stratum::spc
            : operators::lightbank::
                  upper_lower_hemenv_stratum::nospc,
        operators::lightbank::
            upper_lower_hemenv_family::hemenvlerp
    };
}

bool identity_equal(
    const upper_lower_receiver_identity &a,
    const upper_lower_receiver_identity &b) noexcept
{
    return
        a.plan_index == b.plan_index &&
        a.shader_index == b.shader_index &&
        a.stable_receiver_id ==
            b.stable_receiver_id &&
        a.stratum == b.stratum &&
        a.family == b.family;
}

bool identify_exact_stock(
    const void *code,
    std::size_t size,
    upper_lower_receiver_identity &identity) noexcept
{
    identity = {};

    if (code == nullptr ||
        size == 0u)
        return false;

    const auto digest =
        hashing::sha256(
            static_cast<const std::uint8_t *>(code),
            size);

    bool found = false;

    for (const auto &plan :
         generated::k_upper_lower_hemenv_plans) {
        if (plan.stock_size != size ||
            !hashing::matches_hex(
                digest,
                plan.stock_sha256))
            continue;

        if (found)
            return false;

        identity =
            identity_from_plan(plan);
        found = true;
    }

    for (const auto &plan :
         generated_ul_lerp::
             k_upper_lower_hemenvlerp_plans) {
        if (plan.stock_size != size ||
            !hashing::matches_hex(
                digest,
                plan.stock_sha256))
            continue;

        if (found)
            return false;

        identity =
            identity_from_plan(plan);
        found = true;
    }

    return found;
}

void account_exact(
    const upper_lower_receiver_identity &identity) noexcept
{
    if (identity.stratum ==
        operators::lightbank::
            upper_lower_hemenv_stratum::spc)
        ++g_exact_spc_hits;
    else
        ++g_exact_nospc_hits;
}

} // namespace

bool upper_lower_receiver_attest_created_code(
    const void *code,
    std::size_t size,
    const upper_lower_receiver_identity &identity) noexcept
{
    if (g_quarantined.load() ||
        code == nullptr ||
        size == 0u ||
        !identity.valid())
        return false;

    created_code_record record{};
    record.size = size;
    record.digest =
        hashing::sha256(
            static_cast<const std::uint8_t *>(code),
            size);
    record.identity = identity;

    try {
        std::lock_guard<std::mutex> lock(g_mutex);

        const auto found =
            g_created_codes.find(code);

        if (found != g_created_codes.end()) {
            const bool same =
                found->second.size == record.size &&
                found->second.digest == record.digest &&
                identity_equal(
                    found->second.identity,
                    record.identity);

            if (!same) {
                ++g_created_code_conflict;
                g_quarantined.store(true);
                return false;
            }

            return true;
        }

        g_created_codes.emplace(
            code,
            record);
    } catch (...) {
        return false;
    }

    ++g_created_code_attested;
    return true;
}

bool upper_lower_receiver_observe_pipeline(
    std::uint64_t pipeline_handle,
    const void *pixel_shader_code,
    std::size_t pixel_shader_size) noexcept
{
    ++g_pipeline_inits;

    if (g_quarantined.load() ||
        pipeline_handle == 0u ||
        pixel_shader_code == nullptr ||
        pixel_shader_size == 0u)
        return false;

    upper_lower_receiver_identity identity{};
    bool attested_created_code = false;

    try {
        std::lock_guard<std::mutex> lock(g_mutex);

        const auto pending =
            g_created_codes.find(
                pixel_shader_code);

        if (pending != g_created_codes.end()) {
            const auto digest =
                hashing::sha256(
                    static_cast<const std::uint8_t *>(
                        pixel_shader_code),
                    pixel_shader_size);

            if (pending->second.size !=
                    pixel_shader_size ||
                pending->second.digest != digest) {
                ++g_init_mismatch;
                g_quarantined.store(true);
                return false;
            }

            identity =
                pending->second.identity;
            attested_created_code = true;
        }
    } catch (...) {
        return false;
    }

    if (!attested_created_code &&
        !identify_exact_stock(
            pixel_shader_code,
            pixel_shader_size,
            identity))
        return false;

    try {
        std::lock_guard<std::mutex> lock(g_mutex);

        if (g_ambiguous_pipelines.find(
                pipeline_handle) !=
            g_ambiguous_pipelines.end())
            return false;

        const auto existing =
            g_pipeline_receivers.find(
                pipeline_handle);

        if (existing !=
                g_pipeline_receivers.end() &&
            !identity_equal(
                existing->second,
                identity)) {
            g_pipeline_receivers.erase(
                existing);
            g_ambiguous_pipelines.insert(
                pipeline_handle);
            ++g_init_mismatch;
            return false;
        }

        g_pipeline_receivers[
            pipeline_handle] =
                identity;
    } catch (...) {
        return false;
    }

    account_exact(identity);
    return true;
}

void upper_lower_receiver_forget_pipeline(
    std::uint64_t pipeline_handle) noexcept
{
    if (pipeline_handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(g_mutex);

    g_pipeline_receivers.erase(
        pipeline_handle);
    g_ambiguous_pipelines.erase(
        pipeline_handle);

    for (auto it =
             g_bound_receivers.begin();
         it != g_bound_receivers.end();) {
        if (it->second.pipeline_handle ==
            pipeline_handle)
            it =
                g_bound_receivers.erase(it);
        else
            ++it;
    }
}

void upper_lower_receiver_observe_bind(
    const void *command_list_key,
    bool pixel_stage_bound,
    std::uint64_t pipeline_handle) noexcept
{
    if (!pixel_stage_bound ||
        command_list_key == nullptr)
        return;

    ++g_pixel_binds;

    try {
        std::lock_guard<std::mutex> lock(g_mutex);

        const auto found =
            g_pipeline_receivers.find(
                pipeline_handle);

        if (found ==
                g_pipeline_receivers.end() ||
            g_ambiguous_pipelines.find(
                pipeline_handle) !=
                g_ambiguous_pipelines.end()) {
            g_bound_receivers.erase(
                command_list_key);
            ++g_unknown_binds;
            return;
        }

        g_bound_receivers[
            command_list_key] = {
                pipeline_handle,
                found->second
            };

        if (found->second.stratum ==
            operators::lightbank::
                upper_lower_hemenv_stratum::spc)
            ++g_spc_binds;
        else
            ++g_nospc_binds;
    } catch (...) {
        try {
            std::lock_guard<std::mutex> lock(
                g_mutex);
            g_bound_receivers.erase(
                command_list_key);
        } catch (...) {
        }

        ++g_unknown_binds;
    }
}

bool upper_lower_receiver_bound(
    const void *command_list_key,
    upper_lower_receiver_identity &identity) noexcept
{
    ++g_lookups;
    identity = {};

    if (command_list_key == nullptr ||
        g_quarantined.load()) {
        ++g_lookup_misses;
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);

    const auto found =
        g_bound_receivers.find(
            command_list_key);

    if (found ==
        g_bound_receivers.end()) {
        ++g_lookup_misses;
        return false;
    }

    identity =
        found->second.identity;
    ++g_lookup_hits;
    return true;
}

void upper_lower_receiver_pipeline_reset() noexcept
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_created_codes.clear();
        g_pipeline_receivers.clear();
        g_ambiguous_pipelines.clear();
        g_bound_receivers.clear();
    }

    g_created_code_attested.store(0);
    g_created_code_conflict.store(0);
    g_pipeline_inits.store(0);
    g_exact_nospc_hits.store(0);
    g_exact_spc_hits.store(0);
    g_init_mismatch.store(0);
    g_pixel_binds.store(0);
    g_nospc_binds.store(0);
    g_spc_binds.store(0);
    g_unknown_binds.store(0);
    g_lookups.store(0);
    g_lookup_hits.store(0);
    g_lookup_misses.store(0);
    g_quarantined.store(false);
}

upper_lower_pipeline_telemetry
upper_lower_receiver_pipeline_stats() noexcept
{
    return {
        g_created_code_attested.load(),
        g_created_code_conflict.load(),
        g_pipeline_inits.load(),
        g_exact_nospc_hits.load(),
        g_exact_spc_hits.load(),
        g_init_mismatch.load(),
        g_pixel_binds.load(),
        g_nospc_binds.load(),
        g_spc_binds.load(),
        g_unknown_binds.load(),
        g_lookups.load(),
        g_lookup_hits.load(),
        g_lookup_misses.load(),
        g_quarantined.load()
    };
}

} // namespace dsrrl::runtime
