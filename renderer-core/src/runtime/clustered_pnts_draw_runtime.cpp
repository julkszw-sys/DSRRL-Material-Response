#include "dsrrl/runtime/pointlight_ptde_source_runtime.hpp"
#include "dsrrl/runtime/clustered_pointlight_source_gate.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/clustered_pnts_draw_runtime.hpp"

#include "dsrrl/operators/point_light/clustered_sidecar.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"

#include <Windows.h>
#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dsrrl::runtime {
namespace {

using source_raw =
    operators::point_light::clustered_source_raw_v1;
using sidecar_payload =
    operators::point_light::clustered_sidecar_payload_v1;

struct producer_input_snapshot {
    std::uintptr_t owner = 0u;
    std::uint64_t serial = 0u;
    void *collection = nullptr;
    std::array<float,8> query{};
    std::uint8_t mask = 0u;
    bool valid = false;
};

struct draw_selection_tls {
    producer_input_snapshot input{};
    std::uint32_t material_max = 0u;
    operators::material_response::material_identity material{};
    operators::material_response::decision material_decision{};
    sidecar_payload payload{};
    clustered_pnts_prepare_failure cached_failure =
        clustered_pnts_prepare_failure::none;
    std::uint8_t sidecar_result_code = 0u;
    bool owner_verified = false;
    bool material_limit_ready = false;
    bool material_spc = false;
    bool authority_ready = false;
    bool payload_ready = false;
    bool neutral_no_pointlights = false;
    bool ready = false;
};

struct source_selection_cache_tls {
    std::uint64_t producer_serial = 0u;
    std::array<source_raw,4> sources{};
    std::uint8_t selected_count = 0u;
    clustered_pnts_prepare_failure failure =
        clustered_pnts_prepare_failure::none;
    bool attempted = false;
    bool ready = false;
    bool neutral = false;
};

thread_local draw_selection_tls g_draw_selection{};
thread_local source_selection_cache_tls g_source_selection_cache{};
thread_local producer_input_snapshot g_producer_input_tls{};
thread_local std::uint64_t g_local_serial = 0u;

// R40: mirror the mutation-driven shadow-state pattern used by mature D3D
// wrappers instead of expiring an otherwise exact source result at Present.
// The PTDE Bank/Lerp donor payload is a function of immutable bank identity plus
// the live source selector/beta/position state captured below. Active-light
// insertion advances a semantic generation; exact state changes are still
// compared on every lookup. The cache remains TLS and owns no host object.
struct frame_source_state_v1 {
    void *node = nullptr;
    void *collection = nullptr;
    std::uintptr_t target = 0u;
    std::uintptr_t owner = 0u;
    std::uintptr_t endpoint_source_a = 0u;
    std::uintptr_t endpoint_source_b = 0u;
    std::uintptr_t endpoint_param_a = 0u;
    std::uintptr_t endpoint_param_b = 0u;
    std::uint32_t endpoint_first_a = 0u;
    std::uint32_t endpoint_first_b = 0u;
    std::uint16_t endpoint_count_a = 0u;
    std::uint16_t endpoint_count_b = 0u;
    std::uint32_t source_id = 0u;
    std::uint32_t selector_word0 = 0u;
    std::uint32_t selector_word1 = 0u;
    std::array<std::uint32_t,4> position_bits{};
    std::uint8_t source_category = 0u;
    std::uint8_t source_class = 0u;
};

struct frame_source_cache_entry_v1 {
    frame_source_state_v1 state{};
    source_raw source{};
    bool valid = false;
};

// R38: source hits are exact-state keyed, so a linear 32-entry scan on every
// repeated material draw is unnecessary overhead. Use a power-of-two direct
// map: collisions are misses, never authority. Exact state comparison remains
// mandatory before reuse.
constexpr std::size_t k_frame_source_cache_entries = 128u;
static_assert(
    (k_frame_source_cache_entries &
     (k_frame_source_cache_entries - 1u)) == 0u);

// Selection remains frame-scoped because its query describes one draw. Source
// donor results are instead invalidated by semantic mutation. This follows the
// generation/dirty-state model used by DXVK/RenderDoc-style state trackers.
std::atomic<std::uint64_t> g_source_frame_epoch{1u};
std::atomic<std::uint64_t> g_source_semantic_generation{1u};
std::atomic_bool g_source_exec_attested{false};
thread_local std::uint64_t g_source_cache_seen_generation = 0u;
thread_local std::array<
    frame_source_cache_entry_v1,
    k_frame_source_cache_entries> g_source_frame_cache{};

void reset_frame_source_cache_tls(
    std::uint64_t generation) noexcept
{
    g_source_frame_cache = {};
    g_source_cache_seen_generation = generation;
}

void invalidate_source_semantic_generation() noexcept
{
    auto next =
        g_source_semantic_generation.fetch_add(
            1u,
            std::memory_order_acq_rel) + 1u;
    if (next == 0u)
        g_source_semantic_generation.fetch_add(
            1u,
            std::memory_order_acq_rel);
}

bool same_frame_source_state(
    const frame_source_state_v1 &a,
    const frame_source_state_v1 &b) noexcept
{
    return
        a.node == b.node &&
        a.collection == b.collection &&
        a.target == b.target &&
        a.owner == b.owner &&
        a.endpoint_source_a == b.endpoint_source_a &&
        a.endpoint_source_b == b.endpoint_source_b &&
        a.endpoint_param_a == b.endpoint_param_a &&
        a.endpoint_param_b == b.endpoint_param_b &&
        a.endpoint_first_a == b.endpoint_first_a &&
        a.endpoint_first_b == b.endpoint_first_b &&
        a.endpoint_count_a == b.endpoint_count_a &&
        a.endpoint_count_b == b.endpoint_count_b &&
        a.source_id == b.source_id &&
        a.selector_word0 == b.selector_word0 &&
        a.selector_word1 == b.selector_word1 &&
        a.position_bits == b.position_bits &&
        a.source_category == b.source_category &&
        a.source_class == b.source_class;
}

std::size_t frame_source_cache_index(
    const frame_source_state_v1 &state) noexcept
{
    std::uint64_t h =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(
                state.node)) >> 4u;
    const auto mix = [&h](std::uint64_t v) noexcept {
        h ^= v +
             0x9e3779b97f4a7c15ull +
             (h << 6u) +
             (h >> 2u);
    };
    mix(static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(
            state.collection)));
    mix(static_cast<std::uint64_t>(state.target));
    mix(static_cast<std::uint64_t>(state.owner));
    mix(static_cast<std::uint64_t>(state.endpoint_source_a));
    mix(static_cast<std::uint64_t>(state.endpoint_source_b));
    mix(static_cast<std::uint64_t>(state.endpoint_param_a));
    mix(static_cast<std::uint64_t>(state.endpoint_param_b));
    mix(static_cast<std::uint64_t>(state.endpoint_first_a));
    mix(static_cast<std::uint64_t>(state.endpoint_first_b));
    mix(static_cast<std::uint64_t>(state.endpoint_count_a));
    mix(static_cast<std::uint64_t>(state.endpoint_count_b));
    mix(static_cast<std::uint64_t>(state.source_id));
    mix(static_cast<std::uint64_t>(state.selector_word0));
    mix(static_cast<std::uint64_t>(state.selector_word1));
    for (const auto word : state.position_bits)
        mix(static_cast<std::uint64_t>(word));
    mix(static_cast<std::uint64_t>(state.source_category));
    mix(static_cast<std::uint64_t>(state.source_class));
    return static_cast<std::size_t>(
        h & (k_frame_source_cache_entries - 1u));
}

struct frame_selection_key_v1 {
    std::uintptr_t owner = 0u;
    void *collection = nullptr;
    std::array<std::uint32_t,8> query_bits{};
    std::array<void *,4> bucket_heads{};
    std::uint8_t mask = 0u;
};

struct frame_selection_cache_entry_v1 {
    std::uint64_t epoch = 0u;
    std::uint64_t hash = 0u;
    frame_selection_key_v1 key{};
    std::array<std::uint32_t,4> ids{};
    std::array<void *,4> nodes{};
    std::uint8_t count = 0u;
};

constexpr std::size_t k_frame_selection_cache_entries = 128u;
static_assert(
    (k_frame_selection_cache_entries &
     (k_frame_selection_cache_entries - 1u)) == 0u);
thread_local std::array<
    frame_selection_cache_entry_v1,
    k_frame_selection_cache_entries> g_frame_selection_cache{};

std::uint64_t frame_selection_hash(
    const frame_selection_key_v1 &key) noexcept
{
    std::uint64_t h =
        1469598103934665603ull;
    const auto mix = [&h](std::uint64_t v) noexcept {
        h ^= v;
        h *= 1099511628211ull;
    };
    mix(static_cast<std::uint64_t>(key.owner));
    mix(static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(
            key.collection)));
    mix(static_cast<std::uint64_t>(key.mask));
    for (const auto word : key.query_bits)
        mix(static_cast<std::uint64_t>(word));
    for (const auto head : key.bucket_heads)
        mix(static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(
                head)));
    return h;
}

bool same_frame_selection_key(
    const frame_selection_key_v1 &a,
    const frame_selection_key_v1 &b) noexcept
{
    return
        a.owner == b.owner &&
        a.collection == b.collection &&
        a.mask == b.mask &&
        a.query_bits == b.query_bits &&
        a.bucket_heads == b.bucket_heads;
}

#ifdef DSRRL_POINTLIGHT_PROFILE
constexpr std::uint32_t k_pointlight_profile_sample_period = 128u;

struct pointlight_profile_bucket {
    std::atomic<std::uint64_t> samples{0u};
    std::atomic<std::uint64_t> ticks{0u};
    std::atomic<std::uint64_t> max_ticks{0u};
};

pointlight_profile_bucket g_prof_producer{};
pointlight_profile_bucket g_prof_select{};
pointlight_profile_bucket g_prof_capture{};
pointlight_profile_bucket g_prof_sidecar_build{};
pointlight_profile_bucket g_prof_authority{};
pointlight_profile_bucket g_prof_prepare{};
pointlight_profile_bucket g_prof_gpu_cache{};
pointlight_profile_bucket g_prof_upload{};

thread_local std::uint32_t g_prof_producer_seq = 0u;
thread_local std::uint32_t g_prof_sidecar_seq = 0u;
thread_local std::uint32_t g_prof_authority_seq = 0u;
thread_local std::uint32_t g_prof_prepare_seq = 0u;

std::uint64_t prof_qpc() noexcept
{
    LARGE_INTEGER v{};
    QueryPerformanceCounter(&v);
    return static_cast<std::uint64_t>(v.QuadPart);
}

std::uint64_t prof_freq() noexcept
{
    static const std::uint64_t f = []() noexcept {
        LARGE_INTEGER v{};
        QueryPerformanceFrequency(&v);
        return static_cast<std::uint64_t>(v.QuadPart);
    }();
    return f;
}

void prof_add(
    pointlight_profile_bucket &bucket,
    std::uint64_t ticks) noexcept
{
    bucket.samples.fetch_add(1u, std::memory_order_relaxed);
    bucket.ticks.fetch_add(ticks, std::memory_order_relaxed);
    auto observed =
        bucket.max_ticks.load(std::memory_order_relaxed);
    while (observed < ticks &&
           !bucket.max_ticks.compare_exchange_weak(
               observed,
               ticks,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }
}

bool prof_sample(std::uint32_t &seq) noexcept
{
    return ((++seq &
             (k_pointlight_profile_sample_period - 1u)) == 0u);
}

double prof_avg_us(
    const pointlight_profile_bucket &bucket) noexcept
{
    const auto samples =
        bucket.samples.load(std::memory_order_relaxed);
    const auto frequency = prof_freq();
    if (samples == 0u || frequency == 0u)
        return 0.0;
    return
        (static_cast<double>(
             bucket.ticks.load(std::memory_order_relaxed)) *
         1000000.0) /
        (static_cast<double>(frequency) *
         static_cast<double>(samples));
}

double prof_max_us(
    const pointlight_profile_bucket &bucket) noexcept
{
    const auto frequency = prof_freq();
    if (frequency == 0u)
        return 0.0;
    return
        (static_cast<double>(
             bucket.max_ticks.load(std::memory_order_relaxed)) *
         1000000.0) /
        static_cast<double>(frequency);
}

void maybe_log_pointlight_prepare_profile() noexcept
{
    const auto samples =
        g_prof_prepare.samples.load(std::memory_order_relaxed);
    if (samples == 0u ||
        (samples != 1u && (samples % 16u) != 0u))
        return;

    char line[1024]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL PERF R32] POINTLIGHT_PREP sample=1/%u producer_us=%.3f producer_max_us=%.3f select_us=%.3f capture_sources_us=%.3f sidecar_build_us=%.3f authority_us=%.3f prepare_us=%.3f prepare_max_us=%.3f gpu_cache_us=%.3f upload_us=%.3f n=prod:%llu auth:%llu prep:%llu",
        k_pointlight_profile_sample_period,
        prof_avg_us(g_prof_producer),
        prof_max_us(g_prof_producer),
        prof_avg_us(g_prof_select),
        prof_avg_us(g_prof_capture),
        prof_avg_us(g_prof_sidecar_build),
        prof_avg_us(g_prof_authority),
        prof_avg_us(g_prof_prepare),
        prof_max_us(g_prof_prepare),
        prof_avg_us(g_prof_gpu_cache),
        prof_avg_us(g_prof_upload),
        static_cast<unsigned long long>(
            g_prof_producer.samples.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_prof_authority.samples.load(
                std::memory_order_relaxed)),
        static_cast<unsigned long long>(samples));
    reshade::log::message(
        reshade::log::level::info,
        line);
}
#endif

struct gpu_resources {
    ID3D11Device *device = nullptr;
    ID3D11Buffer *t18_buffer = nullptr;
    ID3D11ShaderResourceView *t18_srv = nullptr;
    ID3D11Buffer *t19_buffer = nullptr;
    ID3D11ShaderResourceView *t19_srv = nullptr;
    ID3D11Buffer *b12 = nullptr;
    std::uint64_t generation = 0u;
};

struct gpu_fast_cache_entry {
    ID3D11DeviceContext *context = nullptr;
    ID3D11Device *device = nullptr;
    ID3D11Buffer *t18_buffer = nullptr;
    ID3D11ShaderResourceView *t18_srv = nullptr;
    ID3D11Buffer *t19_buffer = nullptr;
    ID3D11ShaderResourceView *t19_srv = nullptr;
    ID3D11Buffer *b12 = nullptr;
    std::uint64_t generation = 0u;
    std::uint64_t epoch = 0u;

    void clear() noexcept
    {
        if (b12 != nullptr)
            b12->Release();
        if (t19_srv != nullptr)
            t19_srv->Release();
        if (t19_buffer != nullptr)
            t19_buffer->Release();
        if (t18_srv != nullptr)
            t18_srv->Release();
        if (t18_buffer != nullptr)
            t18_buffer->Release();
        if (device != nullptr)
            device->Release();

        context = nullptr;
        device = nullptr;
        t18_buffer = nullptr;
        t18_srv = nullptr;
        t19_buffer = nullptr;
        t19_srv = nullptr;
        b12 = nullptr;
        generation = 0u;
        epoch = 0u;
    }

    ~gpu_fast_cache_entry() noexcept
    {
        clear();
    }
};

thread_local std::array<gpu_fast_cache_entry,8>
    g_gpu_fast_cache{};
thread_local std::uint8_t g_gpu_fast_cache_victim = 0u;
std::atomic<std::uint64_t> g_resource_epoch{1u};

void release_gpu_fast_entry(
    gpu_fast_cache_entry &entry) noexcept
{
    entry.clear();
}

void clear_gpu_fast_cache() noexcept
{
    for (auto &entry : g_gpu_fast_cache)
        release_gpu_fast_entry(entry);
    g_gpu_fast_cache_victim = 0u;
}

