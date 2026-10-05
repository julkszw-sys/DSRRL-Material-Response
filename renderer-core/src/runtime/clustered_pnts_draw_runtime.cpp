#include "dsrrl/runtime/pointlight_ptde_source_runtime.hpp"
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

// R36: source capture is materially more expensive than first-four selection,
// especially for Bank/LerpBank where donor authority and source-manager
// resolution were previously repeated for every authorized material draw.
// Cache the exact resolved carrier only inside a presented-frame epoch and only
// while the relevant source-object bytes are unchanged. The cache is TLS: no
// locks, no cross-thread authority and no retained COM/object ownership.
struct frame_source_state_v1 {
    void *node = nullptr;
    std::uintptr_t target = 0u;
    std::uintptr_t owner = 0u;
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

constexpr std::size_t k_frame_source_cache_entries = 32u;
std::atomic<std::uint64_t> g_source_frame_epoch{1u};
thread_local std::uint64_t g_source_frame_seen_epoch = 0u;
thread_local std::array<
    frame_source_cache_entry_v1,
    k_frame_source_cache_entries> g_source_frame_cache{};
thread_local std::uint8_t g_source_frame_cache_victim = 0u;

void reset_frame_source_cache_tls(
    std::uint64_t epoch) noexcept
{
    g_source_frame_cache = {};
    g_source_frame_cache_victim = 0u;
    g_source_frame_seen_epoch = epoch;
}

bool same_frame_source_state(
    const frame_source_state_v1 &a,
    const frame_source_state_v1 &b) noexcept
{
    return
        a.node == b.node &&
        a.target == b.target &&
        a.owner == b.owner &&
        a.source_id == b.source_id &&
        a.selector_word0 == b.selector_word0 &&
        a.selector_word1 == b.selector_word1 &&
        a.position_bits == b.position_bits &&
        a.source_category == b.source_category &&
        a.source_class == b.source_class;
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

std::atomic<std::uint32_t> g_source_capture_reason_mask{0u};

void log_source_capture_failure_once(
    std::uint32_t bit,
    const char *reason,
    void *node,
    const void *target) noexcept
{
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

    if (!readable_range(collection, 0x90u) ||
        query == nullptr)
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
    // Valid only for this selector pass. Nodes allocated in the same committed
    // VM region reuse one VirtualQuery result, but no region verdict survives
    // into the next draw.
    readable_region_cache node_region{};

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
    if (!readable_range(node, 0x20u)) {
        log_source_capture_failure_once(
            1u << 0,
            "node_unreadable",
            node,
            nullptr);
        return false;
    }

    void **vtable = nullptr;
    std::memcpy(&vtable, node, sizeof(vtable));
    if (!readable_range(
            vtable,
            13u * sizeof(void *))) {
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
    if (!executable_address(target)) {
        log_source_capture_failure_once(
            1u << 3,
            "source_vfunc_nonexec",
            node,
            target);
        return false;
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
        if (!readable_range(
                static_cast<const std::uint8_t *>(node) +
                    position_offset,
                3u * sizeof(float))) {
            log_source_capture_failure_once(
                1u << 7,
                bank_source
                    ? "bank_position_unreadable"
                    : "lerp_position_unreadable",
                node,
                target);
            return false;
        }

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
    void *draw,
    void *renderer_context) noexcept
{
    if (g_runtime != nullptr)
        g_runtime->builder_event(
            draw,
            renderer_context);
}

void clustered_pnts_selector_event_bridge(
    void *owner,
    const void *actual_material) noexcept
{
    if (g_runtime != nullptr)
        g_runtime->selector_event(
            owner,
            actual_material);
}

void clustered_pnts_selector_source_event_bridge() noexcept
{
    if (g_runtime != nullptr)
        g_runtime->selector_source_event();
}

void clustered_pnts_selector_identity_event_bridge(
    const operators::material_response::material_identity &identity,
    bool expected_spc) noexcept
{
    if (g_runtime != nullptr)
        g_runtime->selector_identity_event(
            identity,
            expected_spc);
}

bool clustered_pnts_draw_runtime::install() noexcept
{
    if (g_enabled.load())
        return g_runtime == this;
    if (g_runtime != nullptr &&
        g_runtime != this)
        return false;

    const auto flver =
        flver_identity_transport::status();
    if (!flver.provenance_ok ||
        !flver.selector_armed ||
        !flver.builder_armed)
        return false;

    g_base =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));
    if (g_base == 0u)
        return false;

    g_retained_selector =
        reinterpret_cast<retained_selector_fn>(
            g_base + 0x55FC70u);

    pointlight_ptde_source::
        clear_persistent_structure_cache();

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
    return true;
}

void clustered_pnts_draw_runtime::uninstall() noexcept
{
    g_enabled.store(false);
    consume_draw_selection();
    g_source_selection_cache = {};
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
        const bool selection_ready =
            input.valid &&
            select_first_four_exact(
                input.collection,
                input.query.data(),
                input.mask,
                selected_ids,
                nodes,
                selected_count);
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
