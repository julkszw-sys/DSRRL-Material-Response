#include "dsrrl/runtime/subsurface_pipeline_registry.hpp"
#include "dsrrl/operators/resource_bridges/subsurface_route.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace dsrrl::runtime {
namespace {

struct bound_subsurface {
    std::uint64_t pipeline_handle = 0;
    std::uint32_t target_receiver = 0;
};

std::mutex g_mutex;
std::unordered_map<std::uint64_t,std::uint32_t> g_pipeline_target;
std::unordered_map<const void*,bound_subsurface> g_bound;

std::atomic<std::uint64_t> g_pipeline_inits{0};
std::atomic<std::uint64_t> g_exact_hits{0};
std::atomic<std::uint64_t> g_pixel_binds{0};
std::atomic<std::uint64_t> g_exact_binds{0};
std::atomic<std::uint64_t> g_lookup_hits{0};
std::atomic<std::uint64_t> g_lookup_misses{0};

const operators::resource_bridges::subsurface_receiver_route *
find_route(
    const std::array<std::uint8_t,32> &digest) noexcept
{
    for (const auto &route :
         operators::resource_bridges::
             k_subsurface_receiver_routes) {
        if (operators::legacy_plan::hashing::
                matches_hex(
                    digest,
                    route.dsr_receiver_sha256))
            return &route;
    }

    return nullptr;
}

} // namespace

bool subsurface_receiver_observe_pipeline(
    std::uint64_t pipeline_handle,
    const void *pixel_shader_code,
    std::size_t pixel_shader_size) noexcept
{
    ++g_pipeline_inits;

    if (pipeline_handle == 0u ||
        pixel_shader_code == nullptr ||
        pixel_shader_size == 0u)
        return false;

    const auto digest =
        operators::legacy_plan::hashing::sha256(
            static_cast<const std::uint8_t *>(
                pixel_shader_code),
            pixel_shader_size);

    const auto *route =
        find_route(digest);

    if (route == nullptr)
        return false;

    try {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto found =
            g_pipeline_target.find(
                pipeline_handle);

        if (found != g_pipeline_target.end() &&
            found->second !=
                route->target_plain_receiver_id) {
            g_pipeline_target.erase(found);
            return false;
        }

        g_pipeline_target[pipeline_handle] =
            route->target_plain_receiver_id;
    } catch (...) {
        return false;
    }

    ++g_exact_hits;
    return true;
}

void subsurface_receiver_forget_pipeline(
    std::uint64_t pipeline_handle) noexcept
{
    if (pipeline_handle == 0u)
        return;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_pipeline_target.erase(pipeline_handle);

    for (auto it = g_bound.begin();
         it != g_bound.end();) {
        if (it->second.pipeline_handle ==
            pipeline_handle)
            it = g_bound.erase(it);
        else
            ++it;
    }
}

void subsurface_receiver_observe_bind(
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
            g_pipeline_target.find(
                pipeline_handle);

        if (found == g_pipeline_target.end()) {
            g_bound.erase(command_list_key);
            return;
        }

        g_bound[command_list_key] = {
            pipeline_handle,
            found->second
        };
        ++g_exact_binds;
    } catch (...) {
        try {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_bound.erase(command_list_key);
        } catch (...) {
        }
    }
}

bool subsurface_receiver_bound(
    const void *command_list_key,
    std::uint32_t &target_plain_receiver_id) noexcept
{
    target_plain_receiver_id = 0u;

    if (command_list_key == nullptr) {
        ++g_lookup_misses;
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    const auto found =
        g_bound.find(command_list_key);

    if (found == g_bound.end()) {
        ++g_lookup_misses;
        return false;
    }

    target_plain_receiver_id =
        found->second.target_receiver;
    ++g_lookup_hits;
    return true;
}

void subsurface_receiver_pipeline_reset() noexcept
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_pipeline_target.clear();
        g_bound.clear();
    }

    g_pipeline_inits.store(0);
    g_exact_hits.store(0);
    g_pixel_binds.store(0);
    g_exact_binds.store(0);
    g_lookup_hits.store(0);
    g_lookup_misses.store(0);
}

subsurface_pipeline_telemetry
subsurface_receiver_pipeline_stats() noexcept
{
    return {
        g_pipeline_inits.load(),
        g_exact_hits.load(),
        g_pixel_binds.load(),
        g_exact_binds.load(),
        g_lookup_hits.load(),
        g_lookup_misses.load()
    };
}

} // namespace dsrrl::runtime