gpu_fast_cache_entry *lookup_gpu_fast_cache(
    ID3D11DeviceContext *context,
    ID3D11Device *device) noexcept
{
    const auto epoch =
        g_resource_epoch.load(
            std::memory_order_acquire);

    for (auto &entry : g_gpu_fast_cache) {
        if (entry.epoch != 0u &&
            entry.epoch != epoch)
            release_gpu_fast_entry(entry);

        if (entry.context == context &&
            entry.device == device &&
            entry.epoch == epoch &&
            entry.t18_buffer != nullptr &&
            entry.t18_srv != nullptr &&
            entry.t19_buffer != nullptr &&
            entry.t19_srv != nullptr &&
            entry.b12 != nullptr)
            return &entry;
    }

    return nullptr;
}

gpu_fast_cache_entry *store_gpu_fast_cache(
    ID3D11DeviceContext *context,
    const gpu_resources &gpu) noexcept
{
    if (context == nullptr ||
        gpu.device == nullptr ||
        gpu.t18_buffer == nullptr ||
        gpu.t18_srv == nullptr ||
        gpu.t19_buffer == nullptr ||
        gpu.t19_srv == nullptr ||
        gpu.b12 == nullptr)
        return nullptr;

    auto &entry =
        g_gpu_fast_cache[
            static_cast<std::size_t>(
                g_gpu_fast_cache_victim++) %
            g_gpu_fast_cache.size()];

    release_gpu_fast_entry(entry);

    entry.context = context;
    entry.device = gpu.device;
    entry.t18_buffer = gpu.t18_buffer;
    entry.t18_srv = gpu.t18_srv;
    entry.t19_buffer = gpu.t19_buffer;
    entry.t19_srv = gpu.t19_srv;
    entry.b12 = gpu.b12;
    entry.generation = gpu.generation;
    entry.epoch =
        g_resource_epoch.load(
            std::memory_order_acquire);

    entry.device->AddRef();
    entry.t18_buffer->AddRef();
    entry.t18_srv->AddRef();
    entry.t19_buffer->AddRef();
    entry.t19_srv->AddRef();
    entry.b12->AddRef();

    return &entry;
}

struct upload_identity_tls {
    ID3D11DeviceContext *context = nullptr;
    std::uint64_t generation = 0u;
    std::array<operators::point_light::clustered_t18_record_v1,4>
        last_t18{};
    std::array<std::array<float,4>,4> last_t19{};
    operators::material_response::material_response_b12_payload
        last_b12{};
    bool last_t18_valid = false;
    bool last_t19_valid = false;
    bool last_b12_valid = false;
};

thread_local std::array<upload_identity_tls,8>
    g_upload_identity_cache{};
thread_local std::uint8_t g_upload_identity_victim = 0u;
std::atomic<std::uint64_t> g_gpu_generation{1u};

upload_identity_tls &upload_identity_for(
    ID3D11DeviceContext *context,
    std::uint64_t generation) noexcept
{
    for (auto &entry : g_upload_identity_cache) {
        if (entry.context == context &&
            entry.generation == generation)
            return entry;
    }

    auto &entry =
        g_upload_identity_cache[
            static_cast<std::size_t>(
                g_upload_identity_victim++) %
            g_upload_identity_cache.size()];
    entry = {};
    entry.context = context;
    entry.generation = generation;
    return entry;
}

std::mutex g_resource_mutex;
// Dynamic DISCARD payloads are recording-context local. A shared resource
// would let a second immediate/deferred context replace t18/t19/b12 between
// prepare and bind for the first context.
std::unordered_map<ID3D11DeviceContext *,gpu_resources>
    g_gpu_by_context{};

clustered_pnts_draw_runtime *g_runtime = nullptr;
std::atomic_bool g_enabled{false};
std::atomic_bool g_quarantined{false};
std::atomic<std::uint64_t> g_builder_seen{0u};
std::atomic<std::uint64_t> g_collection_ok{0u};
std::atomic<std::uint64_t> g_collection_fail{0u};
std::atomic<std::uint64_t> g_selector_calls{0u};
std::atomic<std::uint64_t> g_mirror_equal{0u};
std::atomic<std::uint64_t> g_mirror_diff{0u};
std::atomic<std::uint64_t> g_source_capture_ok{0u};
std::atomic<std::uint64_t> g_source_capture_fail{0u};
std::atomic<std::uint64_t> g_source_producer_hits{0u};
std::atomic<std::uint64_t> g_source_row_bridge{0u};
std::atomic<std::uint64_t> g_source_row_stock_dsr{0u};
std::atomic<std::uint64_t> g_source_dsr_only_fail_open{0u};
std::atomic<std::uint64_t> g_source_unclassified_bank_fail_open{0u};
std::atomic<std::uint64_t> g_source_class_reject{0u};
std::atomic<std::uint64_t> g_source_payload_cache_hit{0u};
std::atomic<std::uint64_t> g_source_payload_cache_miss{0u};
std::array<std::atomic<std::uint64_t>,4> g_source_category_hits{};
std::atomic<std::uint64_t> g_snapshot_publish{0u};
std::atomic<std::uint64_t> g_selector_seen{0u};
std::atomic<std::uint64_t> g_owner_join_hit{0u};
std::atomic<std::uint64_t> g_owner_join_miss{0u};
std::atomic<std::uint64_t> g_material_limit_ok{0u};
std::atomic<std::uint64_t> g_material_limit_fail{0u};
std::atomic<std::uint64_t> g_sidecar_ready{0u};
std::atomic<std::uint64_t> g_sidecar_fail{0u};
std::atomic<std::uint64_t> g_t18_create{0u};
std::atomic<std::uint64_t> g_t18_hit{0u};
std::atomic<std::uint64_t> g_t19_create{0u};
std::atomic<std::uint64_t> g_t19_hit{0u};
std::atomic<std::uint64_t> g_b12_create{0u};
std::atomic<std::uint64_t> g_b12_hit{0u};
std::atomic<std::uint64_t> g_prepare_ok{0u};
std::atomic<std::uint64_t> g_prepare_fail{0u};
std::atomic<std::uint64_t> g_prepare_neutral_empty{0u};
std::atomic<std::uint64_t> g_prepare_precondition_fail{0u};
std::atomic<std::uint64_t> g_selection_fail{0u};
std::atomic<std::uint64_t> g_selection_empty{0u};
std::atomic<std::uint64_t> g_sidecar_build_fail{0u};
std::atomic<std::uint64_t> g_context_immediate{0u};
std::atomic<std::uint64_t> g_context_deferred{0u};
std::atomic<std::uint64_t> g_context_other{0u};
std::atomic<std::uint64_t> g_gpu_prepare_fail{0u};
std::atomic<std::uint64_t> g_upload_fail{0u};
std::atomic<std::uint64_t> g_gpu_fast_hit{0u};
std::atomic<std::uint64_t> g_gpu_fast_miss{0u};

std::uintptr_t g_base = 0u;

using retained_selector_fn = int (__fastcall *)(
    void *,
    std::uint32_t *,
    int,
    const void *,
    std::uint8_t);

retained_selector_fn g_retained_selector = nullptr;

bool readable_range(
    const void *ptr,
    std::size_t size) noexcept
{
    if (ptr == nullptr)
        return false;
    if (size == 0u)
        return true;

    auto cursor =
        reinterpret_cast<std::uintptr_t>(ptr);
    const auto end = cursor + size;
    if (end < cursor)
        return false;

    while (cursor < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(
                reinterpret_cast<const void *>(cursor),
                &mbi,
                sizeof(mbi)) != sizeof(mbi))
            return false;

        if (mbi.State != MEM_COMMIT ||
            (mbi.Protect & PAGE_GUARD) != 0u)
            return false;

        const DWORD access = mbi.Protect & 0xffu;
        const bool readable =
            access == PAGE_READONLY ||
            access == PAGE_READWRITE ||
            access == PAGE_WRITECOPY ||
            access == PAGE_EXECUTE_READ ||
            access == PAGE_EXECUTE_READWRITE ||
            access == PAGE_EXECUTE_WRITECOPY;
        if (!readable)
            return false;

        const auto region_begin =
            reinterpret_cast<std::uintptr_t>(
                mbi.BaseAddress);
        const auto region_end =
            region_begin + mbi.RegionSize;
        if (region_end <= cursor ||
            region_end < region_begin)
            return false;

        cursor = std::min(region_end, end);
    }
    return true;
}

struct readable_region_cache {
    std::uintptr_t begin = 0u;
    std::uintptr_t end = 0u;
};

struct source_vm_cache_tls {
    std::uint64_t epoch = 0u;
    readable_region_cache collection{};
    readable_region_cache node{};
    readable_region_cache vtable{};
    readable_region_cache manager{};
    readable_region_cache table{};
    readable_region_cache endpoint{};
    readable_region_cache param{};
};

thread_local source_vm_cache_tls g_source_vm_cache{};

source_vm_cache_tls &source_vm_cache_current() noexcept
{
    const auto epoch =
        g_source_frame_epoch.load(
            std::memory_order_relaxed);
    if (g_source_vm_cache.epoch != epoch) {
        g_source_vm_cache = {};
        g_source_vm_cache.epoch = epoch;
    }
    return g_source_vm_cache;
}

bool readable_range_cached(
    const void *ptr,
    std::size_t size,
    readable_region_cache &cache) noexcept
{
    if (ptr == nullptr)
        return false;
    if (size == 0u)
        return true;

    const auto begin =
        reinterpret_cast<std::uintptr_t>(ptr);
    const auto end = begin + size;
    if (end < begin)
        return false;

    if (cache.begin <= begin &&
        end <= cache.end)
        return true;

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(
            ptr,
            &mbi,
            sizeof(mbi)) != sizeof(mbi) ||
        mbi.State != MEM_COMMIT ||
        (mbi.Protect & PAGE_GUARD) != 0u)
        return false;

    const DWORD access = mbi.Protect & 0xffu;
    const bool readable =
        access == PAGE_READONLY ||
        access == PAGE_READWRITE ||
        access == PAGE_WRITECOPY ||
        access == PAGE_EXECUTE_READ ||
        access == PAGE_EXECUTE_READWRITE ||
        access == PAGE_EXECUTE_WRITECOPY;
    if (!readable)
        return false;

    const auto region_begin =
        reinterpret_cast<std::uintptr_t>(
            mbi.BaseAddress);
    const auto region_end =
        region_begin + mbi.RegionSize;
    if (region_end <= begin ||
        region_end < region_begin)
        return false;

    // Cache is deliberately draw-local at the caller. If an object crosses a
    // VM region boundary, retain the original full validator instead of
    // widening authority.
    if (end > region_end)
        return readable_range(ptr, size);

    cache.begin = region_begin;
    cache.end = region_end;
    return true;
}

bool make_frame_selection_key(
    const producer_input_snapshot &input,
    frame_selection_key_v1 &key) noexcept
{
    key = {};
    if (!input.valid ||
        input.collection == nullptr)
        return false;

    auto &source_vm =
        source_vm_cache_current();
    if (!readable_range_cached(
            input.collection,
            0x90u,
            source_vm.collection))
        return false;

    key.owner = input.owner;
    key.collection = input.collection;
    key.mask = input.mask;
    std::memcpy(
        key.query_bits.data(),
        input.query.data(),
        sizeof(key.query_bits));

    const auto *base =
        static_cast<const std::uint8_t *>(
            input.collection);
    for (std::uint32_t bucket = 0u;
         bucket < 4u;
         ++bucket) {
        std::memcpy(
            &key.bucket_heads[bucket],
            base + 0x18u + bucket * 0x20u,
            sizeof(key.bucket_heads[bucket]));
    }
    return true;
}

std::atomic<std::uint64_t> g_source_capture_reason_mask{0u};

void log_source_capture_failure_once(
    std::uint64_t bit,
    const char *reason,
    void *node,
    const void *target) noexcept
{
    if ((g_source_capture_reason_mask.load(
             std::memory_order_relaxed) & bit) != 0u)
        return;

    const auto previous =
        g_source_capture_reason_mask.fetch_or(
            bit,
            std::memory_order_relaxed);
    if ((previous & bit) != 0u)
        return;

    std::uint32_t source_id = 0u;
    std::uint8_t source_category = 0u;
    if (node != nullptr &&
        readable_range(node, 0x19u)) {
        const auto *bytes =
            static_cast<const std::uint8_t *>(node);
        std::memcpy(
            &source_id,
            bytes + 0x10u,
            sizeof(source_id));
        std::memcpy(
            &source_category,
            bytes + 0x18u,
            sizeof(source_category));
    }

    char line[384]{};
    std::snprintf(
        line,
        sizeof(line),
        "[DSRRL POINTLIGHT R29] source_capture_fail reason=%s source_id=%u category=%u target=%016llx",
        reason != nullptr ? reason : "unknown",
        static_cast<unsigned>(source_id),
        static_cast<unsigned>(source_category),
        static_cast<unsigned long long>(
            reinterpret_cast<std::uintptr_t>(
                target)));
    reshade::log::message(
        reshade::log::level::warning,
        line);
}

bool executable_address(
    const void *ptr) noexcept
{
    if (ptr == nullptr)
        return false;

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(
            ptr,
            &mbi,
            sizeof(mbi)) != sizeof(mbi) ||
        mbi.State != MEM_COMMIT ||
        (mbi.Protect & PAGE_GUARD) != 0u)
        return false;

    const DWORD access = mbi.Protect & 0xffu;
    return
        access == PAGE_EXECUTE ||
        access == PAGE_EXECUTE_READ ||
        access == PAGE_EXECUTE_READWRITE ||
        access == PAGE_EXECUTE_WRITECOPY;
}

struct pointlight_collection_insert_hook_v1 {
    void *target = nullptr;
    void *trampoline = nullptr;
    std::array<std::uint8_t,16> original{};
    bool patched = false;
};

constexpr std::uintptr_t
    k_pointlight_collection_insert_rva = 0x55F750u;
constexpr std::array<std::uint8_t,16>
    k_pointlight_collection_insert_preimage = {
        0x48,0x89,0x5C,0x24,0x08,
        0x57,
        0x48,0x83,0xEC,0x20,
        0x48,0x63,0xDA,
        0x48,0x8B,0xF9
    };

pointlight_collection_insert_hook_v1
    g_pointlight_collection_insert_hook{};

using pointlight_collection_insert_fn =
    void (__fastcall *)(void *,std::int32_t,std::uint32_t);

bool write_pointlight_code(
    void *target,
    const void *bytes,
    std::size_t size) noexcept
{
    if (target == nullptr ||
        bytes == nullptr ||
        size == 0u)
        return false;

    DWORD old_protect = 0u;
    if (!VirtualProtect(
            target,
            size,
            PAGE_EXECUTE_READWRITE,
            &old_protect))
        return false;

    std::memcpy(target, bytes, size);
    const bool flushed =
        FlushInstructionCache(
            GetCurrentProcess(),
            target,
            size) != FALSE;

    DWORD ignored = 0u;
    const bool restored =
        VirtualProtect(
            target,
            size,
            old_protect,
            &ignored) != FALSE;
    return flushed && restored;
}

struct clustered_source_override_hook_v1 {
    void *target = nullptr;
    void *stub = nullptr;
    std::array<std::uint8_t,16> original{};
    bool patched = false;
};

// Exact retail DSR producer cut after DSR has computed category gain/q^2.2
// but immediately before the 48-byte t18 record is written. Replacing the
// stack-local source lanes here gives stock DSR receivers the PTDE source
// signal without receiver/material lookup and without inverse-pow work.
constexpr std::uintptr_t
    k_clustered_source_override_rva = 0xB7E02u;
constexpr std::array<std::uint8_t,16>
    k_clustered_source_override_preimage = {
        0x48,0x8B,0x83,0x20,0x03,0x00,0x00,
        0x48,0x8D,0x0C,0x76,
        0x0F,0x28,0x44,0x24,0x30
    };

clustered_source_override_hook_v1
    g_clustered_source_override_hook{};

struct clustered_source_payload_key_v1 {
    const void *source = nullptr;
    std::uintptr_t target = 0u;
    std::uintptr_t owner = 0u;
    std::uint32_t selector_word0 = 0u;
    std::uint32_t selector_word1 = 0u;
    std::uint64_t semantic_generation = 0u;
};

struct clustered_source_payload_cache_entry_v1 {
    clustered_source_payload_key_v1 key{};
    std::array<float,5> payload{};
    bool valid = false;
};

