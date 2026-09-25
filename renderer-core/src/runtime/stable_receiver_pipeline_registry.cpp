#include "dsrrl/runtime/stable_receiver_pipeline_registry.hpp"
#include "dsrrl/runtime/generated_stable_hemenv_receivers_v1.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace dsrrl::runtime {
namespace {

struct bound_receiver {
    std::uint64_t pipeline_handle = 0;
    std::uint32_t receiver_id = 0;
};

std::mutex g_mutex;
std::unordered_map<std::uint64_t,std::uint32_t> g_pipeline_receiver;
std::unordered_set<std::uint64_t> g_ambiguous_pipeline;
std::unordered_map<const void*,bound_receiver> g_bound_receiver;

std::atomic<std::uint64_t> g_pipeline_inits{0};
std::atomic<std::uint64_t> g_candidate_size_hits{0};
std::atomic<std::uint64_t> g_exact_receiver_hits{0};
std::atomic<std::uint64_t> g_candidate_hash_misses{0};
std::atomic<std::uint64_t> g_pixel_binds{0};
std::atomic<std::uint64_t> g_exact_binds{0};
std::atomic<std::uint64_t> g_unknown_binds{0};
std::atomic<std::uint64_t> g_lookups{0};
std::atomic<std::uint64_t> g_lookup_hits{0};
std::atomic<std::uint64_t> g_lookup_misses{0};
std::atomic<std::uint64_t> g_handle_conflicts{0};

} // namespace

bool stable_receiver_observe_pipeline(
    std::uint64_t pipeline_handle,
    const void *pixel_shader_code,
    std::size_t pixel_shader_size) noexcept
{
    ++g_pipeline_inits;

    if (pipeline_handle == 0u ||
        pixel_shader_code == nullptr ||
        pixel_shader_size == 0u ||
        !generated::stable_hemenv_candidate_size(pixel_shader_size))
        return false;

    ++g_candidate_size_hits;

    const auto digest =
        operators::legacy_plan::hashing::sha256(
            static_cast<const std::uint8_t *>(pixel_shader_code),
            pixel_shader_size);

    return stable_receiver_observe_pipeline_digest(
        pipeline_handle,
        digest,
        pixel_shader_size);
}

bool stable_receiver_observe_pipeline_digest(
    std::uint64_t pipeline_handle,
    const std::array<std::uint8_t,32> &pixel_shader_sha256,
    std::size_t pixel_shader_size) noexcept
{
    if (pipeline_handle == 0u ||
        !generated::stable_hemenv_candidate_size(pixel_shader_size))
        return false;

    const auto receiver =
        generated::stable_hemenv_receiver_id(
            pixel_shader_sha256,
            pixel_shader_size);

    if (receiver == 0u) {
        ++g_candidate_hash_misses;
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pipeline_receiver.erase(pipeline_handle);
        g_ambiguous_pipeline.insert(pipeline_handle);
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(g_mutex);

        if (g_ambiguous_pipeline.find(pipeline_handle) !=
            g_ambiguous_pipeline.end())
            return false;

        const auto existing =
            g_pipeline_receiver.find(pipeline_handle);

        if (existing != g_pipeline_receiver.end() &&
            existing->second != receiver) {
            g_pipeline_receiver.erase(existing);
            g_ambiguous_pipeline.insert(pipeline_handle);
            ++g_handle_conflicts;
            return false;
        }

        g_pipeline_receiver[pipeline_handle] = receiver;
    } catch (...) {
        return false;
    }

    ++g_exact_receiver_hits;
    return true;
}

void stable_receiver_forget_pipeline(
    std::uint64_t pipeline_handle) noexcept
{
    if (pipeline_handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_pipeline_receiver.erase(pipeline_handle);
    g_ambiguous_pipeline.erase(pipeline_handle);

    for (auto it = g_bound_receiver.begin();
         it != g_bound_receiver.end();) {
        if (it->second.pipeline_handle == pipeline_handle)
            it = g_bound_receiver.erase(it);
        else
            ++it;
    }
}

void stable_receiver_observe_bind(
    const void *command_list_key,
    bool pixel_stage_bound,
    std::uint64_t pipeline_handle) noexcept
{
    if (!pixel_stage_bound || command_list_key == nullptr)
        return;

    ++g_pixel_binds;

    std::lock_guard<std::mutex> lock(g_mutex);

    const auto found =
        g_pipeline_receiver.find(pipeline_handle);

    if (found == g_pipeline_receiver.end() ||
        g_ambiguous_pipeline.find(pipeline_handle) !=
            g_ambiguous_pipeline.end()) {
        g_bound_receiver.erase(command_list_key);
        ++g_unknown_binds;
        return;
    }

    g_bound_receiver[command_list_key] =
        bound_receiver{pipeline_handle, found->second};
    ++g_exact_binds;
}

bool stable_receiver_bound(
    const void *command_list_key,
    std::uint32_t &receiver_id) noexcept
{
    ++g_lookups;
    receiver_id = 0u;

    if (command_list_key == nullptr) {
        ++g_lookup_misses;
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    const auto found = g_bound_receiver.find(command_list_key);
    if (found == g_bound_receiver.end()) {
        ++g_lookup_misses;
        return false;
    }

    receiver_id = found->second.receiver_id;
    ++g_lookup_hits;
    return true;
}

void stable_receiver_pipeline_reset() noexcept
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pipeline_receiver.clear();
        g_ambiguous_pipeline.clear();
        g_bound_receiver.clear();
    }

    g_pipeline_inits.store(0);
    g_candidate_size_hits.store(0);
    g_exact_receiver_hits.store(0);
    g_candidate_hash_misses.store(0);
    g_pixel_binds.store(0);
    g_exact_binds.store(0);
    g_unknown_binds.store(0);
    g_lookups.store(0);
    g_lookup_hits.store(0);
    g_lookup_misses.store(0);
    g_handle_conflicts.store(0);
}

stable_receiver_pipeline_telemetry
stable_receiver_pipeline_stats() noexcept
{
    return {
        g_pipeline_inits.load(),
        g_candidate_size_hits.load(),
        g_exact_receiver_hits.load(),
        g_candidate_hash_misses.load(),
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
