#include "dsrrl/runtime/hemenvlerp_pipeline_registry.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"

#include "dsrrl/operators/material_response/generated_diffuse_response_v1.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace dsrrl::runtime {
namespace {

namespace gen =
    operators::material_response::generated_diffuse_v1;
namespace hashing =
    operators::legacy_plan::hashing;

struct bound_identity {
    std::uint64_t pipeline_handle = 0u;
    hemenvlerp_receiver_identity identity{};
};

std::mutex g_mutex;
std::unordered_map<
    std::uint64_t,
    hemenvlerp_receiver_identity> g_pipeline_identity;
std::unordered_set<std::uint64_t> g_ambiguous_pipeline;
std::unordered_map<const void *,bound_identity> g_bound_identity;

struct bound_identity_tls {
    const void *command_list_key = nullptr;
    bound_identity value{};
    std::uint64_t epoch = 0u;
    bool present = false;
};

std::atomic<std::uint64_t> g_bound_epoch{1u};
thread_local bound_identity_tls g_bound_tls{};

std::atomic<std::uint64_t> g_pipeline_inits{0};
std::atomic<std::uint64_t> g_candidate_size_hits{0};
std::atomic<std::uint64_t> g_exact_hits{0};
std::atomic<std::uint64_t> g_hash_misses{0};
std::atomic<std::uint64_t> g_pixel_binds{0};
std::atomic<std::uint64_t> g_exact_binds{0};
std::atomic<std::uint64_t> g_unknown_binds{0};
std::atomic<std::uint64_t> g_lookups{0};
std::atomic<std::uint64_t> g_lookup_hits{0};
std::atomic<std::uint64_t> g_lookup_misses{0};
std::atomic<std::uint64_t> g_handle_conflicts{0};

bool candidate_size(std::size_t size) noexcept
{
    for (const auto &plan :
         gen::k_plans)
        if (plan.family ==
                gen::receiver_family::hemenvlerp &&
            plan.stock_size == size)
            return true;
    return false;
}

bool exact_identity_digest(
    const std::array<std::uint8_t,32> &digest,
    std::size_t size,
    hemenvlerp_receiver_identity &out) noexcept
{
    out = {};
    if (size == 0u ||
        !candidate_size(size))
        return false;

    const gen::plan *hit = nullptr;
    for (const auto &plan :
         gen::k_plans) {
        if (plan.family !=
                gen::receiver_family::hemenvlerp ||
            plan.stock_size != size ||
            !hashing::matches_hex(
                digest,
                plan.stock_sha256))
            continue;

        if (hit != nullptr)
            return false;

        hit = &plan;
    }

    if (hit == nullptr)
        return false;

    out.pair_index =
        hit->family_index;
    out.semantic_receiver_id =
        hit->receiver_id;
    out.exact = true;
    return true;
}

} // namespace

bool hemenvlerp_receiver_observe_pipeline(
    std::uint64_t pipeline_handle,
    const void *pixel_shader_code,
    std::size_t pixel_shader_size) noexcept
{
    ++g_pipeline_inits;

    if (pipeline_handle == 0u ||
        pixel_shader_code == nullptr ||
        pixel_shader_size == 0u ||
        !candidate_size(pixel_shader_size))
        return false;

    ++g_candidate_size_hits;

    const auto digest =
        hashing::sha256(
            static_cast<const std::uint8_t *>(
                pixel_shader_code),
            pixel_shader_size);

    return hemenvlerp_receiver_observe_pipeline_digest(
        pipeline_handle,
        digest,
        pixel_shader_size);
}

bool hemenvlerp_receiver_observe_pipeline_digest(
    std::uint64_t pipeline_handle,
    const std::array<std::uint8_t,32> &pixel_shader_sha256,
    std::size_t pixel_shader_size) noexcept
{
    if (pipeline_handle == 0u ||
        pixel_shader_size == 0u ||
        !candidate_size(pixel_shader_size))
        return false;

    hemenvlerp_receiver_identity identity{};
    if (!exact_identity_digest(
            pixel_shader_sha256,
            pixel_shader_size,
            identity)) {
        ++g_hash_misses;
        try {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_pipeline_identity.erase(pipeline_handle);
            g_ambiguous_pipeline.insert(pipeline_handle);
        } catch (...) {
        }
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(g_mutex);

        if (g_ambiguous_pipeline.find(pipeline_handle) !=
            g_ambiguous_pipeline.end())
            return false;

        const auto existing =
            g_pipeline_identity.find(pipeline_handle);

        if (existing != g_pipeline_identity.end() &&
            (existing->second.pair_index != identity.pair_index ||
             existing->second.semantic_receiver_id !=
                identity.semantic_receiver_id)) {
            g_pipeline_identity.erase(existing);
            g_ambiguous_pipeline.insert(pipeline_handle);
            ++g_handle_conflicts;
            return false;
        }

        g_pipeline_identity[pipeline_handle] =
            identity;
    } catch (...) {
        return false;
    }

    ++g_exact_hits;
    return true;
}