constexpr std::size_t k_clustered_source_payload_cache_entries = 256u;
static_assert(
    (k_clustered_source_payload_cache_entries &
     (k_clustered_source_payload_cache_entries - 1u)) == 0u);

thread_local std::array<
    clustered_source_payload_cache_entry_v1,
    k_clustered_source_payload_cache_entries>
    g_clustered_source_payload_cache{};

std::size_t clustered_source_payload_cache_index(
    const clustered_source_payload_key_v1 &key) noexcept
{
    std::uint64_t h =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(
                key.source)) >> 4u;
    const auto mix = [&h](std::uint64_t v) noexcept {
        h ^= v +
             0x9e3779b97f4a7c15ull +
             (h << 6u) +
             (h >> 2u);
    };
    mix(static_cast<std::uint64_t>(key.target));
    mix(static_cast<std::uint64_t>(key.owner));
    mix(static_cast<std::uint64_t>(key.selector_word0));
    mix(static_cast<std::uint64_t>(key.selector_word1));
    mix(key.semantic_generation);
    return static_cast<std::size_t>(
        h & (k_clustered_source_payload_cache_entries - 1u));
}

bool same_clustered_source_payload_key(
    const clustered_source_payload_key_v1 &a,
    const clustered_source_payload_key_v1 &b) noexcept
{
    return
        a.source == b.source &&
        a.target == b.target &&
        a.owner == b.owner &&
        a.selector_word0 == b.selector_word0 &&
        a.selector_word1 == b.selector_word1 &&
        a.semantic_generation == b.semantic_generation;
}


enum class clustered_live_source_result : std::uint8_t {
    applied = 0,
    stock_dsr_dsr_only_row,
    stock_dsr_unclassified_bank,
    stock_dsr_signature_complete_unknown,
    invalid_node_or_base,
    invalid_vtable,
    invalid_owner,
    invalid_selector,
    invalid_selector_policy,
    invalid_selected_source,
    invalid_param,
    invalid_count_or_row,
    bank_guard_name_offset_read_fail,
    bank_guard_name_offset_range_fail,
    bank_guard_name_word_read_fail,
    bank_signature_header_range_fail,
    bank_signature_table_read_fail,
    bank_signature_row_id_fail,
    bank_signature_row_offset_fail,
    bank_signature_name_offset_fail,
    bank_signature_name_read_fail,
    bank_signature_name_unterminated,
    invalid_row_read,
    invalid_row_numeric,
    invalid_mix
};

struct live_row_v1 {
    std::uint32_t begin_bits = 0u;
    std::uint32_t end_bits = 0u;
    std::int16_t r = 0;
    std::int16_t g = 0;
    std::int16_t b = 0;
    std::int16_t intensity = 0;
};
static_assert(sizeof(live_row_v1) == 16u);

constexpr bool same_live_row(
    const live_row_v1 &a,
    const live_row_v1 &b) noexcept
{
    return
        a.begin_bits == b.begin_bits &&
        a.end_bits == b.end_bits &&
        a.r == b.r &&
        a.g == b.g &&
        a.b == b.b &&
        a.intensity == b.intensity;
}

struct clustered_source_bank_fingerprint_v1 {
    clustered_pointlight_source_gate::gameplay_bank bank =
        clustered_pointlight_source_gate::gameplay_bank::unknown;
    bool known_non_ptde = false;
    live_row_v1 row0{};
};

// R43 lightweight bank authority.
//
// Under the current PARAM carrier, one exact 16-byte row0 payload is sufficient
// to determine every gate outcome. m11/m15_1/m15 intentionally share the same
// row0 payload, but all three are fully bridged and have no DSR-only semantic
// rows, so they are represented by the m11 gate class. m10/m12/m17/m18 have
// distinct row0 payloads and retain their sparse DSR-only row exceptions.
// default and m99 also have distinct row0 payloads and fail open to stock DSR.
// Unknown row0 identities fail open.
//
// This table is gate identity only. It is not a donor source and does not
// replace live DrawParam row payloads.
inline constexpr std::array<
    clustered_source_bank_fingerprint_v1, 10>
    k_clustered_source_bank_fingerprints = {{
    {clustered_pointlight_source_gate::gameplay_bank::m10, false,
        {0x00000000u,0x40A00000u,255,230,200,100}},
    // Shared full-bridge gate class for m11 / m15_1 / m15.
    {clustered_pointlight_source_gate::gameplay_bank::m11, false,
        {0x40000000u,0x40800000u,255,220,150,200}},
    {clustered_pointlight_source_gate::gameplay_bank::m12, false,
        {0x00000000u,0x40600000u,255,255,255,150}},
    {clustered_pointlight_source_gate::gameplay_bank::m13, false,
        {0x40000000u,0x41300000u,215,165,125,150}},
    {clustered_pointlight_source_gate::gameplay_bank::m14, false,
        {0x00000000u,0x41200000u,255,155,125,150}},
    {clustered_pointlight_source_gate::gameplay_bank::m16, false,
        {0x40000000u,0x40A00000u,80,140,255,160}},
    {clustered_pointlight_source_gate::gameplay_bank::m17, false,
        {0x3F333332u,0x40400000u,255,230,180,70}},
    {clustered_pointlight_source_gate::gameplay_bank::m18, false,
        {0x3F800000u,0x40E00000u,255,255,250,300}},
    {clustered_pointlight_source_gate::gameplay_bank::unknown, true,
        {0x40C00000u,0x41400000u,255,128,128,100}}, // default
    {clustered_pointlight_source_gate::gameplay_bank::unknown, true,
        {0x00000000u,0x41200000u,255,255,255,150}}  // m99
}};

constexpr bool clustered_source_bank_fingerprints_unique() noexcept
{
    for (std::size_t i = 0u;
         i < k_clustered_source_bank_fingerprints.size();
         ++i)
        for (std::size_t j = i + 1u;
             j < k_clustered_source_bank_fingerprints.size();
             ++j)
            if (same_live_row(
                    k_clustered_source_bank_fingerprints[i].row0,
                    k_clustered_source_bank_fingerprints[j].row0))
                return false;
    return true;
}
static_assert(
    clustered_source_bank_fingerprints_unique(),
    "R43 PointLight row0 gate fingerprints must remain unique.");

struct clustered_source_bank_identity_cache_entry_v1 {
    std::uintptr_t param = 0u;
    std::uint16_t count = 0u;
    std::uint32_t row0_offset = 0u;
    live_row_v1 row0{};
    clustered_pointlight_source_gate::gameplay_bank bank =
        clustered_pointlight_source_gate::gameplay_bank::unknown;
    bool known_non_ptde = false;
    bool valid = false;
};

constexpr std::size_t k_clustered_source_bank_identity_cache_entries = 16u;
static_assert(
    (k_clustered_source_bank_identity_cache_entries &
     (k_clustered_source_bank_identity_cache_entries - 1u)) == 0u);
thread_local std::array<
    clustered_source_bank_identity_cache_entry_v1,
    k_clustered_source_bank_identity_cache_entries>
    g_clustered_source_bank_identity_cache{};

std::size_t clustered_source_bank_identity_cache_index(
    std::uintptr_t param) noexcept
{
    return static_cast<std::size_t>(
        (param >> 4u) &
        (k_clustered_source_bank_identity_cache_entries - 1u));
}

clustered_live_source_result classify_clustered_source_bank(
    std::uintptr_t param,
    std::uint16_t count,
    std::uint32_t first,
    pointlight_ptde_source::access_cache &cache,
    clustered_pointlight_source_gate::gameplay_bank &bank,
    bool &known_non_ptde) noexcept
{
    using namespace clustered_pointlight_source_gate;
    bank = gameplay_bank::unknown;
    known_non_ptde = false;

    if (count != 64u ||
        first < 0x30u ||
        first > 0x100000u)
        return clustered_live_source_result::
            bank_signature_header_range_fail;

    live_row_v1 row0{};
    if (!pointlight_ptde_source::read_cached(
            param + first,
            row0,
            cache))
        return clustered_live_source_result::
            bank_signature_row_offset_fail;

    auto &cached =
        g_clustered_source_bank_identity_cache[
            clustered_source_bank_identity_cache_index(
                param)];
    if (cached.valid &&
        cached.param == param &&
        cached.count == count &&
        cached.row0_offset == first &&
        same_live_row(cached.row0, row0)) {
        bank = cached.bank;
        known_non_ptde = cached.known_non_ptde;
        return clustered_live_source_result::applied;
    }

    for (const auto &fingerprint :
         k_clustered_source_bank_fingerprints) {
        if (!same_live_row(
                fingerprint.row0,
                row0))
            continue;

        bank = fingerprint.bank;
        known_non_ptde =
            fingerprint.known_non_ptde;

        cached = {};
        cached.param = param;
        cached.count = count;
        cached.row0_offset = first;
        cached.row0 = row0;
        cached.bank = bank;
        cached.known_non_ptde =
            known_non_ptde;
        cached.valid = true;
        return clustered_live_source_result::applied;
    }

    return clustered_live_source_result::
        stock_dsr_signature_complete_unknown;
}

clustered_live_source_result read_clustered_live_drawparam_row(
    std::uintptr_t source,
    std::int32_t selector,
    pointlight_ptde_source::signal &out,
    pointlight_ptde_source::access_cache &cache) noexcept
{
    using namespace clustered_pointlight_source_gate;

    if (selector < 0 || source == 0u)
        return clustered_live_source_result::invalid_selector;

    std::uintptr_t param = 0u;
    if (!pointlight_ptde_source::read_cached(
            source + 0x18u,
            param,
            cache) ||
        param == 0u ||
        !pointlight_ptde_source::readable_cached(
            param,
            0x330u,
            cache))
        return clustered_live_source_result::invalid_param;

    std::uint16_t count = 0u;
    std::uint32_t first = 0u;
    if (!pointlight_ptde_source::read_cached(
            param + 0x0Au,
            count,
            cache) ||
        !pointlight_ptde_source::read_cached(
            param + 0x34u,
            first,
            cache))
        return clustered_live_source_result::invalid_count_or_row;

    const auto row_id =
        static_cast<std::uint32_t>(selector) & 0xFFu;
    if (count != 64u || row_id >= count)
        return clustered_live_source_result::invalid_count_or_row;

    gameplay_bank bank = gameplay_bank::unknown;
    bool known_non_ptde = false;
    const auto structure_result =
        classify_clustered_source_bank(
            param,
            count,
            first,
            cache,
            bank,
            known_non_ptde);
    if (structure_result !=
        clustered_live_source_result::applied)
        return structure_result;

    if (known_non_ptde)
        return clustered_live_source_result::
            stock_dsr_unclassified_bank;
    if (bank == gameplay_bank::unknown)
        return clustered_live_source_result::
            stock_dsr_signature_complete_unknown;

    if (dsr_only_semantic_row(bank, row_id))
        return clustered_live_source_result::
            stock_dsr_dsr_only_row;

    std::uint32_t row_offset = 0u;
    const auto row_offset_address =
        param + 0x34u +
        static_cast<std::uintptr_t>(row_id) * 12u;
    if (!pointlight_ptde_source::read_cached(
            row_offset_address,
            row_offset,
            cache) ||
        row_offset < 0x30u ||
        row_offset > 0x100000u)
        return clustered_live_source_result::invalid_row_read;

    live_row_v1 row{};
    const auto row_address =
        param +
        static_cast<std::uintptr_t>(row_offset);
    if (!pointlight_ptde_source::read_cached(
            row_address,
            row,
            cache))
        return clustered_live_source_result::invalid_row_read;

    std::memcpy(
        &out.begin,
        &row.begin_bits,
        sizeof(out.begin));
    std::memcpy(
        &out.end,
        &row.end_bits,
        sizeof(out.end));

    const float intensity =
        static_cast<float>(row.intensity) * 0.01f;
    out.q = {
        static_cast<float>(row.r) *
            intensity / 255.0f,
        static_cast<float>(row.g) *
            intensity / 255.0f,
        static_cast<float>(row.b) *
            intensity / 255.0f
    };

    if (!std::isfinite(out.begin) ||
        !std::isfinite(out.end) ||
        !std::isfinite(out.q[0]) ||
        !std::isfinite(out.q[1]) ||
        !std::isfinite(out.q[2]) ||
        !(out.end > out.begin) ||
        !(out.end > 0.0f))
        return clustered_live_source_result::invalid_row_numeric;

    return clustered_live_source_result::applied;
}

bool selected_clustered_source_retail(
    std::uintptr_t manager,
    std::int16_t selector,
    std::uintptr_t &source,
    pointlight_ptde_source::access_cache &cache) noexcept
{
    source = 0u;
    if (manager == 0u)
        return false;

    const auto lookup =
        [&](unsigned area) noexcept {
            std::uintptr_t table = 0u;
            std::uintptr_t candidate = 0u;
            if (!pointlight_ptde_source::read_cached(
                    manager + 0x20u +
                        static_cast<std::uintptr_t>(area) * 0x1B0u,
                    table,
                    cache) ||
                table == 0u ||
                !pointlight_ptde_source::read_cached(
                    table + 9u * 0x10u + 8u,
                    candidate,
                    cache))
                return;
            source = candidate;
        };

    const auto area =
        selector < 0
            ? 0xffffffffu
            : (static_cast<unsigned>(
                   static_cast<std::uint16_t>(selector)) >> 8u) & 0x7fu;

    // Exact 0x14055D0B0 / 0x140569B40 behavior: try the selected area first,
    // then common area 11 whenever the first resolver returns nullptr.
    if (area <= 11u)
        lookup(area);
    if (source == 0u)
        lookup(11u);
    return source != 0u;
}

clustered_live_source_result capture_clustered_live_drawparam_source(
    void *node,
    std::uintptr_t base,
    std::array<float,8> &raw) noexcept
{
    if (node == nullptr || base == 0u)
        return clustered_live_source_result::invalid_node_or_base;

    const auto n =
        reinterpret_cast<std::uintptr_t>(node);
    pointlight_ptde_source::access_cache cache{};

    std::uintptr_t vtable = 0u;
    std::uintptr_t fn = 0u;
    std::uintptr_t owner = 0u;
    if (!pointlight_ptde_source::read_cached(
            n,
            vtable,
            cache) ||
        vtable == 0u ||
        !pointlight_ptde_source::read_cached(
            vtable + 0x60u,
            fn,
            cache))
        return clustered_live_source_result::invalid_vtable;
    if (!pointlight_ptde_source::read_cached(
            n + 0x50u,
            owner,
            cache) ||
        owner == 0u)
        return clustered_live_source_result::invalid_owner;

    pointlight_ptde_source::signal a{}, b{}, result{};

    if (fn == base + 0x55BC00u) {
        std::int32_t selector = -1;
        if (!pointlight_ptde_source::read_cached(
                n + 0x58u,
                selector,
                cache))
            return clustered_live_source_result::invalid_selector;

        const auto row_result =
            read_clustered_live_drawparam_row(
                owner,
                selector,
                a,
                cache);
        if (row_result !=
            clustered_live_source_result::applied)
            return row_result;

        if (!pointlight_ptde_source::mix(
                a,
                a,
                0.0f,
                result))
            return clustered_live_source_result::invalid_mix;
    } else if (fn == base + 0x55D0B0u) {
        std::int16_t selector_a = -1;
        std::int16_t selector_b = -1;
        float beta = 0.0f;
        if (!pointlight_ptde_source::read_cached(
                n + 0x58u,
                selector_a,
                cache) ||
            !pointlight_ptde_source::read_cached(
                n + 0x5Au,
                selector_b,
                cache) ||
            !pointlight_ptde_source::read_cached(
                n + 0x5Cu,
                beta,
                cache))
            return clustered_live_source_result::invalid_selector;

        const auto pair =
            pmetal_selector_policy::select(
                selector_a,
                selector_b,
                beta);
        if (!pair.valid)
            return clustered_live_source_result::invalid_selector_policy;

        std::uintptr_t source_a = 0u;
        if (!selected_clustered_source_retail(
                owner,
                pair.a,
                source_a,
                cache))
            return clustered_live_source_result::invalid_selected_source;

        const auto row_a =
            read_clustered_live_drawparam_row(
                source_a,
                pair.a,
                a,
                cache);
        if (row_a !=
            clustered_live_source_result::applied)
            return row_a;

        b = a;
        if (pair.beta != 0.0f) {
            std::uintptr_t source_b = 0u;
            if (!selected_clustered_source_retail(
                    owner,
                    pair.b,
                    source_b,
                    cache))
                return clustered_live_source_result::invalid_selected_source;

            const auto row_b =
                read_clustered_live_drawparam_row(
                    source_b,
                    pair.b,
                    b,
                    cache);
            if (row_b !=
                clustered_live_source_result::applied)
                return row_b;
        }

        if (!pointlight_ptde_source::mix(
                a,
                b,
                pair.beta,
                result))
            return clustered_live_source_result::invalid_mix;
    } else {
        return clustered_live_source_result::invalid_vtable;
    }

    raw[3] = 1.0f / (result.end - result.begin);
    raw[4] = result.q[0];
    raw[5] = result.q[1];
    raw[6] = result.q[2];
    raw[7] = result.end;

    return std::isfinite(raw[3]) &&
           raw[3] > 0.0f
        ? clustered_live_source_result::applied
        : clustered_live_source_result::invalid_row_numeric;
}

void source_hook_emit_u64(
    std::vector<std::uint8_t> &out,
    std::uint64_t value)
{
    for (unsigned i = 0u; i < 8u; ++i)
        out.push_back(
            static_cast<std::uint8_t>(
                value >> (i * 8u)));
}

void source_hook_emit(
    std::vector<std::uint8_t> &out,
    std::initializer_list<std::uint8_t> bytes)
{
    out.insert(
        out.end(),
        bytes.begin(),
        bytes.end());
}

void source_hook_store_xmm(
    std::vector<std::uint8_t> &out,
    std::uint8_t xmm,
    std::uint32_t disp)
{
    source_hook_emit(
        out,
        {0xF3,0x0F,0x7F,
         static_cast<std::uint8_t>(
             0x84u | ((xmm & 7u) << 3u)),
         0x24});
    for (unsigned i = 0u; i < 4u; ++i)
        out.push_back(
            static_cast<std::uint8_t>(
                disp >> (i * 8u)));
}

void source_hook_load_xmm(
    std::vector<std::uint8_t> &out,
    std::uint8_t xmm,
    std::uint32_t disp)
{
    source_hook_emit(
        out,
        {0xF3,0x0F,0x6F,
         static_cast<std::uint8_t>(
             0x84u | ((xmm & 7u) << 3u)),
         0x24});
    for (unsigned i = 0u; i < 4u; ++i)
        out.push_back(
            static_cast<std::uint8_t>(
                disp >> (i * 8u)));
}

float __fastcall clustered_source_override_callback(
    void *source,
    float *geometry,
    float *color_end,
    std::uint32_t *ptde_marker) noexcept
{
    // R44 row-aware consumer carrier. Retail writes only t18 +0x00..+0x24
    // for the 48-byte record at this producer cut, so +0x28 is reserved for
    // the bridge marker. Clear first on every producer visit so every
    // fail-open path remains stock DSR even when the backing t18 allocation
    // is recycled from a previously bridged light.
    if (ptde_marker != nullptr)
        *ptde_marker = 0u;

    if (!g_enabled.load(std::memory_order_relaxed) ||
        g_quarantined.load(std::memory_order_relaxed) ||
        source == nullptr ||
        geometry == nullptr ||
        color_end == nullptr ||
        ptde_marker == nullptr ||
        g_base == 0u)
        return 0.0f;

    telemetry::hot_count(g_source_producer_hits);
    std::uint8_t source_category = 0xffu;
    std::memcpy(
        &source_category,
        static_cast<const std::uint8_t *>(source) + 0x18u,
        sizeof(source_category));
    if (source_category < g_source_category_hits.size())
        telemetry::hot_count(
            g_source_category_hits[source_category]);

    // This callback executes inside the exact attested retail source producer,
    // so the source object and its vtable are already live. Build a tiny
    // semantic key directly from source identity + bank/lerp selector state.
    // Deliberately exclude world position: movement changes geometry.xyz, not
    // the selected DrawParam row payload {invRange, RGB, End}.
    // Dynamic source movement no longer invalidates it.
    void **vtable = nullptr;
    std::memcpy(
        &vtable,
        source,
        sizeof(vtable));
    if (vtable == nullptr)
        return 0.0f;

    void *target = nullptr;
    std::memcpy(
        &target,
        vtable + 12u,
        sizeof(target));
    const auto target_address =
        reinterpret_cast<std::uintptr_t>(
            target);
    if (target_address != g_base + 0x55BC00u &&
        target_address != g_base + 0x55D0B0u) {
        telemetry::hot_count(g_source_class_reject);
        if (target_address == g_base + 0x55C570u) {
            log_source_capture_failure_once(
                1u << 8,
                "source_class_direct_stock_dsr",
                source,
                target);
        } else {
            log_source_capture_failure_once(
                1u << 9,
                "source_class_other_stock_dsr",
                source,
                target);
        }
        return 0.0f;
    }

    clustered_source_payload_key_v1 key{};
    key.source = source;
    key.target = target_address;
    std::memcpy(
        &key.owner,
        static_cast<const std::uint8_t *>(
            source) + 0x50u,
        sizeof(key.owner));
    std::memcpy(
        &key.selector_word0,
        static_cast<const std::uint8_t *>(
            source) + 0x58u,
        sizeof(key.selector_word0));
    std::memcpy(
        &key.selector_word1,
        static_cast<const std::uint8_t *>(
            source) + 0x5Cu,
        sizeof(key.selector_word1));
    key.semantic_generation =
        g_source_semantic_generation.load(
            std::memory_order_acquire);

    if (key.owner == 0u)
        return 0.0f;

    const auto cache_index =
        clustered_source_payload_cache_index(
            key);
    auto &cached =
        g_clustered_source_payload_cache[
            cache_index];

    std::array<float,5> payload{};
    if (cached.valid &&
        same_clustered_source_payload_key(
            cached.key,
            key)) {
        telemetry::hot_count(
            g_source_payload_cache_hit);
        payload = cached.payload;
    } else {
        telemetry::hot_count(
            g_source_payload_cache_miss);
        std::array<float,8> candidate{};
        std::memcpy(
            candidate.data(),
            geometry,
            4u * sizeof(float));
        std::memcpy(
            candidate.data() + 4u,
            color_end,
            4u * sizeof(float));

        // R52 source-only architecture: consume the exact row already
        // selected by stock DSR from the currently loaded DrawParam. The mod
        // supplies PTDE values in those homologous gameplay rows, so runtime
        // needs no donor table and no material/receiver identity. Rows that
        // are DSR-only, or banks without PTDE authority (default/m99), fail
        // open before any source carrier is changed.
        const auto live_result =
            capture_clustered_live_drawparam_source(
                source,
                g_base,
                candidate);
        if (live_result !=
            clustered_live_source_result::applied) {
            telemetry::hot_count(
                g_source_row_stock_dsr);
            if (live_result ==
                clustered_live_source_result::
                    stock_dsr_dsr_only_row) {
                telemetry::hot_count(
                    g_source_dsr_only_fail_open);
            } else if (live_result ==
                       clustered_live_source_result::
                           stock_dsr_unclassified_bank ||
                       live_result ==
                       clustered_live_source_result::
                           stock_dsr_signature_complete_unknown) {
                telemetry::hot_count(
                    g_source_unclassified_bank_fail_open);
                if (live_result ==
                    clustered_live_source_result::
                        stock_dsr_signature_complete_unknown) {
                    log_source_capture_failure_once(
                        1ull << 24,
                        "signature_complete_but_unknown",
                        source,
                        target);
                }
            } else {
                telemetry::hot_count(
                    g_source_capture_fail);
                std::uint64_t bit = 1ull << 23;
                const char *reason = "live_drawparam_unknown";
                switch (live_result) {
                case clustered_live_source_result::invalid_node_or_base:
                    bit = 1u << 10; reason = "live_node_or_base"; break;
                case clustered_live_source_result::invalid_vtable:
                    bit = 1u << 11; reason = "live_vtable"; break;
                case clustered_live_source_result::invalid_owner:
                    bit = 1u << 12; reason = "live_owner"; break;
                case clustered_live_source_result::invalid_selector:
                    bit = 1u << 13; reason = "live_selector"; break;
                case clustered_live_source_result::invalid_selector_policy:
                    bit = 1u << 14; reason = "live_selector_policy"; break;
                case clustered_live_source_result::invalid_selected_source:
                    bit = 1u << 15; reason = "live_selected_source"; break;
                case clustered_live_source_result::invalid_param:
                    bit = 1u << 16; reason = "live_param"; break;
                case clustered_live_source_result::invalid_count_or_row:
                    bit = 1ull << 17; reason = "live_count_or_row"; break;
                case clustered_live_source_result::bank_guard_name_offset_read_fail:
                    bit = 1ull << 25; reason = "bank_guard_name_offset_read_fail"; break;
                case clustered_live_source_result::bank_guard_name_offset_range_fail:
                    bit = 1ull << 26; reason = "bank_guard_name_offset_range_fail"; break;
                case clustered_live_source_result::bank_guard_name_word_read_fail:
                    bit = 1ull << 27; reason = "bank_guard_name_word_read_fail"; break;
                case clustered_live_source_result::bank_signature_header_range_fail:
                    bit = 1ull << 28; reason = "signature_header_range_fail"; break;
                case clustered_live_source_result::bank_signature_table_read_fail:
                    bit = 1ull << 29; reason = "signature_table_read_fail"; break;
                case clustered_live_source_result::bank_signature_row_id_fail:
                    bit = 1ull << 30; reason = "signature_row_id_fail"; break;
                case clustered_live_source_result::bank_signature_row_offset_fail:
                    bit = 1ull << 31; reason = "signature_row_offset_fail"; break;
                case clustered_live_source_result::bank_signature_name_offset_fail:
                    bit = 1ull << 32; reason = "signature_name_offset_fail"; break;
                case clustered_live_source_result::bank_signature_name_read_fail:
                    bit = 1ull << 33; reason = "signature_name_read_fail"; break;
                case clustered_live_source_result::bank_signature_name_unterminated:
                    bit = 1ull << 34; reason = "signature_name_unterminated"; break;
                case clustered_live_source_result::invalid_row_read:
                    bit = 1ull << 19; reason = "live_row_read"; break;
                case clustered_live_source_result::invalid_row_numeric:
                    bit = 1ull << 20; reason = "live_row_numeric"; break;
                case clustered_live_source_result::invalid_mix:
                    bit = 1ull << 21; reason = "live_mix"; break;
                default:
                    break;
                }
                log_source_capture_failure_once(
                    bit,
                    reason,
                    source,
                    target);
            }
            return 0.0f;
        }
        telemetry::hot_count(
            g_source_row_bridge);

        for (std::size_t i = 3u;
             i < candidate.size();
             ++i)
            if (!std::isfinite(candidate[i]))
                return 0.0f;
        if (!(candidate[3] > 0.0f) ||
            !(candidate[7] > 0.0f))
            return 0.0f;

        for (std::size_t i = 0u;
             i < payload.size();
             ++i)
            payload[i] =
                candidate[3u + i];

        cached.key = key;
        cached.payload = payload;
        cached.valid = true;
    }

    // Preserve stock DSR position.xyz and all category metadata/falloff-mode
    // lanes. Replace only the source carrier that PTDE owns.
    geometry[3] = payload[0];
    std::memcpy(
        color_end,
        payload.data() + 1u,
        4u * sizeof(float));

    // Marker is published only after the exact live PTDE row has survived
    // every source validation and the carrier payload has been committed.
    // 1 = use PTDE PointLight local operator; 0 = preserve stock DSR.
    *ptde_marker = 0x3F800000u; // IEEE-754 1.0f

    telemetry::hot_count(
        g_source_capture_ok);
    return 1.0f;
}

bool build_clustered_source_override_stub(
    void *target,
    void *&stub_out) noexcept
{
    stub_out = nullptr;
    try {
        std::vector<std::uint8_t> code;
        code.reserve(352u);

        // Site RSP is 16-byte aligned. Preserve flags, all volatile GPRs and
        // volatile XMM0..5. XMM6..15 are nonvolatile under Win64 and retain
        // the retail producer's category/falloff state across the callback.
        source_hook_emit(code,{0x9C}); // pushfq
        source_hook_emit(
            code,
            {0x48,0x81,0xEC,0x08,0x01,0x00,0x00}); // sub rsp,108h

        source_hook_emit(code,{0x48,0x89,0x44,0x24,0x20}); // rax
        source_hook_emit(code,{0x9C});
        source_hook_emit(code,{0x58});
        source_hook_emit(
            code,
            {0x48,0x89,0x84,0x24,0x58,0x00,0x00,0x00});
        source_hook_emit(code,{0x48,0x89,0x4C,0x24,0x28}); // rcx
        source_hook_emit(code,{0x48,0x89,0x54,0x24,0x30}); // rdx
        source_hook_emit(code,{0x4C,0x89,0x44,0x24,0x38}); // r8
        source_hook_emit(code,{0x4C,0x89,0x4C,0x24,0x40}); // r9
        source_hook_emit(code,{0x4C,0x89,0x54,0x24,0x48}); // r10
        source_hook_emit(code,{0x4C,0x89,0x5C,0x24,0x50}); // r11

        for (std::uint8_t i = 0u; i < 6u; ++i)
            source_hook_store_xmm(
                code,
                i,
                0x60u + 0x10u * i);

        // callback(source=RDI,
        //          geometry=original_rsp+0x20,
        //          color_end=original_rsp+0x30,
        //          ptde_marker=t18_record+0x28)
        source_hook_emit(code,{0x48,0x8B,0xCF}); // mov rcx,rdi
        source_hook_emit(
            code,
            {0x48,0x8D,0x94,0x24,0x30,0x01,0x00,0x00}); // lea rdx,[rsp+130h]
        source_hook_emit(
            code,
            {0x4C,0x8D,0x84,0x24,0x40,0x01,0x00,0x00}); // lea r8,[rsp+140h]

        // Retail immediately writes this same 48-byte t18 record after the
        // callback. Compute its unused +0x28 lane from RBX+0x320 and RSI.
        // rax is volatile and already preserved by the stub.
        source_hook_emit(
            code,
            {0x4C,0x8B,0x8B,0x20,0x03,0x00,0x00}); // mov r9,[rbx+320h]
        source_hook_emit(
            code,
            {0x48,0x8D,0x04,0x76}); // lea rax,[rsi+rsi*2]
        source_hook_emit(
            code,
            {0x48,0x01,0xC0}); // add rax,rax => 6*rsi
        source_hook_emit(
            code,
            {0x4D,0x8D,0x4C,0xC1,0x28}); // lea r9,[r9+rax*8+28h]

        source_hook_emit(code,{0x48,0xB8});
        source_hook_emit_u64(
            code,
            reinterpret_cast<std::uint64_t>(
                &clustered_source_override_callback));
        source_hook_emit(code,{0xFF,0xD0}); // call rax

        // R44 row-aware carrier: the callback returns 1.0f only when the
        // selected live DrawParam row has PTDE authority. Direct sources,
        // the 10 DSR-only semantic rows, default/m99 and every fail-open
        // return 0.0f. The retail record stride is 48 bytes and retail writes
        // only +0x00..+0x27, so +0x28 is a free per-light carrier consumed
        // only by the exact R44 PntS replacement shader.
        //
        //   record = [rbx+0x320] + 48*rsi
        //   record+0x28 = PTDE attenuation authority (0.0f / 1.0f)
        //
        // RBX/RSI are Win64 nonvolatile registers and therefore still hold
        // the retail producer state across the callback.
        source_hook_emit(
            code,
            {0x48,0x8B,0x83,0x20,0x03,0x00,0x00}); // mov rax,[rbx+320h]
        source_hook_emit(
            code,
            {0x48,0x8D,0x0C,0x76}); // lea rcx,[rsi+rsi*2]
        source_hook_emit(
            code,
            {0x48,0x03,0xC9}); // add rcx,rcx => 6*rsi
        source_hook_emit(
            code,
            {0xF3,0x0F,0x11,0x44,0xC8,0x28}); // movss [rax+rcx*8+28h],xmm0

        for (std::uint8_t i = 0u; i < 6u; ++i)
            source_hook_load_xmm(
                code,
                i,
                0x60u + 0x10u * i);

        source_hook_emit(code,{0x4C,0x8B,0x5C,0x24,0x50});
        source_hook_emit(code,{0x4C,0x8B,0x54,0x24,0x48});
        source_hook_emit(code,{0x4C,0x8B,0x4C,0x24,0x40});
        source_hook_emit(code,{0x4C,0x8B,0x44,0x24,0x38});
        source_hook_emit(code,{0x48,0x8B,0x54,0x24,0x30});
        source_hook_emit(code,{0x48,0x8B,0x4C,0x24,0x28});
        source_hook_emit(
            code,
            {0x48,0x8B,0x84,0x24,0x58,0x00,0x00,0x00});
        source_hook_emit(code,{0x50});
        source_hook_emit(code,{0x9D});
        source_hook_emit(code,{0x48,0x8B,0x44,0x24,0x20});
        source_hook_emit(
            code,
            {0x48,0x81,0xC4,0x08,0x01,0x00,0x00});
        source_hook_emit(code,{0x9D});

        // Replay the exact 16 stolen retail bytes.
        code.insert(
            code.end(),
            k_clustered_source_override_preimage.begin(),
            k_clustered_source_override_preimage.end());

        // Absolute indirect jump to target+16.
        source_hook_emit(
            code,
            {0xFF,0x25,0x00,0x00,0x00,0x00});
        source_hook_emit_u64(
            code,
            reinterpret_cast<std::uint64_t>(
                static_cast<std::uint8_t *>(target) +
                k_clustered_source_override_preimage.size()));

        void *stub =
            VirtualAlloc(
                nullptr,
                code.size(),
                MEM_COMMIT | MEM_RESERVE,
                PAGE_EXECUTE_READWRITE);
        if (stub == nullptr)
            return false;

        std::memcpy(
            stub,
            code.data(),
            code.size());
        if (FlushInstructionCache(
                GetCurrentProcess(),
                stub,
                code.size()) == FALSE) {
            VirtualFree(
                stub,
                0u,
                MEM_RELEASE);
            return false;
        }

        stub_out = stub;
        return true;
    } catch (...) {
        return false;
    }
}