void hemenvlerp_receiver_forget_pipeline(
    std::uint64_t pipeline_handle) noexcept
{
    if (pipeline_handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_pipeline_identity.erase(pipeline_handle);
    g_ambiguous_pipeline.erase(pipeline_handle);

    for (auto it =
             g_bound_identity.begin();
         it != g_bound_identity.end();) {
        if (it->second.pipeline_handle ==
            pipeline_handle)
            it =
                g_bound_identity.erase(it);
        else
            ++it;
    }

    ++g_bound_epoch;
}

void hemenvlerp_receiver_observe_bind(
    const void *command_list_key,
    bool pixel_stage_bound,
    std::uint64_t pipeline_handle) noexcept
{
    if (!pixel_stage_bound ||
        command_list_key == nullptr)
        return;

    telemetry::hot_count(g_pixel_binds);

    try {
        std::lock_guard<std::mutex> lock(g_mutex);

        const auto found =
            g_pipeline_identity.find(
                pipeline_handle);

        if (found ==
                g_pipeline_identity.end() ||
            g_ambiguous_pipeline.find(
                pipeline_handle) !=
                g_ambiguous_pipeline.end()) {
            g_bound_identity.erase(
                command_list_key);
            g_bound_tls = {
                command_list_key,
                {},
                g_bound_epoch.load(),
                false
            };
            telemetry::hot_count(g_unknown_binds);
            return;
        }

        const bound_identity bound{
            pipeline_handle,
            found->second
        };
        g_bound_identity[
            command_list_key] = bound;
        g_bound_tls = {
            command_list_key,
            bound,
            g_bound_epoch.load(),
            true
        };
        telemetry::hot_count(g_exact_binds);
    } catch (...) {
        try {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_bound_identity.erase(
                command_list_key);
            g_bound_tls = {
                command_list_key,
                {},
                g_bound_epoch.load(),
                false
            };
        } catch (...) {
            g_bound_tls = {};
        }
        telemetry::hot_count(g_unknown_binds);
    }
}

bool hemenvlerp_receiver_bound(
    const void *command_list_key,
    hemenvlerp_receiver_identity &identity) noexcept
{
    telemetry::hot_count(g_lookups);
    identity = {};

    if (command_list_key == nullptr) {
        telemetry::hot_count(g_lookup_misses);
        return false;
    }

    const auto epoch =
        g_bound_epoch.load();

    if (g_bound_tls.command_list_key ==
            command_list_key &&
        g_bound_tls.epoch == epoch) {
        if (!g_bound_tls.present) {
            telemetry::hot_count(g_lookup_misses);
            return false;
        }

        identity =
            g_bound_tls.value.identity;
        telemetry::hot_count(g_lookup_hits);
        return identity.exact;
    }

    std::lock_guard<std::mutex> lock(g_mutex);

    const auto found =
        g_bound_identity.find(
            command_list_key);

    if (found ==
        g_bound_identity.end()) {
        g_bound_tls = {
            command_list_key,
            {},
            g_bound_epoch.load(),
            false
        };
        telemetry::hot_count(g_lookup_misses);
        return false;
    }

    g_bound_tls = {
        command_list_key,
        found->second,
        g_bound_epoch.load(),
        true
    };
    identity =
        found->second.identity;
    telemetry::hot_count(g_lookup_hits);
    return identity.exact;
}

void hemenvlerp_receiver_pipeline_reset() noexcept
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pipeline_identity.clear();
        g_ambiguous_pipeline.clear();
        g_bound_identity.clear();
        ++g_bound_epoch;
    }

    g_bound_tls = {};

    g_pipeline_inits.store(0u);
    g_candidate_size_hits.store(0u);
    g_exact_hits.store(0u);
    g_hash_misses.store(0u);
    g_pixel_binds.store(0u);
    g_exact_binds.store(0u);
    g_unknown_binds.store(0u);
    g_lookups.store(0u);
    g_lookup_hits.store(0u);
    g_lookup_misses.store(0u);
    g_handle_conflicts.store(0u);
}

hemenvlerp_pipeline_telemetry
hemenvlerp_receiver_pipeline_stats() noexcept
{
    return {
        g_pipeline_inits.load(),
        g_candidate_size_hits.load(),
        g_exact_hits.load(),
        g_hash_misses.load(),
        g_pixel_binds.load(),
        g_exact_binds.load(),
        g_unknown_binds.load(),
        g_lookups.load(),
        g_lookup_hits.load(),
        g_lookup_misses.load(),
        g_handle_conflicts.load()
    };
}

} // namespace dsrrl::runtime