bool install_clustered_source_override_hook() noexcept
{
    auto &hook =
        g_clustered_source_override_hook;
    if (hook.patched)
        return true;
    if (g_base == 0u)
        return false;

    auto *target =
        reinterpret_cast<std::uint8_t *>(
            g_base +
            k_clustered_source_override_rva);
    if (!readable_range(
            target,
            k_clustered_source_override_preimage.size()) ||
        std::memcmp(
            target,
            k_clustered_source_override_preimage.data(),
            k_clustered_source_override_preimage.size()) != 0)
        return false;

    void *stub = nullptr;
    if (!build_clustered_source_override_stub(
            target,
            stub))
        return false;

    std::array<
        std::uint8_t,
        k_clustered_source_override_preimage.size()> patch{};
    patch.fill(0x90u);
    patch[0] = 0xFFu;
    patch[1] = 0x25u;
    const std::uint32_t zero = 0u;
    std::memcpy(
        patch.data() + 2u,
        &zero,
        sizeof(zero));
    const auto address =
        reinterpret_cast<std::uint64_t>(
            stub);
    std::memcpy(
        patch.data() + 6u,
        &address,
        sizeof(address));

    if (!write_pointlight_code(
            target,
            patch.data(),
            patch.size())) {
        VirtualFree(
            stub,
            0u,
            MEM_RELEASE);
        return false;
    }

    hook.target = target;
    hook.stub = stub;
    hook.original =
        k_clustered_source_override_preimage;
    hook.patched = true;
    return true;
}

bool restore_clustered_source_override_hook() noexcept
{
    auto &hook =
        g_clustered_source_override_hook;
    bool ok = true;

    if (hook.patched) {
        ok =
            hook.target != nullptr &&
            write_pointlight_code(
                hook.target,
                hook.original.data(),
                hook.original.size()) &&
            std::memcmp(
                hook.target,
                hook.original.data(),
                hook.original.size()) == 0;
        if (ok)
            hook.patched = false;
    }

    if (ok && hook.stub != nullptr) {
        ok =
            VirtualFree(
                hook.stub,
                0u,
                MEM_RELEASE) != FALSE;
        if (ok)
            hook.stub = nullptr;
    }

    if (ok)
        hook = {};
    return ok;
}

void __fastcall pointlight_collection_insert_detour(
    void *collection,
    std::int32_t category,
    std::uint32_t source_id) noexcept
{
    // Insertion is the lifetime/membership mutation that can make a recycled
    // source pointer authoritative again. Invalidate before calling retail;
    // a rejected insertion only causes a harmless extra generation change.
    invalidate_source_semantic_generation();

    const auto original =
        reinterpret_cast<pointlight_collection_insert_fn>(
            g_pointlight_collection_insert_hook.trampoline);
    if (original != nullptr)
        original(
            collection,
            category,
            source_id);
}

bool install_pointlight_collection_insert_hook() noexcept
{
    auto &hook =
        g_pointlight_collection_insert_hook;
    if (hook.patched)
        return true;
    if (g_base == 0u)
        return false;

    auto *target =
        reinterpret_cast<std::uint8_t *>(
            g_base +
            k_pointlight_collection_insert_rva);
    if (!readable_range(
            target,
            k_pointlight_collection_insert_preimage.size()) ||
        std::memcmp(
            target,
            k_pointlight_collection_insert_preimage.data(),
            k_pointlight_collection_insert_preimage.size()) != 0)
        return false;

    constexpr std::size_t stolen =
        k_pointlight_collection_insert_preimage.size();
    auto *trampoline =
        static_cast<std::uint8_t *>(
            VirtualAlloc(
                nullptr,
                stolen + 14u,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_EXECUTE_READWRITE));
    if (trampoline == nullptr)
        return false;

    std::memcpy(
        trampoline,
        target,
        stolen);
    auto *tail =
        trampoline + stolen;
    tail[0] = 0xffu;
    tail[1] = 0x25u;
    std::uint32_t zero = 0u;
    std::memcpy(tail + 2u, &zero, sizeof(zero));
    const auto continuation =
        reinterpret_cast<std::uint64_t>(
            target + stolen);
    std::memcpy(
        tail + 6u,
        &continuation,
        sizeof(continuation));

    if (FlushInstructionCache(
            GetCurrentProcess(),
            trampoline,
            stolen + 14u) == FALSE) {
        VirtualFree(
            trampoline,
            0u,
            MEM_RELEASE);
        return false;
    }

    std::array<std::uint8_t,stolen> patch{};
    patch.fill(0x90u);
    patch[0] = 0xffu;
    patch[1] = 0x25u;
    std::memcpy(
        patch.data() + 2u,
        &zero,
        sizeof(zero));
    const auto detour =
        reinterpret_cast<std::uint64_t>(
            &pointlight_collection_insert_detour);
    std::memcpy(
        patch.data() + 6u,
        &detour,
        sizeof(detour));

    if (!write_pointlight_code(
            target,
            patch.data(),
            patch.size())) {
        VirtualFree(
            trampoline,
            0u,
            MEM_RELEASE);
        return false;
    }

    hook.target = target;
    hook.trampoline = trampoline;
    hook.original =
        k_pointlight_collection_insert_preimage;
    hook.patched = true;
    return true;
}

bool restore_pointlight_collection_insert_hook() noexcept
{
    auto &hook =
        g_pointlight_collection_insert_hook;
    bool ok = true;

    if (hook.patched) {
        ok =
            hook.target != nullptr &&
            write_pointlight_code(
                hook.target,
                hook.original.data(),
                hook.original.size()) &&
            std::memcmp(
                hook.target,
                hook.original.data(),
                hook.original.size()) == 0;
        if (ok)
            hook.patched = false;
    }

    if (ok &&
        hook.trampoline != nullptr) {
        ok =
            VirtualFree(
                hook.trampoline,
                0u,
                MEM_RELEASE) != FALSE;
        if (ok)
            hook.trampoline = nullptr;
    }

    if (ok)
        hook = {};
    return ok;
}

bool selected_lerp_endpoint_cached(
    std::uintptr_t manager,
    std::int16_t selector,
    std::uintptr_t &source) noexcept
{
    source = 0u;
    if (manager == 0u)
        return false;

    auto &vm = source_vm_cache_current();
    const auto area =
        selector < 0
            ? 0xffffffffu
            : (static_cast<unsigned>(
                   static_cast<std::uint16_t>(
                       selector)) >> 8u) & 127u;

    const auto lookup =
        [&](unsigned index) noexcept -> bool {
            const auto slot =
                manager + 0x20u +
                static_cast<std::uintptr_t>(
                    index) * 0x1b0u;
            if (!readable_range_cached(
                    reinterpret_cast<const void *>(slot),
                    sizeof(std::uintptr_t),
                    vm.manager))
                return false;

            std::uintptr_t table = 0u;
            std::memcpy(
                &table,
                reinterpret_cast<const void *>(slot),
                sizeof(table));
            if (table == 0u)
                return true;

            const auto endpoint_slot =
                table + 9u * 0x10u + 8u;
            if (!readable_range_cached(
                    reinterpret_cast<const void *>(
                        endpoint_slot),
                    sizeof(std::uintptr_t),
                    vm.table))
                return false;

            std::memcpy(
                &source,
                reinterpret_cast<const void *>(
                    endpoint_slot),
                sizeof(source));
            return true;
        };

    if (area <= 11u &&
        !lookup(area))
        return false;
    if (source == 0u &&
        !lookup(11u))
        return false;
    return source != 0u;
}

bool capture_endpoint_param_identity(
    std::uintptr_t endpoint_source,
    std::uintptr_t &param,
    std::uint16_t &count,
    std::uint32_t &first) noexcept
{
    param = 0u;
    count = 0u;
    first = 0u;
    if (endpoint_source == 0u)
        return false;

    auto &vm = source_vm_cache_current();
    const auto param_slot =
        endpoint_source + 0x18u;
    if (!readable_range_cached(
            reinterpret_cast<const void *>(
                param_slot),
            sizeof(std::uintptr_t),
            vm.endpoint))
        return false;

    std::memcpy(
        &param,
        reinterpret_cast<const void *>(
            param_slot),
        sizeof(param));
    if (param == 0u ||
        !readable_range_cached(
            reinterpret_cast<const void *>(
                param),
            0x38u,
            vm.param))
        return false;

    std::memcpy(
        &count,
        reinterpret_cast<const void *>(
            param + 0x0au),
        sizeof(count));
    std::memcpy(
        &first,
        reinterpret_cast<const void *>(
            param + 0x34u),
        sizeof(first));
    return true;
}

bool populate_source_semantic_endpoints(
    bool bank_source,
    bool lerp_bank_source,
    frame_source_state_v1 &state) noexcept
{
    if (bank_source) {
        state.endpoint_source_a = state.owner;
        if (!capture_endpoint_param_identity(
                state.endpoint_source_a,
                state.endpoint_param_a,
                state.endpoint_count_a,
                state.endpoint_first_a))
            return false;
        state.endpoint_source_b =
            state.endpoint_source_a;
        state.endpoint_param_b =
            state.endpoint_param_a;
        state.endpoint_count_b =
            state.endpoint_count_a;
        state.endpoint_first_b =
            state.endpoint_first_a;
        return true;
    }

    if (!lerp_bank_source)
        return false;

    std::int16_t selector_a = -1;
    std::int16_t selector_b = -1;
    float beta = 0.0f;
    std::memcpy(
        &selector_a,
        &state.selector_word0,
        sizeof(selector_a));
    std::memcpy(
        &selector_b,
        reinterpret_cast<const std::uint8_t *>(
            &state.selector_word0) +
            sizeof(selector_a),
        sizeof(selector_b));
    std::memcpy(
        &beta,
        &state.selector_word1,
        sizeof(beta));

    const auto pair =
        pmetal_selector_policy::select(
            selector_a,
            selector_b,
            beta);
    if (!pair.valid ||
        !selected_lerp_endpoint_cached(
            state.owner,
            pair.a,
            state.endpoint_source_a) ||
        !capture_endpoint_param_identity(
            state.endpoint_source_a,
            state.endpoint_param_a,
            state.endpoint_count_a,
            state.endpoint_first_a))
        return false;

    if (pair.beta == 0.0f) {
        state.endpoint_source_b =
            state.endpoint_source_a;
        state.endpoint_param_b =
            state.endpoint_param_a;
        state.endpoint_count_b =
            state.endpoint_count_a;
        state.endpoint_first_b =
            state.endpoint_first_a;
        return true;
    }

    return
        selected_lerp_endpoint_cached(
            state.owner,
            pair.b,
            state.endpoint_source_b) &&
        capture_endpoint_param_identity(
            state.endpoint_source_b,
            state.endpoint_param_b,
            state.endpoint_count_b,
            state.endpoint_first_b);
}

bool spatial_overlap_xyz_unchecked(
    const void *node,
    const std::array<float,4> &query_min,
    const std::array<float,4> &query_max) noexcept
{
    const auto *bytes =
        static_cast<const std::uint8_t *>(node);
    std::array<float,4> node_min{};
    std::array<float,4> node_max{};

    std::memcpy(
        node_min.data(),
        bytes + 0x30u,
        sizeof(node_min));
    std::memcpy(
        node_max.data(),
        bytes + 0x40u,
        sizeof(node_max));

    // Retail selector 0x14055FC70 performs the overlap test as one
    // four-lane SIMD compare in both directions. Preserve all four lanes
    // exactly; the semantic meaning of lane 3 is irrelevant to the carrier.
    for (std::size_t i = 0u; i < 4u; ++i) {
        if (node_max[i] < query_min[i] ||
            query_max[i] < node_min[i])
            return false;
    }
    return true;
}

bool select_first_four_exact(
    void *collection,
    const float *query,
    std::uint8_t mask,
    std::array<std::uint32_t,4> &ids,
    std::array<void *,4> &nodes,
    std::uint8_t &count) noexcept
{
    ids = {};
    nodes = {};
    count = 0u;

    if (query == nullptr)
        return false;

    auto &source_vm =
        source_vm_cache_current();
    if (!readable_range_cached(
            collection,
            0x90u,
            source_vm.collection))
        return false;

    std::array<float,4> query_min{};
    std::array<float,4> query_max{};
    std::memcpy(
        query_min.data(),
        query,
        sizeof(query_min));
    std::memcpy(
        query_max.data(),
        query + 4,
        sizeof(query_max));

    auto *base =
        static_cast<std::uint8_t *>(collection);
    // R37: the source selector and the later source-cache preflight share
    // the same TLS VM-region verdict inside one presented-frame epoch. This
    // removes duplicate VirtualQuery calls for the exact nodes just validated
    // by selection while retaining per-present invalidation.
    auto &node_region = source_vm.node;

    for (std::uint32_t bucket = 0u;
         bucket < 4u && count < 4u;
         ++bucket) {
        if ((mask & (1u << bucket)) == 0u)
            continue;

        void *node = nullptr;
        std::memcpy(
            &node,
            base + 0x18u + bucket * 0x20u,
            sizeof(node));

        std::uint32_t guard = 0u;
        while (node != nullptr &&
               count < 4u) {
            // Validate each list node once. The previous diagnostic path
            // redundantly VirtualQuery'd the same node again inside the
            // overlap helper, which scaled badly with dense light lists.
            if (++guard > 4096u ||
                !readable_range_cached(
                    node,
                    0x50u,
                    node_region))
                return false;

            if (spatial_overlap_xyz_unchecked(
                    node,
                    query_min,
                    query_max)) {
                std::uint32_t id = 0u;
                std::memcpy(
                    &id,
                    static_cast<const std::uint8_t *>(
                        node) + 0x10u,
                    sizeof(id));

                ids[count] = id;
                nodes[count] = node;
                ++count;
                if (count == 4u)
                    break;
            }

            void *next = nullptr;
            std::memcpy(
                &next,
                static_cast<const std::uint8_t *>(
                    node) + 0x28u,
                sizeof(next));
            node = next;
        }
    }

    return true;
}

bool capture_source(
    void *node,
    source_raw &out,
    pointlight_ptde_source::draw_bank_authority_cache &bank_cache) noexcept
{
    out = {};
    auto &source_vm =
        source_vm_cache_current();
    if (!readable_range_cached(
            node,
            0x20u,
            source_vm.node)) {
        log_source_capture_failure_once(
            1u << 0,
            "node_unreadable",
            node,
            nullptr);
        return false;
    }

    void **vtable = nullptr;
    std::memcpy(&vtable, node, sizeof(vtable));
    if (!readable_range_cached(
            vtable,
            13u * sizeof(void *),
            source_vm.vtable)) {
        log_source_capture_failure_once(
            1u << 1,
            "vtable_unreadable",
            node,
            vtable);
        return false;
    }

    void *target = nullptr;
    std::memcpy(
        &target,
        vtable + 12u,
        sizeof(target));

    // Accept only statically attested retail source classes. Bank and LerpBank
    // were already proven by the PointLight donor RE. R31 adds exactly one
    // DirectPointLightEntity source vfunc: DSR base+0x55C570 is the structural
    // homologue of PTDE 0x00D34D50 and emits the same 8-float carrier
    // { position.xyz, 1/(End-Begin), RGB/source signal, End }. No other source
    // class is implied by this authorization.
    const auto target_address =
        reinterpret_cast<std::uintptr_t>(target);
    const bool bank_source =
        target_address == g_base + 0x55BC00u;
    const bool direct_source =
        target_address == g_base + 0x55C570u;
    const bool lerp_bank_source =
        target_address == g_base + 0x55D0B0u;
    if (!bank_source &&
        !direct_source &&
        !lerp_bank_source) {
        log_source_capture_failure_once(
            1u << 2,
            "source_class_unattested",
            node,
            target);
        return false;
    }
    // R37: executable protection for all three exact retail source-vfunc
    // addresses is attested once during install(). Preserve the per-source
    // executable gate as a fail-open fallback for the audit contract, but
    // short-circuit its VirtualQuery after exact install-time attestation.
    if (!g_source_exec_attested.load(
            std::memory_order_relaxed) &&
        !executable_address(target)) {
        log_source_capture_failure_once(
            1u << 3,
            "source_vfunc_nonexec",
            node,
            target);
        return false;
    }

    frame_source_state_v1 frame_state{};
    bool semantic_endpoints_ready = false;
    const bool frame_cacheable =
        bank_source || lerp_bank_source;
    if (frame_cacheable) {
        const std::size_t position_offset =
            bank_source ? 0x60u : 0x70u;
        const std::size_t required_size =
            position_offset + 4u * sizeof(float);
        if (!readable_range_cached(
                node,
                required_size,
                source_vm.node)) {
            log_source_capture_failure_once(
                1u << 7,
                bank_source
                    ? "bank_state_unreadable"
                    : "lerp_state_unreadable",
                node,
                target);
            return false;
        }

        frame_state.node = node;
        frame_state.collection =
            g_draw_selection.input.collection;
        frame_state.target = target_address;
        frame_state.source_class =
            bank_source ? 1u : 2u;
        std::memcpy(
            &frame_state.source_id,
            static_cast<const std::uint8_t *>(node) +
                0x10u,
            sizeof(frame_state.source_id));
        std::memcpy(
            &frame_state.source_category,
            static_cast<const std::uint8_t *>(node) +
                0x18u,
            sizeof(frame_state.source_category));
        std::memcpy(
            &frame_state.owner,
            static_cast<const std::uint8_t *>(node) +
                0x50u,
            sizeof(frame_state.owner));
        std::memcpy(
            &frame_state.selector_word0,
            static_cast<const std::uint8_t *>(node) +
                0x58u,
            sizeof(frame_state.selector_word0));
        std::memcpy(
            &frame_state.selector_word1,
            static_cast<const std::uint8_t *>(node) +
                0x5cu,
            sizeof(frame_state.selector_word1));
        std::memcpy(
            frame_state.position_bits.data(),
            static_cast<const std::uint8_t *>(node) +
                position_offset,
            frame_state.position_bits.size() *
                sizeof(frame_state.position_bits[0]));

        // Bank structure data itself is immutable asset identity; selectors,
        // beta and endpoint routing are live semantic inputs. Include the
        // actual Bank endpoint objects and param table identities so a Lerp
        // manager remap cannot produce a false persistent hit.
        semantic_endpoints_ready =
            populate_source_semantic_endpoints(
                bank_source,
                lerp_bank_source,
                frame_state);

        const auto semantic_generation =
            g_source_semantic_generation.load(
                std::memory_order_acquire);
        if (g_source_cache_seen_generation !=
            semantic_generation)
            reset_frame_source_cache_tls(
                semantic_generation);

        const auto cache_index =
            semantic_endpoints_ready
                ? frame_source_cache_index(
                      frame_state)
                : 0u;
        const auto &entry =
            g_source_frame_cache[cache_index];
        if (semantic_endpoints_ready &&
            entry.valid &&
            same_frame_source_state(
                entry.state,
                frame_state)) {
            out = entry.source;
            static std::atomic_bool
                cache_hit_logged{false};
            if (!cache_hit_logged.exchange(
                    true,
                    std::memory_order_relaxed)) {
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL POINTLIGHT R40] generational_source_cache_hit=1 exact_state_snapshot=ON invalidation=ACTIVE_COLLECTION_INSERT");
            }
            return true;
        }
    }

    alignas(16) std::array<float,8> raw{};

    // R36 performance cut: do not execute the stock Bank/LerpBank source
    // packer and then immediately reconstruct the same source again from the
    // PTDE donor. Retail DSR disassembly attests the only host-owned lane that
    // must be preserved before donor substitution:
    //   BankPointLightEntity     base+0x55BC00 -> position.xyz at node+0x60
    //   LerpBankPointLightEntity base+0x55D0B0 -> position.xyz at node+0x70
    // pointlight_ptde_source::capture() then writes the exact PTDE
    // invRange/RGB/End lanes while preserving raw[0..2].
    //
    // DirectPointLightEntity has no donor and its retail packer is itself the
    // exact PTDE-homologous carrier, so keep the direct native call only for
    // that class.
    if (bank_source || lerp_bank_source) {
        const std::size_t position_offset =
            bank_source ? 0x60u : 0x70u;

        std::memcpy(
            raw.data(),
            static_cast<const std::uint8_t *>(node) +
                position_offset,
            3u * sizeof(float));

        if (!pointlight_ptde_source::capture(
                node,
                g_base,
                raw,
                bank_cache)) {
            // Preserve the pre-R36 donor-miss behavior exactly: unresolved
            // donor authority falls back to the attested stock source packer
            // rather than suppressing an otherwise valid PointLight.
            using source_fn =
                void (__fastcall *)(void *, float *);
            const auto fn =
                reinterpret_cast<source_fn>(target);
            fn(node, raw.data());
        }
    } else {
        using source_fn =
            void (__fastcall *)(void *, float *);
        const auto fn =
            reinterpret_cast<source_fn>(target);
        fn(node, raw.data());
    }

    for (const auto value : raw)
        if (!std::isfinite(value)) {
            log_source_capture_failure_once(
                1u << 4,
                "source_nonfinite",
                node,
                target);
            return false;
        }

    if (!(raw[3] > 0.0f)) {
        log_source_capture_failure_once(
            1u << 5,
            "invalid_inv_range",
            node,
            target);
        return false;
    }

    if (!(raw[7] > 0.0f)) {
        log_source_capture_failure_once(
            1u << 6,
            "invalid_end",
            node,
            target);
        return false;
    }

    std::memcpy(
        &out.source_id,
        static_cast<const std::uint8_t *>(
            node) + 0x10u,
        sizeof(out.source_id));
    std::memcpy(
        &out.source_category,
        static_cast<const std::uint8_t *>(
            node) + 0x18u,
        sizeof(out.source_category));

    for (std::size_t i = 0u; i < 4u; ++i) {
        out.position_inv_range[i] = raw[i];
        out.raw_q_end[i] = raw[4u + i];
    }

    if (frame_cacheable &&
        semantic_endpoints_ready) {
        auto &entry =
            g_source_frame_cache[
                frame_source_cache_index(
                    frame_state)];
        entry.state = frame_state;
        entry.source = out;
        entry.valid = true;
    }

    return true;
}

void release_gpu(
    gpu_resources &gpu) noexcept
{
    if (gpu.b12 != nullptr)
        gpu.b12->Release();
    if (gpu.t19_srv != nullptr)
        gpu.t19_srv->Release();
    if (gpu.t19_buffer != nullptr)
        gpu.t19_buffer->Release();
    if (gpu.t18_srv != nullptr)
        gpu.t18_srv->Release();
    if (gpu.t18_buffer != nullptr)
        gpu.t18_buffer->Release();
    if (gpu.device != nullptr)
        gpu.device->Release();
    gpu = {};
}

void release_all_gpu_locked() noexcept
{
    for (auto &entry : g_gpu_by_context)
        release_gpu(entry.second);
    g_gpu_by_context.clear();
}

bool create_structured(
    ID3D11Device *device,
    UINT byte_width,
    UINT stride,
    ID3D11Buffer **buffer,
    ID3D11ShaderResourceView **srv) noexcept
{
    if (device == nullptr ||
        buffer == nullptr ||
        srv == nullptr)
        return false;

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = byte_width;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags =
        D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = stride;

    ID3D11Buffer *created = nullptr;
    if (FAILED(device->CreateBuffer(
            &desc,
            nullptr,
            &created)) ||
        created == nullptr)
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_UNKNOWN;
    view.ViewDimension =
        D3D11_SRV_DIMENSION_BUFFER;
    view.Buffer.FirstElement = 0u;
    view.Buffer.NumElements =
        byte_width / stride;

    ID3D11ShaderResourceView *created_srv =
        nullptr;
    if (FAILED(device->CreateShaderResourceView(
            created,
            &view,
            &created_srv)) ||
        created_srv == nullptr) {
        created->Release();
        return false;
    }

    *buffer = created;
    *srv = created_srv;
    return true;
}

bool ensure_gpu_locked(
    ID3D11DeviceContext *context,
    ID3D11Device *device,
    gpu_resources *&out) noexcept
{
    out = nullptr;
    if (context == nullptr || device == nullptr)
        return false;

    decltype(g_gpu_by_context)::iterator entry;
    try {
        entry =
            g_gpu_by_context.try_emplace(
                context).first;
    } catch (...) {
        return false;
    }

    auto &gpu = entry->second;
    if (gpu.device != nullptr &&
        gpu.device != device) {
        g_resource_epoch.fetch_add(
            1u,
            std::memory_order_acq_rel);
        release_gpu(gpu);
    }

    const bool complete =
        gpu.device == device &&
        gpu.t18_buffer != nullptr &&
        gpu.t18_srv != nullptr &&
        gpu.t19_buffer != nullptr &&
        gpu.t19_srv != nullptr &&
        gpu.b12 != nullptr;

    if (complete) {
        telemetry::hot_count(g_t18_hit);
        telemetry::hot_count(g_t19_hit);
        telemetry::hot_count(g_b12_hit);
        out = &gpu;
        return true;
    }

    // Never reuse a partial carrier after a failed creation attempt.
    release_gpu(gpu);

    if (!create_structured(
            device,
            static_cast<UINT>(
                sizeof(
                    operators::point_light::
                        clustered_t18_record_v1) *
                4u),
            static_cast<UINT>(
                sizeof(
                    operators::point_light::
                        clustered_t18_record_v1)),
            &gpu.t18_buffer,
            &gpu.t18_srv))
        return false;
    telemetry::hot_count(g_t18_create);

    if (!create_structured(
            device,
            static_cast<UINT>(
                sizeof(std::array<float,4>) *
                4u),
            static_cast<UINT>(
                sizeof(std::array<float,4>)),
            &gpu.t19_buffer,
            &gpu.t19_srv)) {
        release_gpu(gpu);
        return false;
    }
    telemetry::hot_count(g_t19_create);

    D3D11_BUFFER_DESC cb{};
    cb.ByteWidth = 64u;
    cb.Usage = D3D11_USAGE_DYNAMIC;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(device->CreateBuffer(
            &cb,
            nullptr,
            &gpu.b12)) ||
        gpu.b12 == nullptr) {
        release_gpu(gpu);
        return false;
    }
    telemetry::hot_count(g_b12_create);

    gpu.device = device;
    gpu.device->AddRef();
    gpu.generation =
        g_gpu_generation.fetch_add(
            1u,
            std::memory_order_relaxed);
    if (gpu.generation == 0u)
        gpu.generation =
            g_gpu_generation.fetch_add(
                1u,
                std::memory_order_relaxed);
    out = &gpu;
    return true;
}

bool update_buffer(
    ID3D11DeviceContext *context,
    ID3D11Buffer *buffer,
    const void *data,
    std::size_t size) noexcept
{
    if (context == nullptr ||
        buffer == nullptr ||
        data == nullptr ||
        size == 0u)
        return false;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(
            buffer,
            0u,
            D3D11_MAP_WRITE_DISCARD,
            0u,
            &mapped)) ||
        mapped.pData == nullptr)
        return false;

    std::memcpy(mapped.pData, data, size);
    context->Unmap(buffer, 0u);
    return true;
}


} // namespace

void clustered_pnts_builder_event_bridge(
    void *,
    void *) noexcept
{
    // Clustered is source-only. Legacy builder/receiver transport is retired.
}

void clustered_pnts_selector_event_bridge(
    void *,
    const void *) noexcept
{
    // No material/receiver authorization in Clustered source mode.
}

void clustered_pnts_selector_source_event_bridge() noexcept
{
    // Dead by construction in source-only clustered mode.
}

void clustered_pnts_selector_identity_event_bridge(
    const operators::material_response::material_identity &,
    bool) noexcept
{
    // Stock DSR owns the receiver/material stage.
}

bool clustered_pnts_draw_runtime::install() noexcept
{
    if (g_enabled.load())
        return g_runtime == this;
    if (g_runtime != nullptr &&
        g_runtime != this)
        return false;

    g_base =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));
    if (g_base == 0u)
        return false;

    // R37: exact retail source-vfunc executable protection is invariant for
    // the loaded module. Validate it once instead of once per captured source.
    if (!executable_address(
            reinterpret_cast<const void *>(
                g_base + 0x55BC00u)) ||
        !executable_address(
            reinterpret_cast<const void *>(
                g_base + 0x55C570u)) ||
        !executable_address(
            reinterpret_cast<const void *>(
                g_base + 0x55D0B0u)))
        return false;

    g_source_exec_attested.store(
        true,
        std::memory_order_relaxed);
    g_source_vm_cache = {};

    g_retained_selector =
        reinterpret_cast<retained_selector_fn>(
            g_base + 0x55FC70u);

    pointlight_ptde_source::
        clear_persistent_structure_cache();

    // Source-only Clustered bridge: stock DSR selects the live PointLightBank
    // row; this hook replaces only the source carrier at the attested producer
    // cut. No receiver/material or embedded PTDE donor lookup participates.
    if (!install_clustered_source_override_hook())
        return false;
    g_clustered_source_bank_identity_cache = {};
    g_clustered_source_payload_cache = {};

    g_runtime = this;
    g_quarantined.store(false);
    g_enabled.store(true);
    static std::atomic_bool bank_cache_logged{false};
    if (!bank_cache_logged.exchange(
            true,
            std::memory_order_relaxed))
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R29] exact_structure_snapshot_cache=ACTIVE cross_draw=ON revalidate=VM+MEMCMP pointer_only_authority=OFF gpu_tls_fast_cache=ON per_draw_resource_mutex=OFF per_draw_com_ref_churn=OFF pipeline_tls_fast_path=ON per_draw_private_data=OFF per_draw_shader_ref_churn=OFF failure_propagation=EXACT");
    static std::atomic_bool
        structural_cache_logged{false};
    if (!structural_cache_logged.exchange(
            true,
            std::memory_order_relaxed))
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R43-GATE] bank_identity=ROW0_EXACT_PAYLOAD hot_fingerprint_read=16B cold_extra_read=0B full_name_scan=OFF dsr_only_gate=10 default_m99=STOCK_DSR cache=DIRECT16");
    static std::atomic_bool
        frame_selection_cache_logged{false};
    if (!frame_selection_cache_logged.exchange(
            true,
            std::memory_order_relaxed))
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R38] frame_selection_cache=DIRECT128_EXACT_INPUT_PLUS_BUCKET_HEADS source_cache=DIRECT128_EXACT_STATE cache_scope=TLS_PER_PRESENT source_revalidation=ON");
    static std::atomic_bool
        source_only_logged{false};
    if (!source_only_logged.exchange(
            true,
            std::memory_order_relaxed)) {
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL POINTLIGHT R43-DRAWPARAM] clustered=SOURCE_ONLY carrier=LIVE_DRAWPARAM_SELECTED_ROW source_cut=0xB7E02 receiver=STOCK_DSR donor_lookup=OFF material_lookup=OFF replacement_shader=OFF dsr_only_gate=10 default_m99=STOCK_DSR");
    }
    return true;
}

void clustered_pnts_draw_runtime::uninstall() noexcept
{
    g_enabled.store(false);
    if (!restore_clustered_source_override_hook()) {
        reshade::log::message(
            reshade::log::level::error,
            "[DSRRL POINTLIGHT R43-DRAWPARAM] clustered_source_hook_restore_fail=1");
        g_quarantined.store(true, std::memory_order_relaxed);
    }
    g_clustered_source_bank_identity_cache = {};
g_clustered_source_payload_cache = {};
    g_source_exec_attested.store(
        false,
        std::memory_order_relaxed);
    consume_draw_selection();
    g_source_selection_cache = {};
    g_source_vm_cache = {};
    g_frame_selection_cache = {};
    g_source_frame_cache = {};
    g_source_cache_seen_generation = 0u;
    invalidate_source_semantic_generation();
    pointlight_ptde_source::
        clear_persistent_structure_cache();
    g_upload_identity_cache = {};
    g_upload_identity_victim = 0u;
    g_producer_input_tls = {};
    g_local_serial = 0u;

    g_resource_epoch.fetch_add(
        1u,
        std::memory_order_acq_rel);
    clear_gpu_fast_cache();
    {
        std::lock_guard<std::mutex> lock(
            g_resource_mutex);
        release_all_gpu_locked();
    }

    g_retained_selector = nullptr;
    g_base = 0u;
    g_runtime = nullptr;
}

void clustered_pnts_draw_runtime::builder_event(
    void *draw,
    void *renderer_context) noexcept
{
    telemetry::hot_count(g_builder_seen);

    // The 0x14022084F ordinary builder hook is extremely hot. Never traverse
    // PointLight lists, call source vfuncs, VirtualQuery draw memory or take a
    // global lock here. Capture only the exact selector inputs that the later
    // authorized draw may need.
    g_producer_input_tls = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        draw == nullptr ||
        renderer_context == nullptr)
        return;

    producer_input_snapshot input{};
    input.owner =
        reinterpret_cast<std::uintptr_t>(
            renderer_context);
    // Producer snapshot and selector join are both thread-local and
    // explicitly fail open across threads. A process-wide atomic serial added
    // a locked RMW to this extremely hot builder for no semantic benefit.
    input.serial =
        ++g_local_serial;

    const auto *renderer_bytes =
        static_cast<const std::uint8_t *>(
            renderer_context);
    const auto *draw_bytes =
        static_cast<const std::uint8_t *>(
            draw);

    // These exact objects/offsets are already live at the attested retail
    // builder site. Snapshot only draw-local state; external collection/node
    // memory is validated later, and only for an authorized PointLight draw.
    std::memcpy(
        &input.collection,
        renderer_bytes + 0x2328u,
        sizeof(input.collection));
    std::memcpy(
        input.query.data(),
        draw_bytes + 0xD0u,
        sizeof(input.query));
    std::memcpy(
        &input.mask,
        draw_bytes + 0x3Au,
        sizeof(input.mask));

    if (input.collection == nullptr ||
        (input.mask & 0x0fu) == 0u) {
        telemetry::hot_count(g_collection_fail);
        return;
    }

    input.valid = true;
    g_producer_input_tls = input;
    telemetry::hot_count(g_collection_ok);
    telemetry::hot_count(g_snapshot_publish);
}
void clustered_pnts_draw_runtime::selector_event(
    void *owner,
    const void *actual_material) noexcept
{
    telemetry::hot_count(g_selector_seen);
    g_draw_selection = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        owner == nullptr ||
        actual_material == nullptr)
        return;

    const auto owner_key =
        reinterpret_cast<std::uintptr_t>(
            owner);

    // Exact same-thread handoff only. Cross-thread delivery intentionally
    // fails open instead of resurrecting the old globally locked registry.
    if (!g_producer_input_tls.valid ||
        g_producer_input_tls.owner !=
            owner_key) {
        telemetry::hot_count(g_owner_join_miss);
        return;
    }
    telemetry::hot_count(g_owner_join_hit);

    // Exact retail return-RVA validation and resolve_actual_material() have
    // already authenticated this fixed-layout material object. Match the
    // existing FLVER draw-hot policy: direct fixed-layout read, no
    // VirtualQuery/syscall on this multi-million-call path.
    std::uint32_t material_max = 0u;
    std::memcpy(
        &material_max,
        static_cast<const std::uint8_t *>(
            actual_material) + 0x384u,
        sizeof(material_max));

    if (material_max == 0u) {
        telemetry::hot_count(
            g_material_limit_fail);
        return;
    }
    telemetry::hot_count(g_material_limit_ok);

    g_draw_selection.input =
        g_producer_input_tls;
    g_draw_selection.material_max = material_max;
    g_draw_selection.owner_verified = true;
    g_draw_selection.material_limit_ready =
        true;
    g_draw_selection.ready = true;
}
void clustered_pnts_draw_runtime::frame_event(
    std::uint64_t frame_serial) noexcept
{
    auto epoch = frame_serial + 1u;
    if (epoch == 0u)
        epoch = 1u;
    g_source_frame_epoch.store(
        epoch,
        std::memory_order_relaxed);
}

void clustered_pnts_draw_runtime::selector_source_event() noexcept
{
    if (!g_enabled.load() ||
        g_quarantined.load() ||
        !g_draw_selection.ready ||
        !g_draw_selection.owner_verified ||
        !g_draw_selection.material_limit_ready)
        return;

#ifdef DSRRL_POINTLIGHT_PROFILE
    const bool prof_active =
        prof_sample(g_prof_producer_seq);
    const auto prof_producer_start =
        prof_active ? prof_qpc() : 0u;
#endif

    const auto &input = g_draw_selection.input;
    auto &source_cache = g_source_selection_cache;

    if (source_cache.producer_serial != input.serial) {
        source_cache = {};
        source_cache.producer_serial = input.serial;
        source_cache.attempted = true;

        std::array<std::uint32_t,4> selected_ids{};
        std::array<void *,4> nodes{};
        std::uint8_t selected_count = 0u;

#ifdef DSRRL_POINTLIGHT_PROFILE
        const auto prof_select_start =
            prof_active ? prof_qpc() : 0u;
#endif
        // R38: many material draws in one presented frame repeat the exact
        // same PointLight selection query. Producer serial is intentionally
        // unique per builder event, so the R20 per-serial cache cannot reuse
        // that result. Cache only the exact selector output keyed by owner,
        // collection, mask, all eight query words and the four current bucket
        // heads. Scope is one presented-frame epoch. A collision is a miss.
        // Source capture is still executed below on every producer serial, so
        // R36 exact source-state validation remains authoritative.
        frame_selection_key_v1 selection_key{};
        const bool selection_key_ready =
            make_frame_selection_key(
                input,
                selection_key);
        const auto selection_epoch =
            g_source_frame_epoch.load(
                std::memory_order_relaxed);
        const auto selection_hash =
            selection_key_ready
                ? frame_selection_hash(
                      selection_key)
                : 0u;
        const auto selection_index =
            static_cast<std::size_t>(
                selection_hash &
                (k_frame_selection_cache_entries - 1u));

        bool selection_ready = false;
        bool selection_cached = false;
        if (selection_key_ready) {
            const auto &entry =
                g_frame_selection_cache[
                    selection_index];
            if (entry.epoch ==
                    selection_epoch &&
                entry.hash ==
                    selection_hash &&
                same_frame_selection_key(
                    entry.key,
                    selection_key)) {
                selected_ids = entry.ids;
                nodes = entry.nodes;
                selected_count = entry.count;
                selection_ready = true;
                selection_cached = true;

                static std::atomic_bool
                    selection_cache_hit_logged{false};
                if (!selection_cache_hit_logged.exchange(
                        true,
                        std::memory_order_relaxed)) {
                    reshade::log::message(
                        reshade::log::level::info,
                        "[DSRRL POINTLIGHT R38] frame_selection_cache_hit=1 exact_input=ON bucket_heads=ON source_revalidation=ON");
                }
            }
        }

        if (!selection_cached) {
            selection_ready =
                input.valid &&
                select_first_four_exact(
                    input.collection,
                    input.query.data(),
                    input.mask,
                    selected_ids,
                    nodes,
                    selected_count);

            if (selection_ready &&
                selection_key_ready) {
                auto &entry =
                    g_frame_selection_cache[
                        selection_index];
                entry.epoch = selection_epoch;
                entry.hash = selection_hash;
                entry.key = selection_key;
                entry.ids = selected_ids;
                entry.nodes = nodes;
                entry.count = selected_count;
            }
        }
#ifdef DSRRL_POINTLIGHT_PROFILE
        if (prof_active)
            prof_add(
                g_prof_select,
                prof_qpc() - prof_select_start);
#endif
        if (!selection_ready) {
            source_cache.failure =
                clustered_pnts_prepare_failure::selection;
            g_draw_selection.cached_failure =
                source_cache.failure;
            telemetry::hot_count(g_mirror_diff);
            telemetry::hot_count(g_selection_fail);
            telemetry::hot_count(g_sidecar_fail);
#ifdef DSRRL_POINTLIGHT_PROFILE
            if (prof_active)
                prof_add(
                    g_prof_producer,
                    prof_qpc() - prof_producer_start);
#endif
            return;
        }
        telemetry::hot_count(g_selector_calls);

#if defined(DSRRL_CLUSTERED_SELECTOR_RUNTIME_CROSSCHECK)
        if (g_retained_selector == nullptr) {
            source_cache.failure =
                clustered_pnts_prepare_failure::selection;
            g_draw_selection.cached_failure =
                source_cache.failure;
            telemetry::hot_count(g_mirror_diff);
            telemetry::hot_count(g_selection_fail);
            telemetry::hot_count(g_sidecar_fail);
#ifdef DSRRL_POINTLIGHT_PROFILE
            if (prof_active)
                prof_add(
                    g_prof_producer,
                    prof_qpc() - prof_producer_start);
#endif
            return;
        }

        std::array<std::uint32_t,4> host_ids{
            0xffffffffu,0xffffffffu,
            0xffffffffu,0xffffffffu};
        const int host_count =
            g_retained_selector(
                input.collection,
                host_ids.data(),
                4,
                input.query.data(),
                input.mask);

        if (host_count < 0 ||
            host_count > 4 ||
            static_cast<int>(selected_count) !=
                host_count) {
            source_cache.failure =
                clustered_pnts_prepare_failure::selection;
            g_draw_selection.cached_failure =
                source_cache.failure;
            telemetry::hot_count(g_mirror_diff);
            telemetry::hot_count(g_selection_fail);
            telemetry::hot_count(g_sidecar_fail);
#ifdef DSRRL_POINTLIGHT_PROFILE
            if (prof_active)
                prof_add(
                    g_prof_producer,
                    prof_qpc() - prof_producer_start);
#endif
            return;
        }

        for (std::uint8_t i = 0u;
             i < selected_count;
             ++i) {
            if (host_ids[i] != selected_ids[i]) {
                source_cache.failure =
                    clustered_pnts_prepare_failure::selection;
                g_draw_selection.cached_failure =
                    source_cache.failure;
                telemetry::hot_count(g_mirror_diff);
                telemetry::hot_count(g_selection_fail);
                telemetry::hot_count(g_sidecar_fail);
#ifdef DSRRL_POINTLIGHT_PROFILE
                if (prof_active)
                    prof_add(
                        g_prof_producer,
                        prof_qpc() - prof_producer_start);
#endif
                return;
            }
        }
        telemetry::hot_count(g_mirror_equal);
#endif

        source_cache.selected_count = selected_count;
        pointlight_ptde_source::draw_bank_authority_cache
            bank_cache{};
        if (selected_count == 0u) {
            source_cache.ready = true;
            source_cache.neutral = true;
            telemetry::hot_count(g_selection_empty);
        } else {
#ifdef DSRRL_POINTLIGHT_PROFILE
            const auto prof_capture_start =
                prof_active ? prof_qpc() : 0u;
#endif
            for (std::uint8_t i = 0u;
                 i < selected_count;
                 ++i) {
                if (!capture_source(
                        nodes[i],
                        source_cache.sources[i],
                        bank_cache) ||
                    source_cache.sources[i].source_id !=
                        selected_ids[i]) {
                    source_cache.failure =
                        clustered_pnts_prepare_failure::source_capture;
                    g_draw_selection.cached_failure =
                        source_cache.failure;
                    telemetry::hot_count(g_source_capture_fail);
                    telemetry::hot_count(g_sidecar_fail);
#ifdef DSRRL_POINTLIGHT_PROFILE
                    if (prof_active)
                        prof_add(
                            g_prof_producer,
                            prof_qpc() - prof_producer_start);
#endif
                    return;
                }
                telemetry::hot_count(g_source_capture_ok);
            }
            source_cache.ready = true;
#ifdef DSRRL_POINTLIGHT_PROFILE
            if (prof_active)
                prof_add(
                    g_prof_capture,
                    prof_qpc() - prof_capture_start);
#endif
        }
    }

    if (!source_cache.ready) {
        g_draw_selection.cached_failure =
            source_cache.failure;
#ifdef DSRRL_POINTLIGHT_PROFILE
        if (prof_active)
            prof_add(
                g_prof_producer,
                prof_qpc() - prof_producer_start);
#endif
        return;
    }

    if (source_cache.neutral) {
        g_draw_selection.neutral_no_pointlights = true;
        g_draw_selection.cached_failure =
            clustered_pnts_prepare_failure::empty_selection;
        telemetry::hot_count(g_prepare_neutral_empty);
    }

#ifdef DSRRL_POINTLIGHT_PROFILE
    if (prof_active)
        prof_add(
            g_prof_producer,
            prof_qpc() - prof_producer_start);
#endif
}

void clustered_pnts_draw_runtime::selector_identity_event(
    const operators::material_response::material_identity &identity,
    bool expected_spc) noexcept
{
    if (!g_enabled.load() ||
        g_quarantined.load() ||
        !g_draw_selection.ready ||
        !g_draw_selection.owner_verified ||
        !g_draw_selection.material_limit_ready)
        return;

    const auto decision =
        operators::material_response::
            evaluate_direct_pointlight_material_identity(
                identity,
                expected_spc);

    if (!decision.active)
        return;

    g_draw_selection.material = identity;
    g_draw_selection.material_decision = decision;
    g_draw_selection.material_spc = expected_spc;
    g_draw_selection.authority_ready = true;

    const auto &input = g_draw_selection.input;
    auto &source_cache = g_source_selection_cache;

    // R34 source/consumer split: material-response authorization never
    // performs PointLight selection or source capture. The source carrier must
    // already have been produced by selector_source_event() for this exact
    // producer serial. This prevents a future Spc material branch from
    // implicitly changing PointLight source-production policy.
    if (source_cache.producer_serial != input.serial ||
        !source_cache.attempted ||
        !source_cache.ready) {
        g_draw_selection.cached_failure =
            source_cache.producer_serial == input.serial
                ? source_cache.failure
                : clustered_pnts_prepare_failure::selection;
        return;
    }

    if (source_cache.neutral) {
        g_draw_selection.neutral_no_pointlights = true;
        g_draw_selection.cached_failure =
            clustered_pnts_prepare_failure::empty_selection;
        return;
    }

#ifdef DSRRL_POINTLIGHT_PROFILE
    const bool prof_active =
        prof_sample(g_prof_sidecar_seq);
    const auto prof_build_start =
        prof_active ? prof_qpc() : 0u;
#endif
    const auto built =
        operators::point_light::
            build_clustered_sidecar_v1(
                source_cache.sources,
                source_cache.selected_count,
                g_draw_selection.material_max,
                decision);
#ifdef DSRRL_POINTLIGHT_PROFILE
    if (prof_active)
        prof_add(
            g_prof_sidecar_build,
            prof_qpc() - prof_build_start);
#endif

    g_draw_selection.sidecar_result_code =
        static_cast<std::uint8_t>(built.result);

    if (built.result !=
            operators::point_light::
                clustered_sidecar_result_v1::ready ||
        !built.payload.ready) {
        g_draw_selection.cached_failure =
            clustered_pnts_prepare_failure::sidecar_build;
        telemetry::hot_count(g_sidecar_build_fail);
        telemetry::hot_count(g_sidecar_fail);
        return;
    }

    g_draw_selection.payload = built.payload;
    g_draw_selection.payload_ready = true;
    telemetry::hot_count(g_sidecar_ready);
}

bool clustered_pnts_draw_runtime::current_draw_authority(
    bool expected_spc,
    operators::material_response::material_identity &material,
    operators::material_response::decision &decision) const noexcept
{
    material = {};
    decision = {};

#ifdef DSRRL_POINTLIGHT_PROFILE
    const bool prof_active =
        prof_sample(g_prof_authority_seq);
    const auto prof_start =
        prof_active ? prof_qpc() : 0u;
#endif

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        !g_draw_selection.ready ||
        !g_draw_selection.authority_ready ||
        g_draw_selection.material_spc != expected_spc ||
        !g_producer_input_tls.valid ||
        g_draw_selection.input.serial !=
            g_producer_input_tls.serial ||
        g_draw_selection.input.owner !=
            g_producer_input_tls.owner) {
#ifdef DSRRL_POINTLIGHT_PROFILE
        if (prof_active)
            prof_add(
                g_prof_authority,
                prof_qpc() - prof_start);
#endif
        return false;
    }

    material = g_draw_selection.material;
    decision = g_draw_selection.material_decision;
#ifdef DSRRL_POINTLIGHT_PROFILE
    if (prof_active)
        prof_add(
            g_prof_authority,
            prof_qpc() - prof_start);
#endif
    return decision.active;
}

bool clustered_pnts_draw_runtime::prepare_sidecar(
    ID3D11DeviceContext *context,
    const operators::material_response::decision &material,
    prepared_clustered_pnts_draw &prepared) noexcept
{
    prepared = {};

#ifdef DSRRL_POINTLIGHT_PROFILE
    const bool prof_active =
        prof_sample(g_prof_prepare_seq);
    const auto prof_start =
        prof_active ? prof_qpc() : 0u;
#endif

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        context == nullptr ||
        !g_draw_selection.ready ||
        !g_draw_selection.owner_verified ||
        !g_draw_selection.material_limit_ready ||
        !material.active) {
        prepared.failure =
            clustered_pnts_prepare_failure::precondition;
        telemetry::hot_count(g_prepare_precondition_fail);
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    const auto &input =
        g_draw_selection.input;

    if (!g_draw_selection.authority_ready ||
        g_draw_selection.material_decision.route_index !=
            material.route_index) {
        prepared.failure =
            clustered_pnts_prepare_failure::precondition;
        telemetry::hot_count(g_prepare_precondition_fail);
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    if (g_draw_selection.neutral_no_pointlights) {
        prepared.producer_serial = input.serial;
        prepared.raw_selected_count = 0u;
        prepared.material_max_pnt_lit_num =
            g_draw_selection.material_max;
        prepared.effective_count = 0u;
        prepared.owner_verified = true;
        prepared.selector_mirror_verified = true;
        prepared.neutral_no_pointlights = true;
        return false;
    }

    if (!g_draw_selection.payload_ready) {
        prepared.failure =
            g_draw_selection.cached_failure !=
                    clustered_pnts_prepare_failure::none
                ? g_draw_selection.cached_failure
                : clustered_pnts_prepare_failure::sidecar_build;
        prepared.sidecar_result_code =
            g_draw_selection.sidecar_result_code;
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    const auto &payload =
        g_draw_selection.payload;

    const auto context_type = context->GetType();
    if (context_type == D3D11_DEVICE_CONTEXT_IMMEDIATE)
        telemetry::hot_count(g_context_immediate);
    else if (context_type == D3D11_DEVICE_CONTEXT_DEFERRED)
        telemetry::hot_count(g_context_deferred);
    else
        telemetry::hot_count(g_context_other);

    ID3D11Device *device = nullptr;
    context->GetDevice(&device);
    if (device == nullptr) {
        prepared.failure =
            clustered_pnts_prepare_failure::gpu_prepare;
        telemetry::hot_count(g_gpu_prepare_fail);
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    // R29: after one synchronized lookup/create per recording context, keep
    // stable COM references in a TLS cache. Steady-state PointLight draws no
    // longer take the process-global resource mutex and no longer perform
    // five AddRef + three Release operations merely to protect one draw.
    // A global epoch invalidates every TLS entry when device/resource
    // ownership changes; pointer identity alone never authorizes reuse.
    ID3D11Buffer *t18_buffer = nullptr;
    ID3D11ShaderResourceView *t18_srv = nullptr;
    ID3D11Buffer *t19_buffer = nullptr;
    ID3D11ShaderResourceView *t19_srv = nullptr;
    ID3D11Buffer *b12 = nullptr;
    bool upload_t18 = true;
    bool upload_t19 = true;
    bool upload_b12 = true;
    std::uint64_t gpu_generation = 0u;

#ifdef DSRRL_POINTLIGHT_PROFILE
    const auto prof_gpu_start =
        prof_active ? prof_qpc() : 0u;
#endif
    gpu_fast_cache_entry *fast =
        lookup_gpu_fast_cache(
            context,
            device);

    if (fast != nullptr) {
        telemetry::hot_count(
            g_gpu_fast_hit);
    } else {
        telemetry::hot_count(
            g_gpu_fast_miss);

        std::lock_guard<std::mutex> lock(
            g_resource_mutex);

        gpu_resources *gpu = nullptr;
        if (!ensure_gpu_locked(
                context,
                device,
                gpu)) {
            device->Release();
            prepared.failure =
                clustered_pnts_prepare_failure::gpu_prepare;
            telemetry::hot_count(g_gpu_prepare_fail);
            telemetry::hot_count(g_prepare_fail);
            return false;
        }

        if (gpu == nullptr ||
            gpu->t18_buffer == nullptr ||
            gpu->t18_srv == nullptr ||
            gpu->t19_buffer == nullptr ||
            gpu->t19_srv == nullptr ||
            gpu->b12 == nullptr) {
            device->Release();
            prepared.failure =
                clustered_pnts_prepare_failure::gpu_resources;
            telemetry::hot_count(g_prepare_fail);
            return false;
        }

        fast =
            store_gpu_fast_cache(
                context,
                *gpu);
        if (fast == nullptr) {
            device->Release();
            prepared.failure =
                clustered_pnts_prepare_failure::gpu_resources;
            telemetry::hot_count(g_prepare_fail);
            return false;
        }
    }

#ifdef DSRRL_POINTLIGHT_PROFILE
    if (prof_active)
        prof_add(
            g_prof_gpu_cache,
            prof_qpc() - prof_gpu_start);
#endif

    t18_buffer = fast->t18_buffer;
    t18_srv = fast->t18_srv;
    t19_buffer = fast->t19_buffer;
    t19_srv = fast->t19_srv;
    b12 = fast->b12;
    gpu_generation = fast->generation;

    device->Release();

    auto &upload_identity =
        upload_identity_for(
            context,
            gpu_generation);
    upload_t18 =
        !upload_identity.last_t18_valid ||
        std::memcmp(
            upload_identity.last_t18.data(),
            payload.t18.data(),
            sizeof(payload.t18)) != 0;
    upload_t19 =
        !upload_identity.last_t19_valid ||
        std::memcmp(
            upload_identity.last_t19.data(),
            payload.t19.data(),
            sizeof(payload.t19)) != 0;
    upload_b12 =
        !upload_identity.last_b12_valid ||
        std::memcmp(
            upload_identity.last_b12.data(),
            payload.b12.data(),
            sizeof(payload.b12)) != 0;

#ifdef DSRRL_POINTLIGHT_PROFILE
    const auto prof_upload_start =
        prof_active ? prof_qpc() : 0u;
#endif
    const bool uploaded =
        (!upload_t18 ||
         update_buffer(
             context,
             t18_buffer,
             payload.t18.data(),
             sizeof(payload.t18))) &&
        (!upload_t19 ||
         update_buffer(
             context,
             t19_buffer,
             payload.t19.data(),
             sizeof(payload.t19))) &&
        (!upload_b12 ||
         update_buffer(
             context,
             b12,
             payload.b12.data(),
             sizeof(payload.b12)));
#ifdef DSRRL_POINTLIGHT_PROFILE
    if (prof_active)
        prof_add(
            g_prof_upload,
            prof_qpc() - prof_upload_start);
#endif

    if (uploaded) {
        if (upload_t18) {
            upload_identity.last_t18 = payload.t18;
            upload_identity.last_t18_valid = true;
        }
        if (upload_t19) {
            upload_identity.last_t19 = payload.t19;
            upload_identity.last_t19_valid = true;
        }
        if (upload_b12) {
            upload_identity.last_b12 = payload.b12;
            upload_identity.last_b12_valid = true;
        }
    }

    if (!uploaded) {
        prepared.failure =
            clustered_pnts_prepare_failure::upload;
        telemetry::hot_count(g_upload_fail);
        telemetry::hot_count(g_prepare_fail);
        return false;
    }

    prepared.t18 = t18_srv;
    prepared.t19 = t19_srv;
    prepared.b12 = b12;
    prepared.producer_serial =
        input.serial;
    prepared.raw_selected_count =
        payload.raw_selected_count;
    prepared.material_max_pnt_lit_num =
        payload.material_max_pnt_lit_num;
    prepared.effective_count =
        payload.effective_count;
    prepared.owner_verified = true;
    prepared.selector_mirror_verified = true;
    prepared.carrier_borrowed_tls = true;
    prepared.ready = true;

    telemetry::hot_count(g_prepare_ok);
#ifdef DSRRL_POINTLIGHT_PROFILE
    if (prof_active) {
        prof_add(
            g_prof_prepare,
            prof_qpc() - prof_start);
        maybe_log_pointlight_prepare_profile();
    }
#endif
    return true;
}

void clustered_pnts_draw_runtime::release_prepared_draw(
    prepared_clustered_pnts_draw &prepared) noexcept
{
    if (!prepared.carrier_borrowed_tls) {
        if (prepared.b12 != nullptr)
            prepared.b12->Release();
        if (prepared.t19 != nullptr)
            prepared.t19->Release();
        if (prepared.t18 != nullptr)
            prepared.t18->Release();
    }
    prepared = {};
}

void clustered_pnts_draw_runtime::consume_draw_selection() noexcept
{
    g_draw_selection = {};
}

void clustered_pnts_draw_runtime::on_destroy_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());

    g_resource_epoch.fetch_add(
        1u,
        std::memory_order_acq_rel);
    clear_gpu_fast_cache();

    std::lock_guard<std::mutex> lock(
        g_resource_mutex);

    for (auto it =
             g_gpu_by_context.begin();
         it != g_gpu_by_context.end();) {
        if (it->second.device == native) {
            release_gpu(it->second);
            it =
                g_gpu_by_context.erase(it);
        } else {
            ++it;
        }
    }
}

clustered_pnts_telemetry
clustered_pnts_draw_runtime::telemetry() const noexcept
{
    return {
        g_builder_seen.load(),
        g_collection_ok.load(),
        g_collection_fail.load(),
        g_selector_calls.load(),
        g_mirror_equal.load(),
        g_mirror_diff.load(),
        g_source_capture_ok.load(),
        g_source_capture_fail.load(),
        g_source_producer_hits.load(),
        g_source_row_bridge.load(),
        g_source_row_stock_dsr.load(),
        g_source_dsr_only_fail_open.load(),
        g_source_unclassified_bank_fail_open.load(),
        g_source_class_reject.load(),
        g_source_payload_cache_hit.load(),
        g_source_payload_cache_miss.load(),
        g_source_category_hits[0].load(),
        g_source_category_hits[1].load(),
        g_source_category_hits[2].load(),
        g_source_category_hits[3].load(),
        g_snapshot_publish.load(),
        g_selector_seen.load(),
        g_owner_join_hit.load(),
        g_owner_join_miss.load(),
        g_material_limit_ok.load(),
        g_material_limit_fail.load(),
        g_sidecar_ready.load(),
        g_sidecar_fail.load(),
        g_t18_create.load(),
        g_t18_hit.load(),
        g_t19_create.load(),
        g_t19_hit.load(),
        g_b12_create.load(),
        g_b12_hit.load(),
        g_prepare_ok.load(),
        g_prepare_fail.load(),
        g_prepare_neutral_empty.load(),
        g_prepare_precondition_fail.load(),
        g_selection_fail.load(),
        g_selection_empty.load(),
        g_sidecar_build_fail.load(),
        g_context_immediate.load(),
        g_context_deferred.load(),
        g_context_other.load(),
        g_gpu_prepare_fail.load(),
        g_upload_fail.load(),
        g_enabled.load(),
        g_quarantined.load()
    };
}

void clustered_pnts_draw_runtime::reset() noexcept
{
    consume_draw_selection();
    g_source_selection_cache = {};
    g_frame_selection_cache = {};
    reset_frame_source_cache_tls(
        g_source_frame_epoch.fetch_add(
            1u,
            std::memory_order_relaxed) + 1u);
    g_source_vm_cache = {};
    pointlight_ptde_source::
        clear_persistent_structure_cache();
    g_producer_input_tls = {};

    g_resource_epoch.fetch_add(
        1u,
        std::memory_order_acq_rel);
    clear_gpu_fast_cache();
    {
        std::lock_guard<std::mutex> lock(
            g_resource_mutex);
        release_all_gpu_locked();
    }

    g_local_serial = 0u;
    g_builder_seen.store(0u);
    g_collection_ok.store(0u);
    g_collection_fail.store(0u);
    g_selector_calls.store(0u);
    g_mirror_equal.store(0u);
    g_mirror_diff.store(0u);
    g_source_capture_ok.store(0u);
    g_source_capture_fail.store(0u);
    g_source_producer_hits.store(0u);
    g_source_row_bridge.store(0u);
    g_source_row_stock_dsr.store(0u);
    g_source_dsr_only_fail_open.store(0u);
    g_source_unclassified_bank_fail_open.store(0u);
    g_source_class_reject.store(0u);
    g_source_payload_cache_hit.store(0u);
    g_source_payload_cache_miss.store(0u);
    for (auto &counter : g_source_category_hits)
        counter.store(0u);
    g_snapshot_publish.store(0u);
    g_selector_seen.store(0u);
    g_owner_join_hit.store(0u);
    g_owner_join_miss.store(0u);
    g_material_limit_ok.store(0u);
    g_material_limit_fail.store(0u);
    g_sidecar_ready.store(0u);
    g_sidecar_fail.store(0u);
    g_t18_create.store(0u);
    g_t18_hit.store(0u);
    g_t19_create.store(0u);
    g_t19_hit.store(0u);
    g_b12_create.store(0u);
    g_b12_hit.store(0u);
    g_prepare_ok.store(0u);
    g_prepare_fail.store(0u);
    g_prepare_neutral_empty.store(0u);
    g_prepare_precondition_fail.store(0u);
    g_selection_fail.store(0u);
    g_selection_empty.store(0u);
    g_sidecar_build_fail.store(0u);
    g_context_immediate.store(0u);
    g_context_deferred.store(0u);
    g_context_other.store(0u);
    g_gpu_prepare_fail.store(0u);
    g_upload_fail.store(0u);
    g_gpu_fast_hit.store(0u);
    g_gpu_fast_miss.store(0u);
    g_source_capture_reason_mask.store(
        0u,
        std::memory_order_relaxed);
    g_quarantined.store(false);
}

} // namespace dsrrl::runtime
