#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/upper_lower_draw_runtime.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/operators/lightbank/snapshot_freshness.hpp"
#include "dsrrl/operators/lightbank/hemdir3.hpp"
#include "dsrrl/runtime/generated_pmetal_env_source_authority.hpp"
#include "dsrrl/runtime/fixed_pointlight_draw_runtime.hpp"

#include <Windows.h>
#include <d3d11.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include <mutex>
#include <unordered_map>

namespace dsrrl::runtime {
namespace {

struct f4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct snapshot {
    operators::lightbank::lightbank_snapshot_fingerprint fingerprint{};
    alignas(16) std::array<f4,8> ul_payload{};
    alignas(16) std::array<f4,8> hemdir3_payload{};
    bool d123_ready = false;

    bool pmetal_env_ready = false;
    f4 pmetal_env_a{};
    f4 pmetal_env_b{};
    float pmetal_env_beta = 0.0f;
    std::uint64_t pmetal_bank_a = 0;
    std::uint64_t pmetal_bank_b = 0;
    std::uint32_t pmetal_row_a = 0;
    std::uint32_t pmetal_row_b = 0;

};

struct producer_tls {
    bool active = false;
    std::uintptr_t owner = 0;
    const std::uint8_t *assignment = nullptr;
    bool have_upper = false;
    bool have_lower = false;
    bool have_d123 = false;
    bool have_pmetal_env = false;
    f4 upper{};
    f4 lower{};
    std::array<operators::lightbank::hemdir3_lobe,3> d123{};
    f4 pmetal_env_a{};
    f4 pmetal_env_b{};
    float pmetal_env_beta = 0.0f;
    std::uint64_t pmetal_bank_a = 0;
    std::uint64_t pmetal_bank_b = 0;
    std::uint32_t pmetal_row_a = 0;
    std::uint32_t pmetal_row_b = 0;

    // Cheap reference carrier. These fields are populated by the already
    // attested steady/blend packers and contain no decoded D123/P_Metal
    // payload. Expensive operator work is deferred until its exact consumer
    // gate is known.
    void *source_a = nullptr;
    void *source_b = nullptr;
    std::int32_t selector_a = -1;
    std::int32_t selector_b = -1;
    float source_beta = 0.0f;
    bool reference_ready = false;
    bool evaluated_vectors_ready = false;
    std::array<f4,3> evaluated_directions{};
};

struct lightbank_reference_token {
    operators::lightbank::lightbank_snapshot_fingerprint fingerprint{};
    void *source_a = nullptr;
    void *source_b = nullptr;
    std::int32_t selector_a = -1;
    std::int32_t selector_b = -1;
    float beta = 0.0f;
    std::array<f4,3> directions{};
    f4 upper{};
    f4 lower{};
    bool valid = false;
    bool available = false;
};

struct steady_q_cache_entry {
    void *source = nullptr;
    std::int32_t selector = -1;
    const std::uint8_t *header = nullptr;
    const std::uint8_t *records = nullptr;
    const std::uint8_t *record = nullptr;
    bool view_valid = false;
    bool sample_valid = false;
    f4 q_upper{};
    f4 q_lower{};
    f4 upper{};
    f4 lower{};
};

struct raw_record_cache_entry {
    void *source = nullptr;
    std::int32_t selector = -1;
    const std::uint8_t *header = nullptr;
    const std::uint8_t *record = nullptr;
    bool valid = false;
};

struct d123_identity_cache_entry {
    const std::uint8_t *record = nullptr;
    std::array<std::uint8_t,0x24> raw{};
    bool sample_valid = false;
    std::array<operators::lightbank::hemdir3_lobe,3> lobes{};
};

struct pmetal_bank_cache_entry {
    const std::uint8_t *base = nullptr;
    std::uint16_t count = 0u;
    std::uint64_t signature = 0u;
    const pmetal_env_source_authority::bank_donor *bank = nullptr;
    bool valid = false;
};


// Clean-Q proved that steady semantic capture is sufficient to collapse
// performance. A 32-entry direct-mapped cache is too small for scenes with a
// large active LightBank working set: unrelated source/selector pairs can
// continually evict one another and force the fully validated slow path on
// every call. Keep the cache allocation fixed and TLS, but make the three
// clean-Q hot caches 4-way set associative with 128 sets (512 entries).
constexpr std::size_t k_steady_cache_sets = 128u;
constexpr std::size_t k_steady_cache_ways = 4u;
constexpr std::size_t k_steady_cache_entries =
    k_steady_cache_sets * k_steady_cache_ways;
constexpr std::size_t k_pmetal_cache_slots = 32u;

thread_local std::array<steady_q_cache_entry,k_steady_cache_entries>
    g_steady_q_cache{};
thread_local std::array<raw_record_cache_entry,k_steady_cache_entries>
    g_raw_record_cache{};
thread_local std::array<d123_identity_cache_entry,k_steady_cache_entries>
    g_d123_identity_cache{};
thread_local std::array<std::uint8_t,k_steady_cache_sets>
    g_steady_q_victim{};
thread_local std::array<std::uint8_t,k_steady_cache_sets>
    g_raw_record_victim{};
thread_local std::array<std::uint8_t,k_steady_cache_sets>
    g_d123_victim{};

thread_local std::array<pmetal_bank_cache_entry,k_pmetal_cache_slots>
    g_pmetal_bank_cache{};

// b13 is draw-local data, but the D3D11 buffer object itself does not need to
// be draw-local. Creating an immutable buffer per LightBank snapshot caused
// thousands of ID3D11Device::CreateBuffer calls while turning the camera.
// Keep one DEFAULT constant buffer per native D3D11 context and update its
// 128-byte payload in command-stream order before the replay draw. Separate
// U/L and HemDir3 buffers preserve independent carrier contents.
//
// D3D11 contexts are the natural serialization domain for UpdateSubresource.
// A TLS entry keeps the steady hot path lock-free; the global map is touched
// only when a context is first observed or after device teardown.
struct b13_upload_slot {
    ID3D11Device *device = nullptr;
    ID3D11Buffer *ul_buffer = nullptr;
    ID3D11Buffer *hemdir3_buffer = nullptr;
    std::array<f4,8> ul_last{};
    std::array<f4,8> hemdir3_last{};
    bool ul_last_valid = false;
    bool hemdir3_last_valid = false;

    ~b13_upload_slot()
    {
        if (ul_buffer != nullptr)
            ul_buffer->Release();
        if (hemdir3_buffer != nullptr)
            hemdir3_buffer->Release();
        if (device != nullptr)
            device->Release();
    }
};

std::mutex g_b13_upload_mutex;
std::unordered_map<
    ID3D11DeviceContext *,
    std::unique_ptr<b13_upload_slot>>
    g_b13_upload_slots;
std::atomic<std::uint64_t> g_b13_upload_epoch{1u};

struct b13_upload_tls_cache {
    ID3D11DeviceContext *context = nullptr;
    b13_upload_slot *slot = nullptr;
    std::uint64_t epoch = 0u;
};

thread_local b13_upload_tls_cache
    g_b13_upload_tls{};

b13_upload_slot *resolve_b13_upload_slot(
    ID3D11DeviceContext *context) noexcept
{
    if (context == nullptr)
        return nullptr;

    const auto epoch =
        g_b13_upload_epoch.load(
            std::memory_order_acquire);

    if (g_b13_upload_tls.context == context &&
        g_b13_upload_tls.slot != nullptr &&
        g_b13_upload_tls.epoch == epoch)
        return g_b13_upload_tls.slot;

    try {
        std::lock_guard<std::mutex> lock(
            g_b13_upload_mutex);

        const auto found =
            g_b13_upload_slots.find(context);
        if (found != g_b13_upload_slots.end()) {
            g_b13_upload_tls = {
                context,
                found->second.get(),
                epoch
            };
            return found->second.get();
        }

        ID3D11Device *device = nullptr;
        context->GetDevice(&device);
        if (device == nullptr)
            return nullptr;

        auto owned =
            std::make_unique<b13_upload_slot>();
        owned->device = device;

        auto *const raw = owned.get();
        g_b13_upload_slots.emplace(
            context,
            std::move(owned));

        g_b13_upload_tls = {
            context,
            raw,
            epoch
        };
        return raw;
    } catch (...) {
        return nullptr;
    }
}

void clear_b13_upload_slots() noexcept
{
    g_b13_upload_epoch.fetch_add(
        1u,
        std::memory_order_acq_rel);

    {
        std::lock_guard<std::mutex> lock(
            g_b13_upload_mutex);
        g_b13_upload_slots.clear();
    }

    g_b13_upload_tls = {};
}

struct inline_hook {
    void *target = nullptr;
    void *trampoline = nullptr;
    void *detour = nullptr;
    std::size_t stolen = 0;
    std::array<std::uint8_t,32> original{};
    bool patched = false;
};

using wrapper_fn =
    void *(__fastcall *)(void *,void *,void *,float);
using blend_fn =
    void *(__fastcall *)(void *,const void *,const void *,float);
using steady_packer_fn =
    void (__fastcall *)(void *,void *,std::int32_t);
using steady_eval_tail_fn =
    void (__fastcall *)(
        const void *,
        void *,
        std::int32_t,
        void *);
using lightbank_blend_packer_fn =
    void *(__fastcall *)(
        void *,
        void *,
        std::int32_t,
        void *,
        std::int32_t,
        float);
using pmetal_env_blend_fn =
    void (__fastcall *)(
        float *,
        void *,
        std::int32_t,
        void *,
        std::int32_t,
        float);
using steady_cache_builder_fn =
    void (__fastcall *)(void *,const void *);

constexpr std::uintptr_t k_rva_wrapper_type5 = 0x1C0BE0u;
constexpr std::uintptr_t k_rva_wrapper_type6 = 0x1C0C10u;
constexpr std::uintptr_t k_rva_blend_helper = 0x5642F0u;
constexpr std::uintptr_t k_rva_steady_packer = 0x563B80u;
constexpr std::uintptr_t k_rva_steady_eval_tail = 0x5634E7u;
constexpr std::uintptr_t k_rva_steady_cache_builder = 0x563590u;
constexpr std::uintptr_t k_rva_blend_packer = 0x5637E0u;
constexpr std::uintptr_t k_rva_pmetal_env_blend = 0x563C30u;

constexpr std::uintptr_t k_ret_blend_upper = 0x5639BAu;
constexpr std::uintptr_t k_ret_blend_lower = 0x5639D5u;
constexpr std::uintptr_t k_ret_sel_1 = 0x20E019u;
constexpr std::uintptr_t k_ret_sel_2 = 0x20EB7Fu;
constexpr std::uintptr_t k_ret_sel_3 = 0x20FB9Eu;

constexpr std::array<std::uint8_t,17> k_wrapper_bytes = {
    0x48,0x83,0xEC,0x38,0x4D,0x8B,0xC8,0xF3,0x0F,0x11,0x5C,0x24,0x20,0x4C,0x8B,0x41,0x40
};
constexpr std::array<std::uint8_t,19> k_blend_bytes = {
    0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x81,0xEC,0xC0,0x00,0x00,0x00
};
constexpr std::array<std::uint8_t,14> k_steady_packer_bytes = {
    0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x40,0x48,0x8B,0x41,0x18
};
// 0x1405634E7 is reached only on the valid selected-record path, after DSR
// has written qUpper*1.5 and qLower*1.5 to dst+0x60/+0x70. These 15 stolen
// bytes are branch-free and RIP-independent, so the existing raw trampoline
// remains relocation-safe.
constexpr std::array<std::uint8_t,15> k_steady_eval_tail_bytes = {
    0x0F,0x28,0x81,0x80,0x00,0x00,0x00,
    0x66,0x0F,0x7F,0x82,0x80,0x00,0x00,0x00
};
// 0x140563590 is the LightBank record cache builder. The first 14 bytes are
// a branch-free/RIP-independent prologue, so the existing trampoline is safe.
// Rewriting qUpper/qLower here moves the PTDE steady representation once per
// cache record instead of recomputing it for every visible object evaluation.
constexpr std::array<std::uint8_t,14> k_steady_cache_builder_bytes = {
    0x48,0x89,0x74,0x24,0x10,
    0x48,0x89,0x7C,0x24,0x18,
    0x55,0x48,0x8B,0xEC
};
constexpr std::array<std::uint8_t,15> k_blend_packer_bytes = {
    0x40,0x55,0x56,0x48,0x8D,0x6C,0x24,0xC1,
    0x48,0x81,0xEC,0x88,0x00,0x00,0x00
};
constexpr std::array<std::uint8_t,14> k_pmetal_env_blend_bytes = {
    0x40,0x53,0x48,0x83,0xEC,0x50,0x44,0x8B,
    0x94,0x24,0x80,0x00,0x00,0x00
};

constexpr std::size_t k_record_stride = 0x110u;
constexpr std::size_t k_q_upper_offset = 0x60u;
constexpr std::size_t k_q_lower_offset = 0x70u;
constexpr float k_inv_pow = 1.0f / 2.2f;
constexpr float k_stock_steady_ul_gain = 1.5f;

#pragma pack(push,1)
struct raw_rgbm {
    std::int16_t r;
    std::int16_t g;
    std::int16_t b;
    std::int16_t m;
};
#pragma pack(pop)

core::renderer_core *g_core = nullptr;
upper_lower_draw_runtime *g_runtime = nullptr;
std::uintptr_t g_base = 0u;
std::array<inline_hook,5> g_hooks{};
inline_hook g_pmetal_env_hook{};
inline_hook g_steady_eval_tail_hook{};
inline_hook g_steady_cache_builder_hook{};

wrapper_fn g_wrapper5_orig = nullptr;
wrapper_fn g_wrapper6_orig = nullptr;
blend_fn g_blend_orig = nullptr;
steady_packer_fn g_steady_packer_orig = nullptr;
steady_eval_tail_fn g_steady_eval_tail_orig = nullptr;
lightbank_blend_packer_fn g_blend_packer_orig = nullptr;
pmetal_env_blend_fn g_pmetal_env_blend_orig = nullptr;
steady_cache_builder_fn g_steady_cache_builder_orig = nullptr;

std::mutex g_snapshot_mutex;
std::unordered_map<
    std::uintptr_t,
    std::shared_ptr<const snapshot>> g_snapshots;

struct selector_snapshot_tls {
    std::uintptr_t owner = 0u;
    std::uint64_t epoch = 0u;
    std::shared_ptr<const snapshot> selected{};
};

std::atomic<std::uint64_t> g_snapshot_epoch{1u};
thread_local producer_tls g_producer{};
thread_local std::shared_ptr<const snapshot> g_draw_snapshot{};
thread_local selector_snapshot_tls g_selector_snapshot{};

// The old owner->shared_ptr snapshot registry was correct but far too
// expensive under geometry-heavy scenes. The cache-builder architecture uses
// a bounded 2-way TLS registry carrying only references plus the exact
// freshness fingerprint. No heap allocation, mutex, shared_ptr or global
// epoch participates in this path.
constexpr std::size_t k_reference_token_sets = 32u;
constexpr std::size_t k_reference_token_ways = 2u;
constexpr std::size_t k_reference_token_entries =
    k_reference_token_sets * k_reference_token_ways;
thread_local std::array<
    lightbank_reference_token,
    k_reference_token_entries>
    g_reference_tokens{};
thread_local std::array<std::uint8_t,k_reference_token_sets>
    g_reference_token_victim{};
thread_local lightbank_reference_token
    g_draw_reference_token{};

std::atomic_bool g_enabled{false};
std::atomic_bool g_quarantined{false};
std::atomic_bool g_restore_failed{false};

std::atomic<std::uint64_t> g_wrapper5{0};
std::atomic<std::uint64_t> g_wrapper6{0};
std::atomic<std::uint64_t> g_steady_seen{0};
std::atomic<std::uint64_t> g_steady_pass{0};
std::atomic<std::uint64_t> g_blend_seen{0};
std::atomic<std::uint64_t> g_blend_upper{0};
std::atomic<std::uint64_t> g_blend_lower{0};
std::atomic<std::uint64_t> g_d123_steady{0};
std::atomic<std::uint64_t> g_d123_blend_direction{0};
std::atomic<std::uint64_t> g_d123_blend_color{0};
std::atomic<std::uint64_t> g_d123_snapshot_publish{0};
std::atomic<std::uint64_t> g_snapshot_publish{0};
std::atomic<std::uint64_t> g_selector_seen{0};
std::atomic<std::uint64_t> g_selector_match{0};
std::atomic<std::uint64_t> g_selector_miss{0};
std::atomic<std::uint64_t> g_tuple_mismatch{0};
std::atomic<std::uint64_t> g_b13_create{0};
std::atomic<std::uint64_t> g_b13_hit{0};
std::atomic<std::uint64_t> g_hemdir3_b13_create{0};
std::atomic<std::uint64_t> g_hemdir3_b13_hit{0};
std::atomic<std::uint64_t> g_requests{0};
std::atomic<std::uint64_t> g_hemdir3_carrier_requests{0};
std::atomic<std::uint64_t> g_pmetal_env_steady{0};
std::atomic<std::uint64_t> g_pmetal_env_blend{0};
std::atomic<std::uint64_t> g_pmetal_env_miss{0};
std::atomic_bool g_pmetal_env_hook_armed{false};
std::atomic_bool g_direct_ul_producer_active{false};
std::atomic_bool g_steady_cache_builder_active{false};
std::atomic<std::uint64_t> g_direct_ul_steady_inject{0};
std::atomic<std::uint64_t> g_direct_ul_blend_inject{0};
std::atomic<std::uint64_t> g_direct_ul_inject_fail{0};

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

        cursor =
            std::min(
                region_end,
                end);
    }

    return true;
}

template <typename T>
bool safe_read(
    const void *ptr,
    T &out) noexcept
{
    if (!readable_range(
            ptr,
            sizeof(T)))
        return false;

    std::memcpy(
        &out,
        ptr,
        sizeof(T));
    return true;
}

std::size_t steady_cache_set(
    const void *ptr,
    std::int32_t selector) noexcept
{
    const auto value =
        reinterpret_cast<std::uintptr_t>(ptr);
    const auto mixed =
        (value >> 4u) ^
        (static_cast<std::uintptr_t>(
            static_cast<std::uint32_t>(selector)) *
         static_cast<std::uintptr_t>(0x9E3779B1u));

    return
        static_cast<std::size_t>(
            mixed &
            (k_steady_cache_sets - 1u));
}

std::size_t d123_cache_set(
    const void *ptr) noexcept
{
    const auto value =
        reinterpret_cast<std::uintptr_t>(ptr);

    return
        static_cast<std::size_t>(
            (value >> 4u) &
            (k_steady_cache_sets - 1u));
}

steady_q_cache_entry &steady_q_cache_for(
    void *source,
    std::int32_t selector) noexcept
{
    const auto set =
        steady_cache_set(
            source,
            selector);
    const auto base =
        set * k_steady_cache_ways;

    for (std::size_t way = 0u;
         way < k_steady_cache_ways;
         ++way) {
        auto &entry =
            g_steady_q_cache[base + way];
        if (entry.view_valid &&
            entry.source == source &&
            entry.selector == selector)
            return entry;
    }

    for (std::size_t way = 0u;
         way < k_steady_cache_ways;
         ++way) {
        auto &entry =
            g_steady_q_cache[base + way];
        if (!entry.view_valid) {
            entry = {};
            return entry;
        }
    }

    const auto victim =
        static_cast<std::size_t>(
            g_steady_q_victim[set]++ &
            static_cast<std::uint8_t>(
                k_steady_cache_ways - 1u));
    auto &entry =
        g_steady_q_cache[base + victim];
    entry = {};
    return entry;
}

raw_record_cache_entry &raw_record_cache_for(
    void *source,
    std::int32_t selector) noexcept
{
    const auto set =
        steady_cache_set(
            source,
            selector);
    const auto base =
        set * k_steady_cache_ways;

    for (std::size_t way = 0u;
         way < k_steady_cache_ways;
         ++way) {
        auto &entry =
            g_raw_record_cache[base + way];
        if (entry.valid &&
            entry.source == source &&
            entry.selector == selector)
            return entry;
    }

    for (std::size_t way = 0u;
         way < k_steady_cache_ways;
         ++way) {
        auto &entry =
            g_raw_record_cache[base + way];
        if (!entry.valid) {
            entry = {};
            return entry;
        }
    }

    const auto victim =
        static_cast<std::size_t>(
            g_raw_record_victim[set]++ &
            static_cast<std::uint8_t>(
                k_steady_cache_ways - 1u));
    auto &entry =
        g_raw_record_cache[base + victim];
    entry = {};
    return entry;
}

d123_identity_cache_entry &d123_cache_for(
    const std::uint8_t *record) noexcept
{
    const auto set =
        d123_cache_set(record);
    const auto base =
        set * k_steady_cache_ways;

    for (std::size_t way = 0u;
         way < k_steady_cache_ways;
         ++way) {
        auto &entry =
            g_d123_identity_cache[base + way];
        if (entry.sample_valid &&
            entry.record == record)
            return entry;
    }

    for (std::size_t way = 0u;
         way < k_steady_cache_ways;
         ++way) {
        auto &entry =
            g_d123_identity_cache[base + way];
        if (!entry.sample_valid) {
            entry = {};
            return entry;
        }
    }

    const auto victim =
        static_cast<std::size_t>(
            g_d123_victim[set]++ &
            static_cast<std::uint8_t>(
                k_steady_cache_ways - 1u));
    auto &entry =
        g_d123_identity_cache[base + victim];
    entry = {};
    return entry;
}

std::size_t pmetal_bank_cache_slot(
    const void *ptr) noexcept
{
    const auto value =
        reinterpret_cast<std::uintptr_t>(ptr);

    return
        static_cast<std::size_t>(
            (value >> 4u) &
            (k_pmetal_cache_slots - 1u));
}

std::uint64_t pmetal_fnv_byte(
    std::uint64_t hash,
    std::uint8_t value) noexcept
{
    hash ^= value;
    return hash * 0x100000001b3ULL;
}

struct readable_window {
    std::uintptr_t begin = 0u;
    std::uintptr_t end = 0u;
};

thread_local readable_window g_selector_window{};
thread_local readable_window g_assignment_window{};

bool ensure_readable_window(
    const void *ptr,
    readable_window &window) noexcept
{
    if (ptr == nullptr)
        return false;

    const auto address =
        reinterpret_cast<std::uintptr_t>(ptr);

    if (window.begin <= address &&
        address < window.end)
        return true;

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(
            ptr,
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

    if (region_end <= address ||
        region_end < region_begin)
        return false;

    window.begin = region_begin;
    window.end = region_end;
    return true;
}

bool read_selector_tuple(
    const std::uint8_t *descriptor,
    std::uint16_t &selector_a,
    std::uint16_t &selector_b,
    std::uint32_t &beta_bits) noexcept
{
    selector_a = 0u;
    selector_b = 0u;
    beta_bits = 0u;

    if (descriptor == nullptr)
        return false;

    const auto *tuple =
        descriptor + 0x4Cu;
    const auto begin =
        reinterpret_cast<std::uintptr_t>(
            tuple);
    const auto end =
        begin + 8u;

    if (end < begin ||
        !ensure_readable_window(
            tuple,
            g_selector_window) ||
        end > g_selector_window.end)
        return false;

    // This descriptor is observed at three exact, attested engine return
    // sites. Once its VM region is validated, read the contiguous tuple in
    // one copy instead of issuing three VirtualQuery-backed safe_read calls.
    std::array<std::uint8_t,8> raw{};
    std::memcpy(
        raw.data(),
        tuple,
        raw.size());
    std::memcpy(
        &selector_a,
        raw.data(),
        sizeof(selector_a));
    std::memcpy(
        &selector_b,
        raw.data() + 2u,
        sizeof(selector_b));
    std::memcpy(
        &beta_bits,
        raw.data() + 4u,
        sizeof(beta_bits));
    return true;
}

bool read_assignment_tuple(
    const std::uint8_t *assignment,
    std::uint16_t &selector_a,
    std::uint16_t &selector_b,
    std::uint32_t &beta_bits) noexcept
{
    selector_a = 0u;
    selector_b = 0u;
    beta_bits = 0u;

    if (assignment == nullptr)
        return false;

    const auto *tuple =
        assignment + 8u;
    const auto begin =
        reinterpret_cast<std::uintptr_t>(
            tuple);
    const auto end =
        begin + 8u;

    if (end < begin ||
        !ensure_readable_window(
            tuple,
            g_assignment_window) ||
        end > g_assignment_window.end)
        return false;

    // Wrapper assignment is engine-owned and has just been consumed by the
    // original call. Validate its VM region once, then read the contiguous
    // selectorA/selectorB/beta tuple in one copy instead of three
    // VirtualQuery-backed safe_read operations per publication.
    std::array<std::uint8_t,8> raw{};
    std::memcpy(
        raw.data(),
        tuple,
        raw.size());
    std::memcpy(
        &selector_a,
        raw.data(),
        sizeof(selector_a));
    std::memcpy(
        &selector_b,
        raw.data() + 2u,
        sizeof(selector_b));
    std::memcpy(
        &beta_bits,
        raw.data() + 4u,
        sizeof(beta_bits));
    return true;
}


const std::uint8_t *resolve_raw_lightbank_record(
    void *source,
    std::int32_t selector) noexcept;
f4 decode_rgbm(
    const raw_rgbm &value) noexcept;
f4 lerp4(
    const f4 &a,
    const f4 &b,
    float t) noexcept;

std::size_t reference_token_set(
    std::uintptr_t owner) noexcept
{
    const auto mixed =
        (owner >> 4u) ^
        (owner >> 17u) ^
        (owner >> 31u);

    return static_cast<std::size_t>(
        mixed &
        (k_reference_token_sets - 1u));
}

bool read_assignment_tuple_attested(
    const std::uint8_t *assignment,
    std::uint16_t &selector_a,
    std::uint16_t &selector_b,
    std::uint32_t &beta_bits) noexcept
{
    selector_a = 0u;
    selector_b = 0u;
    beta_bits = 0u;

    if (assignment == nullptr)
        return false;

    // run_wrapper calls this only after the original engine wrapper consumed
    // the same assignment object. The memory is therefore live for this
    // transaction; avoid VirtualQuery on every producer event.
    std::array<std::uint8_t,8> raw{};
    std::memcpy(
        raw.data(),
        assignment + 8u,
        raw.size());
    std::memcpy(
        &selector_a,
        raw.data(),
        sizeof(selector_a));
    std::memcpy(
        &selector_b,
        raw.data() + 2u,
        sizeof(selector_b));
    std::memcpy(
        &beta_bits,
        raw.data() + 4u,
        sizeof(beta_bits));
    return true;
}

bool read_selector_tuple_attested(
    const std::uint8_t *descriptor,
    std::uint16_t &selector_a,
    std::uint16_t &selector_b,
    std::uint32_t &beta_bits) noexcept
{
    selector_a = 0u;
    selector_b = 0u;
    beta_bits = 0u;

    if (descriptor == nullptr)
        return false;

    // descriptor is supplied only from the three exact 0x22BA20 return-site
    // register mappings. It has just been consumed by the engine selector.
    std::array<std::uint8_t,8> raw{};
    std::memcpy(
        raw.data(),
        descriptor + 0x4Cu,
        raw.size());
    std::memcpy(
        &selector_a,
        raw.data(),
        sizeof(selector_a));
    std::memcpy(
        &selector_b,
        raw.data() + 2u,
        sizeof(selector_b));
    std::memcpy(
        &beta_bits,
        raw.data() + 4u,
        sizeof(beta_bits));
    return true;
}

void publish_reference_token(
    const producer_tls &producer) noexcept
{
    if (!producer.reference_ready ||
        !producer.evaluated_vectors_ready ||
        !producer.have_upper ||
        !producer.have_lower ||
        producer.owner == 0u ||
        producer.assignment == nullptr ||
        producer.source_a == nullptr ||
        producer.source_b == nullptr ||
        producer.selector_a < 0 ||
        producer.selector_b < 0 ||
        !std::isfinite(producer.source_beta))
        return;

    std::uint16_t selector_a = 0u;
    std::uint16_t selector_b = 0u;
    std::uint32_t beta_bits = 0u;
    if (!read_assignment_tuple_attested(
            producer.assignment,
            selector_a,
            selector_b,
            beta_bits))
        return;

    lightbank_reference_token token{};
    token.fingerprint = {
        producer.owner,
        selector_a,
        selector_b,
        beta_bits
    };
    float assignment_beta = 0.0f;
    std::memcpy(
        &assignment_beta,
        &beta_bits,
        sizeof(assignment_beta));
    if (!std::isfinite(assignment_beta))
        return;

    token.source_a = producer.source_a;
    token.source_b = producer.source_b;
    token.selector_a = producer.selector_a;
    token.selector_b = producer.selector_b;
    token.beta = assignment_beta;
    token.directions =
        producer.evaluated_directions;
    token.upper = producer.upper;
    token.lower = producer.lower;
    token.valid = true;
    token.available = true;

    const auto set =
        reference_token_set(
            producer.owner);
    const auto base =
        set * k_reference_token_ways;

    for (std::size_t way = 0u;
         way < k_reference_token_ways;
         ++way) {
        auto &entry =
            g_reference_tokens[base + way];
        if (entry.valid &&
            entry.fingerprint.owner ==
                producer.owner) {
            entry = token;
            return;
        }
    }

    for (std::size_t way = 0u;
         way < k_reference_token_ways;
         ++way) {
        auto &entry =
            g_reference_tokens[base + way];
        if (!entry.valid ||
            !entry.available) {
            entry = token;
            return;
        }
    }

    const auto victim =
        static_cast<std::size_t>(
            g_reference_token_victim[set]++ &
            static_cast<std::uint8_t>(
                k_reference_token_ways - 1u));
    g_reference_tokens[base + victim] =
        token;
}

lightbank_reference_token *find_reference_token(
    std::uintptr_t owner) noexcept
{
    if (owner == 0u)
        return nullptr;

    const auto set =
        reference_token_set(owner);
    const auto base =
        set * k_reference_token_ways;

    for (std::size_t way = 0u;
         way < k_reference_token_ways;
         ++way) {
        auto &entry =
            g_reference_tokens[base + way];
        if (entry.valid &&
            entry.available &&
            entry.fingerprint.owner ==
                owner)
            return &entry;
    }

    return nullptr;
}

bool capture_evaluated_vectors(
    void *dst,
    producer_tls &producer) noexcept
{
    if (dst == nullptr)
        return false;

    const auto *bytes =
        static_cast<const std::uint8_t *>(dst);

    for (std::size_t i = 0u;
         i < producer.evaluated_directions.size();
         ++i)
        std::memcpy(
            &producer.evaluated_directions[i],
            bytes + i * sizeof(f4),
            sizeof(f4));

    std::memcpy(
        &producer.upper,
        bytes + k_q_upper_offset,
        sizeof(producer.upper));
    std::memcpy(
        &producer.lower,
        bytes + k_q_lower_offset,
        sizeof(producer.lower));

    producer.have_upper = true;
    producer.have_lower = true;
    producer.evaluated_vectors_ready = true;
    return true;
}

bool capture_source_reference(
    void *source_a,
    std::int32_t selector_a,
    void *source_b,
    std::int32_t selector_b,
    float beta,
    producer_tls &producer) noexcept
{
    if (source_a == nullptr ||
        source_b == nullptr ||
        selector_a < 0 ||
        selector_b < 0 ||
        !std::isfinite(beta))
        return false;

    producer.source_a = source_a;
    producer.source_b = source_b;
    producer.selector_a = selector_a;
    producer.selector_b = selector_b;
    producer.source_beta = beta;
    producer.reference_ready = true;
    return true;
}

bool decode_d123_colors_only(
    const lightbank_reference_token &token,
    std::array<f4,3> &colors) noexcept
{
    colors = {};

    if (!token.valid ||
        token.source_a == nullptr ||
        token.source_b == nullptr ||
        token.selector_a < 0 ||
        token.selector_b < 0 ||
        !std::isfinite(token.beta))
        return false;

    const auto *raw_a =
        resolve_raw_lightbank_record(
            token.source_a,
            token.selector_a);
    const auto *raw_b =
        resolve_raw_lightbank_record(
            token.source_b,
            token.selector_b);

    if (raw_a == nullptr &&
        raw_b == nullptr)
        return false;

    const std::uint8_t *eval_a = raw_a;
    const std::uint8_t *eval_b = raw_b;
    float beta = token.beta;

    if (raw_a != nullptr &&
        (token.selector_a ==
             token.selector_b ||
         raw_b == nullptr ||
         beta <= 0.0f)) {
        eval_b = raw_a;
        beta = 0.0f;
    } else if (
        raw_b != nullptr &&
        (raw_a == nullptr ||
         beta >= 1.0f)) {
        eval_a = raw_b;
        eval_b = raw_b;
        beta = 0.0f;
    }

    if (eval_a == nullptr ||
        eval_b == nullptr)
        return false;

    for (std::size_t i = 0u;
         i < colors.size();
         ++i) {
        raw_rgbm a{};
        raw_rgbm b{};
        std::memcpy(
            &a,
            eval_a + i * 0x0Cu + 0x04u,
            sizeof(a));
        std::memcpy(
            &b,
            eval_b + i * 0x0Cu + 0x04u,
            sizeof(b));

        colors[i] =
            beta == 0.0f
                ? decode_rgbm(a)
                : lerp4(
                    decode_rgbm(a),
                    decode_rgbm(b),
                    beta);

        if (!std::isfinite(colors[i].x) ||
            !std::isfinite(colors[i].y) ||
            !std::isfinite(colors[i].z))
            return false;
    }

    return true;
}

bool build_hemdir3_reference_payload(
    const lightbank_reference_token &token,
    std::array<f4,8> &payload) noexcept
{
    payload = {};
    if (!token.valid)
        return false;

    std::array<f4,3> colors{};
    if (!decode_d123_colors_only(
            token,
            colors))
        return false;

    for (std::size_t i = 0u;
         i < 3u;
         ++i) {
        payload[i] =
            token.directions[i];
        payload[3u + i] =
            colors[i];
    }

    payload[6] = token.upper;
    payload[7] = token.lower;
    return true;
}

bool pmetal_bank_signature(
    const std::uint8_t *base,
    std::uint64_t &signature) noexcept
{
    signature = 0u;
    if (base == nullptr)
        return false;

    // Bank identity is immutable resource metadata, but this function may sit
    // on the steady LightBank hot path. The previous implementation performed
    // safe_read -> VirtualQuery for every row field and every name byte.
    // Preserve the exact FNV identity equation while validating the fixed
    // header/table once and reusing a validated VM window while scanning
    // strings. This removes per-byte VirtualQuery without introducing a stale
    // identity cache or weakening fail-open semantics.
    if (!readable_range(
            base + 8u,
            sizeof(std::uint16_t) * 2u))
        return false;

    std::uint16_t version = 0u;
    std::uint16_t count = 0u;
    std::memcpy(
        &version,
        base + 8u,
        sizeof(version));
    std::memcpy(
        &count,
        base + 10u,
        sizeof(count));

    if (version != 4u ||
        count == 0u ||
        count > 256u)
        return false;

    const std::size_t table_bytes =
        0x30u +
        static_cast<std::size_t>(count) * 12u;

    if (!readable_range(
            base,
            table_bytes))
        return false;

    std::uint64_t hash = 0xcbf29ce484222325ULL;
    hash = pmetal_fnv_byte(
        hash,
        static_cast<std::uint8_t>(count));
    hash = pmetal_fnv_byte(
        hash,
        static_cast<std::uint8_t>(count >> 8u));

    const std::uint32_t minimum_name =
        0x30u +
        static_cast<std::uint32_t>(count) * 12u;

    readable_window name_window{};

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto *entry =
            base + 0x30u +
            static_cast<std::size_t>(i) * 12u;

        std::uint32_t row_id = 0u;
        std::uint32_t name_offset = 0u;
        std::memcpy(
            &row_id,
            entry,
            sizeof(row_id));
        std::memcpy(
            &name_offset,
            entry + 8u,
            sizeof(name_offset));

        if (name_offset < minimum_name ||
            name_offset > 0x100000u)
            return false;

        for (std::uint32_t shift = 0u;
             shift < 32u;
             shift += 8u)
            hash = pmetal_fnv_byte(
                hash,
                static_cast<std::uint8_t>(
                    row_id >> shift));

        bool terminated = false;
        for (std::uint32_t j = 0u;
             j < 256u;
             ++j) {
            const auto *name_byte =
                base +
                static_cast<std::size_t>(
                    name_offset) +
                j;

            if (!ensure_readable_window(
                    name_byte,
                    name_window))
                return false;

            std::uint8_t ch = 0u;
            std::memcpy(
                &ch,
                name_byte,
                sizeof(ch));

            hash = pmetal_fnv_byte(
                hash,
                ch);

            if (ch == 0u) {
                terminated = true;
                break;
            }
        }

        if (!terminated)
            return false;
    }

    signature = hash;
    return true;
}

const pmetal_env_source_authority::bank_donor *
resolve_pmetal_bank(
    const std::uint8_t *base,
    std::uint16_t count,
    std::uint64_t &signature) noexcept
{
    signature = 0u;
    if (base == nullptr ||
        count == 0u)
        return nullptr;

    auto &cached =
        g_pmetal_bank_cache[
            pmetal_bank_cache_slot(base)];

    // Legacy V13 already cached bank_index(base), including negative results.
    // The integrated runtime had accidentally dropped that layer, causing a
    // full exact FNV scan of the same non-P_Metal LightBank on every steady
    // packer invocation. Restore that source contract, but make it TLS and
    // include live row-count identity in the key. Only a successfully decoded
    // exact signature is cached; unreadable/transient memory never becomes a
    // durable negative result.
    if (cached.valid &&
        cached.base == base &&
        cached.count == count) {
        signature = cached.signature;
        return cached.bank;
    }

    std::uint64_t decoded_signature = 0u;
    if (!pmetal_bank_signature(
            base,
            decoded_signature)) {
        cached = {};
        return nullptr;
    }

    const auto *bank =
        pmetal_env_source_authority::find_bank(
            decoded_signature);

    cached.base = base;
    cached.count = count;
    cached.signature = decoded_signature;
    cached.bank = bank;
    cached.valid = true;

    signature = decoded_signature;
    return bank;
}

bool read_exact_pmetal_env_source(
    void *source,
    std::int32_t selector,
    f4 &out,
    std::uint64_t &bank_signature,
    std::uint32_t &row_id) noexcept
{
    out = {};
    bank_signature = 0u;
    row_id = 0u;

    if (source == nullptr ||
        selector < 0)
        return false;

    // Both callers are exact engine producer hooks and invoke this only
    // after the original producer returned successfully, so 'source' is
    // engine-attested for this call. On the steady hot path, first reuse the
    // immutable bank verdict by exact base pointer. This is especially
    // important for the overwhelmingly common non-P_Metal case: a known
    // negative bank now exits after one pointer load instead of repeating
    // safe_read -> VirtualQuery for source/header/count on every capture.
    const std::uint8_t *base = nullptr;
    std::memcpy(
        &base,
        static_cast<const std::uint8_t *>(
            source) + 0x18u,
        sizeof(base));
    if (base == nullptr)
        return false;

    const auto index =
        static_cast<std::uint8_t>(
            selector);

    auto &cached_bank =
        g_pmetal_bank_cache[
            pmetal_bank_cache_slot(base)];

    const pmetal_env_source_authority::bank_donor *bank = nullptr;
    std::uint16_t count = 0u;

    if (cached_bank.valid &&
        cached_bank.base == base) {
        // The source object is engine-attested at this hook. Recheck the tiny
        // immutable header identity directly so address reuse or a changed
        // bank count cannot inherit an old negative/positive verdict. This
        // retains fail-open identity without reintroducing VirtualQuery.
        std::uint16_t live_version = 0u;
        std::uint16_t live_count = 0u;
        std::memcpy(
            &live_version,
            base + 8u,
            sizeof(live_version));
        std::memcpy(
            &live_count,
            base + 10u,
            sizeof(live_count));

        if (live_version != 4u ||
            live_count != cached_bank.count ||
            live_count == 0u ||
            live_count > 256u) {
            cached_bank = {};
            return false;
        }

        count = live_count;
        bank_signature =
            cached_bank.signature;
        bank = cached_bank.bank;

        if (static_cast<std::uint32_t>(index) >=
                count ||
            bank == nullptr)
            return false;
    } else {
        std::uint16_t version = 0u;
        if (!safe_read(base + 8u, version) ||
            !safe_read(base + 10u, count) ||
            version != 4u ||
            count == 0u ||
            count > 256u ||
            static_cast<std::uint32_t>(index) >=
                count)
            return false;

        bank =
            resolve_pmetal_bank(
                base,
                count,
                bank_signature);
        if (bank == nullptr)
            return false;
    }

    const auto *entry =
        base + 0x30u +
        static_cast<std::size_t>(index) * 12u;

    // pmetal_bank_signature validated the complete fixed row table before a
    // cache entry could become valid. With immutable bank metadata and exact
    // base identity, a cached positive bank may read the selected row id
    // directly. The slow path above remains fail-open for unseen carriers.
    if (cached_bank.valid &&
        cached_bank.base == base) {
        std::memcpy(
            &row_id,
            entry,
            sizeof(row_id));
    } else if (!safe_read(
                   entry,
                   row_id)) {
        return false;
    }

    const auto *row =
        pmetal_env_source_authority::find_row(
            *bank,
            row_id);
    if (row == nullptr)
        return false;

    const float scale =
        static_cast<float>(row->m) *
        0.01f;

    out = {
        static_cast<float>(row->r) /
            255.0f * scale,
        static_cast<float>(row->g) /
            255.0f * scale,
        static_cast<float>(row->b) /
            255.0f * scale,
        0.0f
    };

    return
        std::isfinite(out.x) &&
        std::isfinite(out.y) &&
        std::isfinite(out.z);
}

bool write_bytes(
    void *dst,
    const void *src,
    std::size_t size) noexcept
{
    DWORD old = 0u;
    if (!VirtualProtect(
            dst,
            size,
            PAGE_EXECUTE_READWRITE,
            &old))
        return false;

    std::memcpy(
        dst,
        src,
        size);

    const bool flushed =
        FlushInstructionCache(
            GetCurrentProcess(),
            dst,
            size) != FALSE;

    DWORD ignored = 0u;
    (void)VirtualProtect(
        dst,
        size,
        old,
        &ignored);

    return flushed;
}

template <std::size_t N>
bool prepare_hook(
    inline_hook &hook,
    std::uintptr_t rva,
    const std::array<std::uint8_t,N> &expected,
    void *detour) noexcept
{
    static_assert(N >= 14u && N <= 32u);

    auto *target =
        reinterpret_cast<std::uint8_t *>(
            g_base + rva);

    if (!readable_range(
            target,
            N) ||
        std::memcmp(
            target,
            expected.data(),
            N) != 0)
        return false;

    void *trampoline =
        VirtualAlloc(
            nullptr,
            N + 14u,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE);

    if (trampoline == nullptr)
        return false;

    std::memcpy(
        trampoline,
        target,
        N);

    auto *tail =
        static_cast<std::uint8_t *>(
            trampoline) + N;

    tail[0] = 0xFFu;
    tail[1] = 0x25u;

    std::uint32_t zero = 0u;
    std::memcpy(
        tail + 2u,
        &zero,
        sizeof(zero));

    const auto back =
        reinterpret_cast<std::uint64_t>(
            target + N);

    std::memcpy(
        tail + 6u,
        &back,
        sizeof(back));

    if (FlushInstructionCache(
            GetCurrentProcess(),
            trampoline,
            N + 14u) == FALSE) {
        VirtualFree(
            trampoline,
            0,
            MEM_RELEASE);
        return false;
    }

    hook.target = target;
    hook.trampoline = trampoline;
    hook.detour = detour;
    hook.stolen = N;

    std::copy(
        expected.begin(),
        expected.end(),
        hook.original.begin());

    return true;
}

bool arm_hook(
    inline_hook &hook) noexcept
{
    if (hook.target == nullptr ||
        hook.trampoline == nullptr ||
        hook.detour == nullptr ||
        hook.stolen < 14u ||
        hook.stolen > hook.original.size())
        return false;

    std::array<std::uint8_t,32> patch{};
    patch.fill(0x90u);
    patch[0] = 0xFFu;
    patch[1] = 0x25u;

    std::uint32_t zero = 0u;
    std::memcpy(
        patch.data() + 2u,
        &zero,
        sizeof(zero));

    const auto detour =
        reinterpret_cast<std::uint64_t>(
            hook.detour);
    std::memcpy(
        patch.data() + 6u,
        &detour,
        sizeof(detour));

    hook.patched = true;
    return write_bytes(
        hook.target,
        patch.data(),
        hook.stolen);
}

bool restore_hook(
    inline_hook &hook) noexcept
{
    bool ok = true;

    if (hook.patched) {
        ok =
            hook.target != nullptr &&
            hook.stolen != 0u &&
            write_bytes(
                hook.target,
                hook.original.data(),
                hook.stolen);

        if (ok)
            ok =
                std::memcmp(
                    hook.target,
                    hook.original.data(),
                    hook.stolen) == 0;

        if (ok)
            hook.patched = false;
    }

    if (ok &&
        hook.trampoline != nullptr) {
        ok =
            VirtualFree(
                hook.trampoline,
                0,
                MEM_RELEASE) != FALSE;

        if (ok)
            hook.trampoline = nullptr;
    }

    if (ok)
        hook = {};

    return ok;
}

const std::uint8_t *resolve_raw_lightbank_record(
    void *source,
    std::int32_t selector) noexcept
{
    if (source == nullptr ||
        selector < 0)
        return nullptr;

    auto &cached =
        raw_record_cache_for(
            source,
            selector);

    // The packer original has already completed successfully before this
    // resolver is called. Re-reading the engine-owned source->header pointer
    // is therefore the narrow live-object attestation used by the hot path.
    // VirtualQuery is retained only when the tuple changes.
    if (cached.valid &&
        cached.source == source &&
        cached.selector == selector) {
        const std::uint8_t *live_header = nullptr;
        std::memcpy(
            &live_header,
            static_cast<const std::uint8_t *>(
                source) + 0x18u,
            sizeof(live_header));

        if (live_header != nullptr &&
            live_header == cached.header) {
            std::uint16_t live_type = 0u;
            std::uint16_t live_count = 0u;
            std::memcpy(
                &live_type,
                live_header + 0x08u,
                sizeof(live_type));
            std::memcpy(
                &live_count,
                live_header + 0x0Au,
                sizeof(live_count));

            if (live_type == 4u &&
                static_cast<std::uint32_t>(
                    selector) < live_count) {
                const auto index =
                    static_cast<std::size_t>(
                        static_cast<std::uint32_t>(
                            selector));
                std::uint32_t live_offset = 0u;
                std::memcpy(
                    &live_offset,
                    live_header +
                        0x34u +
                        index * 12u,
                    sizeof(live_offset));

                if (live_header +
                        live_offset ==
                    cached.record)
                    return cached.record;
            }
        }

        cached = {};
    }

    const std::uint8_t *header = nullptr;
    if (!safe_read(
            static_cast<const std::uint8_t *>(
                source) + 0x18u,
            header) ||
        header == nullptr)
        return nullptr;

    std::uint16_t type = 0u;
    std::uint16_t count = 0u;
    if (!safe_read(header + 0x08u, type) ||
        !safe_read(header + 0x0Au, count) ||
        type != 4u ||
        static_cast<std::uint32_t>(selector) >= count)
        return nullptr;

    const std::size_t index =
        static_cast<std::size_t>(
            static_cast<std::uint32_t>(selector));

    std::uint32_t offset = 0u;
    if (!safe_read(
            header + 0x34u + index * 12u,
            offset))
        return nullptr;

    const auto *record =
        header + offset;

    if (!readable_range(
            record,
            0x50u))
        return nullptr;

    cached.source = source;
    cached.selector = selector;
    cached.header = header;
    cached.record = record;
    cached.valid = true;

    return record;
}

bool raw_d123_endpoint(
    const std::uint8_t *record,
    std::array<
        operators::lightbank::hemdir3_raw_lobe_endpoint,
        3> &out) noexcept
{
    if (record == nullptr)
        return false;

    for (std::size_t i = 0u;
         i < out.size();
         ++i) {
        const auto *base =
            record + i * 0x0Cu;

        std::int16_t x = 0;
        std::int16_t y = 0;
        raw_rgbm color{};

        if (!safe_read(base + 0x00u, x) ||
            !safe_read(base + 0x02u, y) ||
            !safe_read(base + 0x04u, color))
            return false;

        out[i].direction.x_degrees =
            static_cast<float>(x);
        out[i].direction.y_degrees =
            static_cast<float>(y);
        out[i].color.rgb_255 = {
            static_cast<float>(color.r),
            static_cast<float>(color.g),
            static_cast<float>(color.b)
        };
        out[i].color.multiplier_percent =
            static_cast<float>(color.m);
    }

    return true;
}

bool decode_d123_identity_raw(
    const std::array<std::uint8_t,0x24> &raw,
    std::array<
        operators::lightbank::hemdir3_lobe,
        3> &out) noexcept
{
    std::array<
        operators::lightbank::hemdir3_raw_lobe_endpoint,
        3> endpoint{};

    for (std::size_t i = 0u;
         i < endpoint.size();
         ++i) {
        const auto *base =
            raw.data() +
            i * 0x0Cu;

        std::int16_t x = 0;
        std::int16_t y = 0;
        raw_rgbm color{};

        std::memcpy(
            &x,
            base + 0x00u,
            sizeof(x));
        std::memcpy(
            &y,
            base + 0x02u,
            sizeof(y));
        std::memcpy(
            &color,
            base + 0x04u,
            sizeof(color));

        endpoint[i].direction.x_degrees =
            static_cast<float>(x);
        endpoint[i].direction.y_degrees =
            static_cast<float>(y);
        endpoint[i].color.rgb_255 = {
            static_cast<float>(color.r),
            static_cast<float>(color.g),
            static_cast<float>(color.b)
        };
        endpoint[i].color.multiplier_percent =
            static_cast<float>(color.m);
    }

    const auto sample =
        operators::lightbank::
            evaluate_hemdir3_profile(
                endpoint,
                endpoint,
                0.0f);

    if (sample.result !=
        operators::lightbank::
            hemdir3_profile_result::exact)
        return false;

    out = sample.lobes;
    return true;
}

bool evaluate_raw_d123(
    const std::uint8_t *a,
    const std::uint8_t *b,
    float beta,
    std::array<
        operators::lightbank::hemdir3_lobe,
        3> &out) noexcept
{
    if (a != nullptr &&
        a == b &&
        beta == 0.0f) {
        auto &cached =
            d123_cache_for(a);

        std::array<std::uint8_t,0x24> raw{};
        std::memcpy(
            raw.data(),
            a,
            raw.size());

        if (cached.sample_valid &&
            cached.record == a &&
            std::memcmp(
                cached.raw.data(),
                raw.data(),
                raw.size()) == 0) {
            out = cached.lobes;
            return true;
        }

        std::array<
            operators::lightbank::hemdir3_lobe,
            3> decoded{};

        if (!decode_d123_identity_raw(
                raw,
                decoded)) {
            cached = {};
            return false;
        }

        cached.record = a;
        cached.raw = raw;
        cached.sample_valid = true;
        cached.lobes = decoded;
        out = decoded;
        return true;
    }

    std::array<
        operators::lightbank::hemdir3_raw_lobe_endpoint,
        3> endpoint_a{};
    std::array<
        operators::lightbank::hemdir3_raw_lobe_endpoint,
        3> endpoint_b{};

    if (!raw_d123_endpoint(a, endpoint_a) ||
        !raw_d123_endpoint(b, endpoint_b))
        return false;

    const auto sample =
        operators::lightbank::
            evaluate_hemdir3_profile(
                endpoint_a,
                endpoint_b,
                beta);

    if (sample.result !=
        operators::lightbank::
            hemdir3_profile_result::exact)
        return false;

    out = sample.lobes;
    return true;
}

bool inverse_q(
    float q,
    float &out) noexcept
{
    if (!std::isfinite(q) ||
        q < 0.0f)
        return false;

    out =
        q == 0.0f
            ? 0.0f
            : std::pow(q, k_inv_pow);

    return std::isfinite(out);
}

bool rewrite_steady_cache_ptde_ul(
    void *dst,
    const void *raw_row) noexcept
{
    if (dst == nullptr ||
        raw_row == nullptr)
        return false;

    auto *bytes =
        static_cast<std::uint8_t *>(dst);
    const auto *raw =
        static_cast<const std::uint8_t *>(
            raw_row);

    raw_rgbm authored_upper{};
    raw_rgbm authored_lower{};
    std::memcpy(
        &authored_upper,
        raw + 0x24u,
        sizeof(authored_upper));
    std::memcpy(
        &authored_lower,
        raw + 0x2Cu,
        sizeof(authored_lower));

    // PTDE steady U/L consumes the authored RGBM carrier linearly:
    // P=(RGB/255)*(M/100). DSR steady evaluator 0x140563460 applies its
    // stock x1.5 terminal gain to cache +0x60/+0x70, so the exact cache
    // representation is P/1.5. This avoids the DSR pow(2.2) domain entirely
    // and requires only integer->float conversion/multiply once per row.
    const auto to_cache =
        [](const raw_rgbm &value) noexcept -> f4 {
            const float scale =
                (static_cast<float>(value.m) /
                 100.0f) /
                k_stock_steady_ul_gain;
            return {
                static_cast<float>(value.r) /
                    255.0f * scale,
                static_cast<float>(value.g) /
                    255.0f * scale,
                static_cast<float>(value.b) /
                    255.0f * scale,
                0.0f
            };
        };

    const auto linear_upper =
        to_cache(authored_upper);
    const auto linear_lower =
        to_cache(authored_lower);

    f4 cache_upper{};
    f4 cache_lower{};
    std::memcpy(
        &cache_upper,
        bytes + k_q_upper_offset,
        sizeof(cache_upper));
    std::memcpy(
        &cache_lower,
        bytes + k_q_lower_offset,
        sizeof(cache_lower));

    // Preserve DSR W lanes exactly; replace only semantic RGB.
    cache_upper.x = linear_upper.x;
    cache_upper.y = linear_upper.y;
    cache_upper.z = linear_upper.z;
    cache_lower.x = linear_lower.x;
    cache_lower.y = linear_lower.y;
    cache_lower.z = linear_lower.z;

    std::memcpy(
        bytes + k_q_upper_offset,
        &cache_upper,
        sizeof(cache_upper));
    std::memcpy(
        bytes + k_q_lower_offset,
        &cache_lower,
        sizeof(cache_lower));
    return true;
}

void __fastcall hook_steady_cache_builder(
    void *dst,
    const void *raw_row) noexcept
{
    if (g_steady_cache_builder_orig != nullptr)
        g_steady_cache_builder_orig(
            dst,
            raw_row);

    if (!g_steady_cache_builder_active.load(
            std::memory_order_acquire) ||
        dst == nullptr ||
        raw_row == nullptr)
        return;

    if (rewrite_steady_cache_ptde_ul(
            dst,
            raw_row))
        telemetry::hot_count(
            g_direct_ul_steady_inject);
    else
        telemetry::hot_count(
            g_direct_ul_inject_fail);
}

bool read_selected_ptde(
    void *source,
    std::int32_t selector,
    f4 &upper,
    f4 &lower) noexcept
{
    if (source == nullptr ||
        selector < 0)
        return false;

    auto &cached =
        steady_q_cache_for(
            source,
            selector);

    const std::uint8_t *record = nullptr;

    if (cached.view_valid &&
        cached.source == source &&
        cached.selector == selector) {
        // This function is called only after the exact steady packer original
        // returned. The live source object is therefore engine-attested for
        // this call. Refresh its two carrier pointers without VirtualQuery;
        // fall back to the fully validated slow path if either changed.
        const std::uint8_t *live_header = nullptr;
        const std::uint8_t *live_records = nullptr;

        std::memcpy(
            &live_header,
            static_cast<const std::uint8_t *>(
                source) + 0x18u,
            sizeof(live_header));
        std::memcpy(
            &live_records,
            static_cast<const std::uint8_t *>(
                source) + 0x20u,
            sizeof(live_records));

        if (live_header == cached.header &&
            live_records == cached.records &&
            live_header != nullptr &&
            live_records != nullptr) {
            std::uint16_t live_count = 0u;
            std::memcpy(
                &live_count,
                live_header + 0x0Au,
                sizeof(live_count));

            if (static_cast<std::uint32_t>(
                    selector) < live_count)
                record = cached.record;
            else
                cached = {};
        } else {
            cached = {};
        }
    }

    if (record == nullptr) {
        const std::uint8_t *header = nullptr;
        if (!safe_read(
                static_cast<const std::uint8_t *>(
                    source) + 0x18u,
                header) ||
            header == nullptr)
            return false;

        std::uint16_t count = 0u;
        if (!safe_read(
                header + 0x0Au,
                count) ||
            static_cast<std::uint32_t>(
                selector) >= count)
            return false;

        const std::uint8_t *records = nullptr;
        if (!safe_read(
                static_cast<const std::uint8_t *>(
                    source) + 0x20u,
                records) ||
            records == nullptr)
            return false;

        record =
            records +
            static_cast<std::size_t>(
                selector) *
            k_record_stride;

        // Validate both q vectors once for this exact live
        // (source,header,records,selector) tuple.
        if (!readable_range(
                record + k_q_upper_offset,
                (k_q_lower_offset -
                 k_q_upper_offset) +
                    sizeof(f4)))
            return false;

        cached = {};
        cached.source = source;
        cached.selector = selector;
        cached.header = header;
        cached.records = records;
        cached.record = record;
        cached.view_valid = true;
    }

    f4 q_upper{};
    f4 q_lower{};

    std::memcpy(
        &q_upper,
        record + k_q_upper_offset,
        sizeof(q_upper));
    std::memcpy(
        &q_lower,
        record + k_q_lower_offset,
        sizeof(q_lower));

    if (cached.sample_valid &&
        std::memcmp(
            &cached.q_upper,
            &q_upper,
            sizeof(q_upper)) == 0 &&
        std::memcmp(
            &cached.q_lower,
            &q_lower,
            sizeof(q_lower)) == 0) {
        upper = cached.upper;
        lower = cached.lower;
        return true;
    }

    f4 u{};
    f4 l{};

    if (!inverse_q(q_upper.x, u.x) ||
        !inverse_q(q_upper.y, u.y) ||
        !inverse_q(q_upper.z, u.z) ||
        !inverse_q(q_lower.x, l.x) ||
        !inverse_q(q_lower.y, l.y) ||
        !inverse_q(q_lower.z, l.z)) {
        cached.sample_valid = false;
        return false;
    }

    upper = {u.x,u.y,u.z,0.0f};
    lower = {l.x,l.y,l.z,0.0f};

    cached.q_upper = q_upper;
    cached.q_lower = q_lower;
    cached.upper = upper;
    cached.lower = lower;
    cached.sample_valid = true;
    return true;
}

f4 decode_rgbm(
    const raw_rgbm &value) noexcept
{
    const float scale =
        static_cast<float>(
            value.m) /
        100.0f;

    return {
        static_cast<float>(value.r) /
            255.0f * scale,
        static_cast<float>(value.g) /
            255.0f * scale,
        static_cast<float>(value.b) /
            255.0f * scale,
        0.0f
    };
}

f4 lerp4(
    const f4 &a,
    const f4 &b,
    float t) noexcept
{
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        0.0f
    };
}

bool float_bits_equal(
    float a,
    float b) noexcept
{
    return std::memcmp(
        &a,
        &b,
        sizeof(float)) == 0;
}

bool f4_bits_equal(
    const f4 &a,
    const f4 &b) noexcept
{
    return
        float_bits_equal(a.x, b.x) &&
        float_bits_equal(a.y, b.y) &&
        float_bits_equal(a.z, b.z) &&
        float_bits_equal(a.w, b.w);
}

bool payload_bits_equal(
    const std::array<f4,8> &a,
    const std::array<f4,8> &b) noexcept
{
    for (std::size_t i = 0u;
         i < a.size();
         ++i)
        if (!f4_bits_equal(
                a[i],
                b[i]))
            return false;

    return true;
}

bool snapshot_matches_candidate(
    const snapshot &current,
    const operators::lightbank::
        lightbank_snapshot_fingerprint &fingerprint,
    const std::array<f4,8> &ul_payload,
    const std::array<f4,8> &hemdir3_payload,
    const producer_tls &producer) noexcept
{
    if (!operators::lightbank::
            lightbank_snapshot_matches_draw(
                current.fingerprint,
                fingerprint) ||
        current.d123_ready !=
            producer.have_d123 ||
        current.pmetal_env_ready !=
            producer.have_pmetal_env ||
        !payload_bits_equal(
            current.ul_payload,
            ul_payload) ||
        !payload_bits_equal(
            current.hemdir3_payload,
            hemdir3_payload))
        return false;

    return
        f4_bits_equal(
            current.pmetal_env_a,
            producer.pmetal_env_a) &&
        f4_bits_equal(
            current.pmetal_env_b,
            producer.pmetal_env_b) &&
        float_bits_equal(
            current.pmetal_env_beta,
            producer.pmetal_env_beta) &&
        current.pmetal_bank_a ==
            producer.pmetal_bank_a &&
        current.pmetal_bank_b ==
            producer.pmetal_bank_b &&
        current.pmetal_row_a ==
            producer.pmetal_row_a &&
        current.pmetal_row_b ==
            producer.pmetal_row_b;
}

void publish_snapshot(
    const producer_tls &producer) noexcept
{
    if (!producer.active ||
        producer.owner == 0u ||
        producer.assignment == nullptr ||
        !producer.have_upper ||
        !producer.have_lower)
        return;

    std::uint16_t selector_a = 0u;
    std::uint16_t selector_b = 0u;
    std::uint32_t beta_bits = 0u;

    if (!read_assignment_tuple(
            producer.assignment,
            selector_a,
            selector_b,
            beta_bits))
        return;

    const operators::lightbank::
        lightbank_snapshot_fingerprint fingerprint{
            producer.owner,
            selector_a,
            selector_b,
            beta_bits
        };

    std::array<f4,8> ul_payload{};
    std::array<f4,8> hemdir3_payload{};

    ul_payload[6] =
        producer.upper;
    ul_payload[7] =
        producer.lower;

    hemdir3_payload[6] =
        producer.upper;
    hemdir3_payload[7] =
        producer.lower;

    if (producer.have_d123) {
        for (std::size_t i = 0u;
             i < producer.d123.size();
             ++i) {
            const auto &lobe =
                producer.d123[i];

            hemdir3_payload[i] = {
                lobe.direction.x,
                lobe.direction.y,
                lobe.direction.z,
                0.0f
            };

            hemdir3_payload[3u + i] = {
                lobe.color.x,
                lobe.color.y,
                lobe.color.z,
                0.0f
            };
        }

        telemetry::hot_count(g_d123_snapshot_publish);
    }

    try {
        std::lock_guard<std::mutex> lock(
            g_snapshot_mutex);

        const auto found =
            g_snapshots.find(
                producer.owner);

        // Repeated wrapper calls commonly republish the same immutable
        // LightBank tuple. Replacing the shared snapshot discarded its
        // already-realized b13 and forced another ID3D11Buffer::CreateBuffer
        // on the next draw. Preserve the exact existing snapshot (and its GPU
        // buffers) only when fingerprint and every transported payload bit are
        // identical. Any selector/beta/U/L/D123/P_Metal change still creates a
        // fresh snapshot and follows the original freshness path.
        if (found != g_snapshots.end() &&
            found->second &&
            snapshot_matches_candidate(
                *found->second,
                fingerprint,
                ul_payload,
                hemdir3_payload,
                producer)) {
            telemetry::hot_count(g_snapshot_publish);
            return;
        }

        auto fresh =
            std::make_shared<snapshot>();

        fresh->fingerprint =
            fingerprint;
        fresh->ul_payload =
            ul_payload;
        fresh->hemdir3_payload =
            hemdir3_payload;
        fresh->d123_ready =
            producer.have_d123;

        fresh->pmetal_env_ready =
            producer.have_pmetal_env;
        fresh->pmetal_env_a =
            producer.pmetal_env_a;
        fresh->pmetal_env_b =
            producer.pmetal_env_b;
        fresh->pmetal_env_beta =
            producer.pmetal_env_beta;
        fresh->pmetal_bank_a =
            producer.pmetal_bank_a;
        fresh->pmetal_bank_b =
            producer.pmetal_bank_b;
        fresh->pmetal_row_a =
            producer.pmetal_row_a;
        fresh->pmetal_row_b =
            producer.pmetal_row_b;

        g_snapshots[producer.owner] =
            std::move(fresh);
        g_snapshot_epoch.fetch_add(
            1u,
            std::memory_order_release);

        telemetry::hot_count(g_snapshot_publish);
    } catch (...) {
    }
}

void *run_wrapper(
    wrapper_fn original,
    std::atomic<std::uint64_t> &counter,
    void *rcx,
    void *owner,
    void *assignment,
    float x) noexcept
{
    telemetry::hot_count(counter);

    const auto previous =
        g_producer;

    g_producer = {};
    g_producer.active = true;
    g_producer.owner =
        reinterpret_cast<std::uintptr_t>(
            owner);
    g_producer.assignment =
        static_cast<const std::uint8_t *>(
            assignment);

    void *result =
        original != nullptr
            ? original(
                rcx,
                owner,
                assignment,
                x)
            : nullptr;

    const auto completed =
        g_producer;
    g_producer =
        previous;

    // Steady U/L is now materialized in the LightBank cache builder and
    // true interior blend is written directly by hook_blend_packer. Do not
    // republish the old D123/P_Metal/U-L snapshot stack on every wrapper:
    // owner runtime proved that this extended stack is the geometry-scaled
    // performance failure while direct U/L alone is not.
    if (g_steady_cache_builder_active.load(
            std::memory_order_acquire)) {
        publish_reference_token(
            completed);
        return result;
    }

    if (!completed.have_upper ||
        !completed.have_lower) {
        std::lock_guard<std::mutex> lock(
            g_snapshot_mutex);
        if (g_snapshots.erase(
                completed.owner) != 0u)
            g_snapshot_epoch.fetch_add(
                1u,
                std::memory_order_release);
    } else {
        publish_snapshot(
            completed);
    }

    return result;
}

void *__fastcall hook_wrapper5(
    void *rcx,
    void *owner,
    void *assignment,
    float x) noexcept
{
    return run_wrapper(
        g_wrapper5_orig,
        g_wrapper5,
        rcx,
        owner,
        assignment,
        x);
}

void *__fastcall hook_wrapper6(
    void *rcx,
    void *owner,
    void *assignment,
    float x) noexcept
{
    return run_wrapper(
        g_wrapper6_orig,
        g_wrapper6,
        rcx,
        owner,
        assignment,
        x);
}

void write_direct_ul_rgb(
    void *dst,
    const f4 &upper,
    const f4 &lower) noexcept
{
    // The exact producer functions have just written both float4 lanes, so
    // dst is engine-attested here. Preserve DSR's W lanes and replace only
    // RGB, matching the PTDE U/L operator's authored color semantics.
    auto *bytes =
        static_cast<std::uint8_t *>(dst);

    f4 out_upper{};
    f4 out_lower{};
    std::memcpy(
        &out_upper,
        bytes + k_q_upper_offset,
        sizeof(out_upper));
    std::memcpy(
        &out_lower,
        bytes + k_q_lower_offset,
        sizeof(out_lower));

    out_upper.x = upper.x;
    out_upper.y = upper.y;
    out_upper.z = upper.z;
    out_lower.x = lower.x;
    out_lower.y = lower.y;
    out_lower.z = lower.z;

    std::memcpy(
        bytes + k_q_upper_offset,
        &out_upper,
        sizeof(out_upper));
    std::memcpy(
        bytes + k_q_lower_offset,
        &out_lower,
        sizeof(out_lower));
}

void __fastcall hook_steady_eval_tail(
    const void *record,
    void *dst,
    std::int32_t selector,
    void *source) noexcept
{
    // Complete the original evaluator first. The tail trampoline continues
    // through the original RET and returns here before the caller copies
    // dst+0x60/+0x70 into renderer state +0x12A0/+0x12B0.
    if (g_steady_eval_tail_orig != nullptr)
        g_steady_eval_tail_orig(
            record,
            dst,
            selector,
            source);

    if (g_steady_cache_builder_active.load(
            std::memory_order_acquire))
        return;

    if (!g_direct_ul_producer_active.load(
            std::memory_order_acquire) ||
        !g_producer.active ||
        g_core == nullptr ||
        dst == nullptr ||
        source == nullptr) {
        return;
    }

    // The integrated addon enables upper_lower before install() and uninstalls
    // this producer before disabling the feature registry. Therefore
    // g_direct_ul_producer_active is the hot-path activation token. Do not
    // call feature_registry::enabled() here: it takes a std::mutex and this
    // hook runs once per steady producer evaluation.

    f4 upper{};
    f4 lower{};
    if (!read_selected_ptde(
            source,
            selector,
            upper,
            lower)) {
        telemetry::hot_count(g_direct_ul_inject_fail);
        return;
    }

    // Reuse this exact decoded sample in hook_steady_packer. Before the
    // direct-producer path existed, the packer performed one PTDE decode per
    // steady event. The tail hook had accidentally added a second identical
    // read_selected_ptde() on the same event.
    g_producer.upper = upper;
    g_producer.lower = lower;
    g_producer.have_upper = true;
    g_producer.have_lower = true;

    write_direct_ul_rgb(
        dst,
        upper,
        lower);
    telemetry::hot_count(g_direct_ul_steady_inject);
}

void __fastcall hook_steady_packer(
    void *source,
    void *dst,
    std::int32_t selector) noexcept
{
    telemetry::hot_count(g_steady_seen);

    if (g_steady_packer_orig != nullptr)
        g_steady_packer_orig(
            source,
            dst,
            selector);

    if (g_steady_cache_builder_active.load(
            std::memory_order_acquire)) {
        if (g_producer.active &&
            capture_source_reference(
                source,
                selector,
                source,
                selector,
                0.0f,
                g_producer) &&
            capture_evaluated_vectors(
                dst,
                g_producer))
            telemetry::hot_count(
                g_steady_pass);
        return;
    }

    if (!g_producer.active)
        return;

    if (g_pmetal_env_hook_armed.load()) {
        f4 env{};
        std::uint64_t bank = 0u;
        std::uint32_t row = 0u;

        if (read_exact_pmetal_env_source(
                source,
                selector,
                env,
                bank,
                row)) {
            g_producer.have_pmetal_env = true;
            g_producer.pmetal_env_a = env;
            g_producer.pmetal_env_b = env;
            g_producer.pmetal_env_beta = 0.0f;
            g_producer.pmetal_bank_a = bank;
            g_producer.pmetal_bank_b = bank;
            g_producer.pmetal_row_a = row;
            g_producer.pmetal_row_b = row;
            telemetry::hot_count(g_pmetal_env_steady);
        } else {
            telemetry::hot_count(g_pmetal_env_miss);
        }
    }

    // The direct steady-evaluator tail runs inside g_steady_packer_orig and
    // already decoded the exact same selected record. Reuse that event-local
    // sample instead of traversing and validating the carrier a second time.
    // If direct injection is unavailable or its decode failed, preserve the
    // original fail-open capture path.
    if (!g_producer.have_upper ||
        !g_producer.have_lower) {
        f4 upper{};
        f4 lower{};

        if (!read_selected_ptde(
                source,
                selector,
                upper,
                lower))
            return;

        g_producer.upper = upper;
        g_producer.lower = lower;
        g_producer.have_upper = true;
        g_producer.have_lower = true;
    }

    const auto *raw =
        resolve_raw_lightbank_record(
            source,
            selector);

    if (evaluate_raw_d123(
            raw,
            raw,
            0.0f,
            g_producer.d123)) {
        g_producer.have_d123 = true;
        telemetry::hot_count(g_d123_steady);
    }

    telemetry::hot_count(g_steady_pass);
}

void *__fastcall hook_blend(
    void *dst,
    const void *a,
    const void *b,
    float beta) noexcept
{
    telemetry::hot_count(g_blend_seen);

#if defined(_MSC_VER)
    const auto return_address =
        reinterpret_cast<std::uintptr_t>(
            _ReturnAddress());
#else
    const auto return_address =
        reinterpret_cast<std::uintptr_t>(
            __builtin_return_address(0));
#endif

    const auto rva =
        return_address >= g_base
            ? return_address - g_base
            : 0u;

    if (g_producer.active &&
        a != nullptr &&
        b != nullptr &&
        (rva == k_ret_blend_upper ||
         rva == k_ret_blend_lower)) {
        raw_rgbm raw_a{};
        raw_rgbm raw_b{};

        if (safe_read(a, raw_a) &&
            safe_read(b, raw_b)) {
            const auto value =
                lerp4(
                    decode_rgbm(raw_a),
                    decode_rgbm(raw_b),
                    beta);

            if (std::isfinite(value.x) &&
                std::isfinite(value.y) &&
                std::isfinite(value.z)) {
                if (rva ==
                    k_ret_blend_upper) {
                    g_producer.upper = value;
                    g_producer.have_upper = true;
                    telemetry::hot_count(g_blend_upper);
                } else {
                    g_producer.lower = value;
                    g_producer.have_lower = true;
                    telemetry::hot_count(g_blend_lower);
                }
            }
        }
    }

    return g_blend_orig != nullptr
        ? g_blend_orig(
            dst,
            a,
            b,
            beta)
        : nullptr;
}

void *__fastcall hook_blend_packer(
    void *dst,
    void *source_a,
    std::int32_t selector_a,
    void *source_b,
    std::int32_t selector_b,
    float beta) noexcept
{
    void *result =
        g_blend_packer_orig != nullptr
            ? g_blend_packer_orig(
                dst,
                source_a,
                selector_a,
                source_b,
                selector_b,
                beta)
            : nullptr;

    if (!g_producer.active)
        return result;

    if (g_direct_ul_producer_active.load(
            std::memory_order_acquire) &&
        g_core != nullptr &&
        dst != nullptr &&
        g_producer.have_upper &&
        g_producer.have_lower) {
        write_direct_ul_rgb(
            dst,
            g_producer.upper,
            g_producer.lower);
        telemetry::hot_count(g_direct_ul_blend_inject);
    }

    // In cache-builder mode carry only source references plus the already
    // evaluated direction/U-L vectors. D123 colors and P_Metal donor lookup
    // are deferred until their exact consumer gates.
    if (g_steady_cache_builder_active.load(
            std::memory_order_acquire)) {
        if (capture_source_reference(
                source_a,
                selector_a,
                source_b,
                selector_b,
                beta,
                g_producer))
            (void)capture_evaluated_vectors(
                dst,
                g_producer);
        return result;
    }

    const auto *raw_a =
        resolve_raw_lightbank_record(
            source_a,
            selector_a);
    const auto *raw_b =
        resolve_raw_lightbank_record(
            source_b,
            selector_b);

    const std::uint8_t *eval_a = raw_a;
    const std::uint8_t *eval_b = raw_b;
    float eval_beta = beta;

    if (raw_a != nullptr &&
        (selector_a == selector_b ||
         raw_b == nullptr ||
         beta <= 0.0f)) {
        eval_b = raw_a;
        eval_beta = 0.0f;
    } else if (
        raw_b != nullptr &&
        (raw_a == nullptr ||
         beta >= 1.0f)) {
        eval_a = raw_b;
        eval_b = raw_b;
        eval_beta = 0.0f;
    }

    if (evaluate_raw_d123(
            eval_a,
            eval_b,
            eval_beta,
            g_producer.d123)) {
        g_producer.have_d123 = true;
        g_d123_blend_direction += 3u;
        g_d123_blend_color += 3u;
    }

    return result;
}

void __fastcall hook_pmetal_env_blend(
    float *out,
    void *source_a,
    std::int32_t selector_a,
    void *source_b,
    std::int32_t selector_b,
    float beta) noexcept
{
    if (g_pmetal_env_blend_orig != nullptr)
        g_pmetal_env_blend_orig(
            out,
            source_a,
            selector_a,
            source_b,
            selector_b,
            beta);

    if (!g_producer.active ||
        !g_pmetal_env_hook_armed.load())
        return;

    f4 a{};
    f4 b{};
    std::uint64_t bank_a = 0u;
    std::uint64_t bank_b = 0u;
    std::uint32_t row_a = 0u;
    std::uint32_t row_b = 0u;

    if (read_exact_pmetal_env_source(
            source_a,
            selector_a,
            a,
            bank_a,
            row_a) &&
        read_exact_pmetal_env_source(
            source_b,
            selector_b,
            b,
            bank_b,
            row_b) &&
        std::isfinite(beta)) {
        g_producer.have_pmetal_env = true;
        g_producer.pmetal_env_a = a;
        g_producer.pmetal_env_b = b;
        // V13 producer contract publishes clamp01(beta) into b12[3].w.
        // Preserve that operator behavior rather than exporting an unchecked
        // transport value.
        g_producer.pmetal_env_beta =
            std::clamp(
                beta,
                0.0f,
                1.0f);
        g_producer.pmetal_bank_a = bank_a;
        g_producer.pmetal_bank_b = bank_b;
        g_producer.pmetal_row_a = row_a;
        g_producer.pmetal_row_b = row_b;
        telemetry::hot_count(g_pmetal_env_blend);
    } else {
        g_producer.have_pmetal_env = false;
        telemetry::hot_count(g_pmetal_env_miss);
    }
}

bool install_optional_pmetal_env_hook() noexcept
{
    if (!prepare_hook(
            g_pmetal_env_hook,
            k_rva_pmetal_env_blend,
            k_pmetal_env_blend_bytes,
            reinterpret_cast<void *>(
                &hook_pmetal_env_blend)))
        return false;

    g_pmetal_env_blend_orig =
        reinterpret_cast<pmetal_env_blend_fn>(
            g_pmetal_env_hook.trampoline);

    if (!arm_hook(
            g_pmetal_env_hook)) {
        (void)restore_hook(
            g_pmetal_env_hook);
        g_pmetal_env_blend_orig = nullptr;
        return false;
    }

    g_pmetal_env_hook_armed.store(true);
    return true;
}

bool install_producer_hooks() noexcept
{
    if (g_base == 0u)
        return false;

    if (!prepare_hook(
            g_hooks[0],
            k_rva_wrapper_type5,
            k_wrapper_bytes,
            reinterpret_cast<void *>(
                &hook_wrapper5)) ||
        !prepare_hook(
            g_hooks[1],
            k_rva_wrapper_type6,
            k_wrapper_bytes,
            reinterpret_cast<void *>(
                &hook_wrapper6)) ||
        !prepare_hook(
            g_hooks[2],
            k_rva_blend_helper,
            k_blend_bytes,
            reinterpret_cast<void *>(
                &hook_blend)) ||
        !prepare_hook(
            g_hooks[3],
            k_rva_steady_packer,
            k_steady_packer_bytes,
            reinterpret_cast<void *>(
                &hook_steady_packer)) ||
        !prepare_hook(
            g_hooks[4],
            k_rva_blend_packer,
            k_blend_packer_bytes,
            reinterpret_cast<void *>(
                &hook_blend_packer)) ||
        !prepare_hook(
            g_steady_eval_tail_hook,
            k_rva_steady_eval_tail,
            k_steady_eval_tail_bytes,
            reinterpret_cast<void *>(
                &hook_steady_eval_tail)) ||
        !prepare_hook(
            g_steady_cache_builder_hook,
            k_rva_steady_cache_builder,
            k_steady_cache_builder_bytes,
            reinterpret_cast<void *>(
                &hook_steady_cache_builder))) {
        return false;
    }

    g_wrapper5_orig =
        reinterpret_cast<wrapper_fn>(
            g_hooks[0].trampoline);
    g_wrapper6_orig =
        reinterpret_cast<wrapper_fn>(
            g_hooks[1].trampoline);
    g_blend_orig =
        reinterpret_cast<blend_fn>(
            g_hooks[2].trampoline);
    g_steady_packer_orig =
        reinterpret_cast<steady_packer_fn>(
            g_hooks[3].trampoline);
    g_steady_eval_tail_orig =
        reinterpret_cast<steady_eval_tail_fn>(
            g_steady_eval_tail_hook.trampoline);
    g_blend_packer_orig =
        reinterpret_cast<lightbank_blend_packer_fn>(
            g_hooks[4].trampoline);
    g_steady_cache_builder_orig =
        reinterpret_cast<steady_cache_builder_fn>(
            g_steady_cache_builder_hook.trampoline);

    // Cache-builder architecture: wrapper5/6 + blend helper + blend
    // packer are required for true interior blend. The steady packer
    // (g_hooks[3], RVA 0x563B80) is armed only as a reference-token tap:
    // it copies engine-attested source/selector and already-evaluated vectors
    // without D123/P_Metal decoding. Evaluator-tail 0x5634E7 stays unarmed.
    // Steady U/L correction itself is carried exclusively by 0x563590.
    if (!arm_hook(g_hooks[0]) ||
        !arm_hook(g_hooks[1]) ||
        !arm_hook(g_hooks[2]) ||
        !arm_hook(g_hooks[3]) ||
        !arm_hook(g_hooks[4]) ||
        !arm_hook(
            g_steady_cache_builder_hook))
        return false;

    g_steady_cache_builder_active.store(
        true,
        std::memory_order_release);

    // Only after both the steady cache builder and the already-owned blend
    // packer hook are armed may integrated routing bypass draw-time U/L.
    g_direct_ul_producer_active.store(
        true,
        std::memory_order_release);

    // P_Metal/HemDir3 capture must not piggyback on steady U/L evaluation.
    // Owner runtime isolated that shared semantic-capture stack as the
    // geometry-scaled bottleneck. Those operators fail open to stock DSR
    // until they get their own producer-local carriers.
    g_pmetal_env_hook_armed.store(false);

    return true;
}

bool restore_producer_hooks() noexcept
{
    g_direct_ul_producer_active.store(
        false,
        std::memory_order_release);
    g_steady_cache_builder_active.store(
        false,
        std::memory_order_release);

    bool ok =
        restore_hook(
            g_pmetal_env_hook);

    g_pmetal_env_hook_armed.store(false);

    if (ok)
        g_pmetal_env_blend_orig = nullptr;

    ok =
        restore_hook(
            g_steady_cache_builder_hook) &&
        ok;
    ok =
        restore_hook(
            g_steady_eval_tail_hook) &&
        ok;

    for (auto it = g_hooks.rbegin();
         it != g_hooks.rend();
         ++it)
        ok = restore_hook(*it) && ok;

    if (ok) {
        g_wrapper5_orig = nullptr;
        g_wrapper6_orig = nullptr;
        g_blend_orig = nullptr;
        g_steady_packer_orig = nullptr;
        g_steady_eval_tail_orig = nullptr;
        g_steady_cache_builder_orig = nullptr;
        g_blend_packer_orig = nullptr;
    }

    return ok;
}

ID3D11Buffer *realize_b13_payload(
    const std::array<f4,8> &payload,
    ID3D11DeviceContext *context,
    bool hemdir3_combined) noexcept
{
    if (context == nullptr)
        return nullptr;

    auto *slot =
        resolve_b13_upload_slot(context);
    if (slot == nullptr ||
        slot->device == nullptr)
        return nullptr;

    auto *&buffer =
        hemdir3_combined
            ? slot->hemdir3_buffer
            : slot->ul_buffer;

    auto &last =
        hemdir3_combined
            ? slot->hemdir3_last
            : slot->ul_last;

    bool &last_valid =
        hemdir3_combined
            ? slot->hemdir3_last_valid
            : slot->ul_last_valid;

    bool created = false;

    if (buffer == nullptr) {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth =
            static_cast<UINT>(
                sizeof(payload));
        desc.Usage =
            D3D11_USAGE_DEFAULT;
        desc.BindFlags =
            D3D11_BIND_CONSTANT_BUFFER;

        if (FAILED(slot->device->CreateBuffer(
                &desc,
                nullptr,
                &buffer)) ||
            buffer == nullptr)
            return nullptr;

        created = true;
        last_valid = false;
    }

    const bool payload_unchanged =
        last_valid &&
        std::memcmp(
            last.data(),
            payload.data(),
            sizeof(payload)) == 0;

    if (!payload_unchanged) {
        context->UpdateSubresource(
            buffer,
            0u,
            nullptr,
            payload.data(),
            0u,
            0u);

        last = payload;
        last_valid = true;
    }

    if (hemdir3_combined) {
        if (created)
            telemetry::hot_count(
                g_hemdir3_b13_create);
        else
            telemetry::hot_count(
                g_hemdir3_b13_hit);
    } else {
        if (created)
            telemetry::hot_count(
                g_b13_create);
        else
            telemetry::hot_count(
                g_b13_hit);
    }

    return buffer;
}

ID3D11Buffer *realize_b13(
    const std::shared_ptr<const snapshot> &selected,
    ID3D11DeviceContext *context,
    bool hemdir3_combined) noexcept
{
    if (!selected ||
        (hemdir3_combined &&
         !selected->d123_ready))
        return nullptr;

    return realize_b13_payload(
        hemdir3_combined
            ? selected->hemdir3_payload
            : selected->ul_payload,
        context,
        hemdir3_combined);
}

void clear_snapshots() noexcept
{
    g_draw_snapshot.reset();
    g_draw_reference_token = {};
    g_reference_tokens = {};
    g_reference_token_victim = {};
    clear_b13_upload_slots();
    g_selector_snapshot = {};
    g_selector_window = {};
    g_assignment_window = {};

    std::lock_guard<std::mutex> lock(
        g_snapshot_mutex);
    g_snapshots.clear();
    g_snapshot_epoch.fetch_add(
        1u,
        std::memory_order_release);
}

} // namespace

upper_lower_draw_runtime::upper_lower_draw_runtime(
    core::renderer_core &core) noexcept
    : core_(core)
{
}

void upper_lower_selector_event_bridge(
    void *owner,
    void *return_address,
    void *r14,
    void *r15) noexcept
{
    if (g_runtime != nullptr)
        g_runtime->selector_event(
            owner,
            return_address,
            r14,
            r15);

    // Fixed PointLight uses the exact same owner_context association as the
    // already-owned selector hook. Keep this bridge independent of U/L
    // activation; the PointLight runtime is inert when not installed.
    fixed_pointlight_selector_event_bridge(owner);
}

bool upper_lower_draw_runtime::install() noexcept
{
    if (g_enabled.load())
        return g_runtime == this;

    if (g_runtime != nullptr &&
        g_runtime != this)
        return false;

    const auto selector_status =
        flver_identity_transport::status();

    if (!selector_status.provenance_ok ||
        !selector_status.selector_armed)
        return false;

    g_core = &core_;
    g_runtime = this;
    g_base =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));

    if (g_base == 0u ||
        !install_producer_hooks()) {
        (void)restore_producer_hooks();
        g_core = nullptr;
        g_runtime = nullptr;
        g_base = 0u;
        return false;
    }

    g_quarantined.store(false);
    g_restore_failed.store(false);
    g_enabled.store(true);
    return true;
}

void upper_lower_draw_runtime::uninstall() noexcept
{
    g_enabled.store(false);
    consume_draw_selection();
    clear_snapshots();

    if (!restore_producer_hooks()) {
        g_restore_failed.store(true);
        g_quarantined.store(true);
        return;
    }

    g_core = nullptr;
    g_runtime = nullptr;
    g_base = 0u;
}

void upper_lower_draw_runtime::selector_event(
    void *owner,
    void *return_address,
    void *r14,
    void *r15) noexcept
{
    telemetry::hot_count(g_selector_seen);
    g_draw_snapshot.reset();
    g_draw_reference_token = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        owner == nullptr ||
        return_address == nullptr ||
        g_base == 0u)
        return;

    const auto absolute =
        reinterpret_cast<std::uintptr_t>(
            return_address);

    if (absolute < g_base)
        return;

    const auto rva =
        absolute - g_base;

    const std::uint8_t *descriptor =
        nullptr;

    if (rva == k_ret_sel_1 ||
        rva == k_ret_sel_3)
        descriptor =
            static_cast<const std::uint8_t *>(
                r15);
    else if (rva == k_ret_sel_2)
        descriptor =
            static_cast<const std::uint8_t *>(
                r14);
    else
        return;

    if (descriptor == nullptr) {
        telemetry::hot_count(g_selector_miss);
        return;
    }

    if (g_steady_cache_builder_active.load(
            std::memory_order_acquire)) {
        const auto owner_key =
            reinterpret_cast<std::uintptr_t>(
                owner);

        auto *candidate =
            find_reference_token(
                owner_key);
        if (candidate == nullptr) {
            telemetry::hot_count(
                g_selector_miss);
            return;
        }

        std::uint16_t selector_a = 0u;
        std::uint16_t selector_b = 0u;
        std::uint32_t beta_bits = 0u;
        if (!read_selector_tuple_attested(
                descriptor,
                selector_a,
                selector_b,
                beta_bits)) {
            candidate->available = false;
            telemetry::hot_count(
                g_selector_miss);
            return;
        }

        const operators::lightbank::
            lightbank_snapshot_fingerprint draw{
                owner_key,
                selector_a,
                selector_b,
                beta_bits
            };

        if (!operators::lightbank::
                lightbank_snapshot_matches_draw(
                    candidate->fingerprint,
                    draw)) {
            candidate->available = false;
            telemetry::hot_count(
                g_tuple_mismatch);
            return;
        }

        g_draw_reference_token =
            *candidate;
        g_draw_reference_token.available =
            false;
        candidate->available = false;
        telemetry::hot_count(
            g_selector_match);
        return;
    }

    std::uint16_t selector_a = 0u;
    std::uint16_t selector_b = 0u;
    std::uint32_t beta_bits = 0u;

    if (!read_selector_tuple(
            descriptor,
            selector_a,
            selector_b,
            beta_bits)) {
        telemetry::hot_count(g_selector_miss);
        return;
    }

    const auto owner_key =
        reinterpret_cast<std::uintptr_t>(
            owner);
    const auto epoch =
        g_snapshot_epoch.load(
            std::memory_order_acquire);

    std::shared_ptr<const snapshot> selected{};

    if (g_selector_snapshot.owner ==
            owner_key &&
        g_selector_snapshot.epoch ==
            epoch) {
        selected =
            g_selector_snapshot.selected;
    } else {
        std::lock_guard<std::mutex> lock(
            g_snapshot_mutex);

        const auto found =
            g_snapshots.find(
                owner_key);

        if (found != g_snapshots.end())
            selected = found->second;

        g_selector_snapshot.owner =
            owner_key;
        g_selector_snapshot.epoch =
            g_snapshot_epoch.load(
                std::memory_order_relaxed);
        g_selector_snapshot.selected =
            selected;
    }

    if (!selected) {
        telemetry::hot_count(g_selector_miss);
        return;
    }

    const operators::lightbank::
        lightbank_snapshot_fingerprint draw{
            reinterpret_cast<std::uintptr_t>(
                owner),
            selector_a,
            selector_b,
            beta_bits
        };

    if (!operators::lightbank::
            lightbank_snapshot_matches_draw(
                selected->fingerprint,
                draw)) {
        telemetry::hot_count(g_tuple_mismatch);
        return;
    }

    g_draw_snapshot =
        std::move(selected);
    telemetry::hot_count(g_selector_match);
}

bool upper_lower_draw_runtime::direct_producer_active() const noexcept
{
    return
        g_enabled.load(std::memory_order_acquire) &&
        !g_quarantined.load(std::memory_order_acquire) &&
        g_direct_ul_producer_active.load(
            std::memory_order_acquire);
}

bool upper_lower_draw_runtime::prepare_upper_lower_carrier(
    ID3D11DeviceContext *context,
    prepared_upper_lower_draw &prepared) noexcept
{
    prepared = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        context == nullptr)
        return false;

    if (g_steady_cache_builder_active.load(
            std::memory_order_acquire)) {
        if (!g_draw_reference_token.valid)
            return false;

        std::array<f4,8> payload{};
        payload[6] =
            g_draw_reference_token.upper;
        payload[7] =
            g_draw_reference_token.lower;

        auto *b13 =
            realize_b13_payload(
                payload,
                context,
                false);
        if (b13 == nullptr)
            return false;

        prepared.b13 = b13;
        prepared.ready = true;
        telemetry::hot_count(g_requests);
        return true;
    }

    if (!g_draw_snapshot)
        return false;

    // install() is only reached after the integrated feature registry has
    // enabled upper_lower, and uninstall() runs before that registry is
    // disabled. g_enabled is therefore the lifecycle-equivalent hot token;
    // avoid the registry mutex on every carrier request.

    auto *b13 =
        realize_b13(
            g_draw_snapshot,
            context,
            false);

    if (b13 == nullptr)
        return false;

    // Producer-only contract: realize and retain the exact fresh b13
    // payload for consumer constant-buffer slot 13u. Receiver/shader ownership
    // is established by the consumer
    // adapter (ordinary HemEnv, MR+U/L, or another explicitly certified
    // consumer), never here.
    prepared.b13 = b13;
    prepared.ready = true;
    telemetry::hot_count(g_requests);
    return true;
}

bool upper_lower_draw_runtime::prepare_draw_request(
    ID3D11DeviceContext *context,
    std::uint32_t receiver_id,
    prepared_upper_lower_draw &prepared) noexcept
{
    if (receiver_id < 24u ||
        receiver_id > 47u)
        return false;

    if (!prepare_upper_lower_carrier(
            context,
            prepared))
        return false;

    // Legacy compatibility surface only. U/L now requires an exact consumer
    // PS plus b13, so a carrier-only request must not be dispatched.
    prepared.request = {};
    return true;
}

bool upper_lower_draw_runtime::prepare_hemdir3_carrier(
    ID3D11DeviceContext *context,
    prepared_hemdir3_carrier &prepared) noexcept
{
    prepared = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        context == nullptr)
        return false;

    if (g_steady_cache_builder_active.load(
            std::memory_order_acquire)) {
        if (!g_draw_reference_token.valid)
            return false;

        std::array<f4,8> payload{};
        if (!build_hemdir3_reference_payload(
                g_draw_reference_token,
                payload))
            return false;

        auto *b13 =
            realize_b13_payload(
                payload,
                context,
                true);
        if (b13 == nullptr)
            return false;

        prepared.b13 = b13;
        prepared.fingerprint =
            g_draw_reference_token.fingerprint;
        prepared.d123_ready = true;
        prepared.upper_lower_ready = true;
        prepared.ready = true;
        telemetry::hot_count(
            g_hemdir3_carrier_requests);
        return true;
    }

    if (!g_draw_snapshot ||
        !g_draw_snapshot->d123_ready)
        return false;

    auto *b13 =
        realize_b13(
            g_draw_snapshot,
            context,
            true);

    if (b13 == nullptr)
        return false;

    prepared.b13 = b13;
    prepared.fingerprint =
        g_draw_snapshot->fingerprint;
    prepared.d123_ready = true;
    prepared.upper_lower_ready = true;
    prepared.ready = true;
    telemetry::hot_count(g_hemdir3_carrier_requests);
    return true;
}

void upper_lower_draw_runtime::release_hemdir3_carrier(
    prepared_hemdir3_carrier &prepared) noexcept
{
    prepared = {};
}

bool upper_lower_draw_runtime::selected_pmetal_env_source(
    pmetal_env_source &out) const noexcept
{
    out = {};

    if (!g_enabled.load() ||
        g_quarantined.load())
        return false;

    if (g_steady_cache_builder_active.load(
            std::memory_order_acquire)) {
        const auto &token =
            g_draw_reference_token;
        if (!token.valid ||
            token.source_a == nullptr ||
            token.source_b == nullptr ||
            token.selector_a < 0 ||
            token.selector_b < 0 ||
            !std::isfinite(token.beta))
            return false;

        f4 a{};
        f4 b{};
        std::uint64_t bank_a = 0u;
        std::uint64_t bank_b = 0u;
        std::uint32_t row_a = 0u;
        std::uint32_t row_b = 0u;

        if (!read_exact_pmetal_env_source(
                token.source_a,
                token.selector_a,
                a,
                bank_a,
                row_a))
            return false;

        if (token.beta <= 0.0f) {
            b = a;
            bank_b = bank_a;
            row_b = row_a;
        } else if (!read_exact_pmetal_env_source(
                       token.source_b,
                       token.selector_b,
                       b,
                       bank_b,
                       row_b)) {
            return false;
        }

        if (token.beta >= 1.0f) {
            a = b;
            bank_a = bank_b;
            row_a = row_b;
        }

        out.a = {a.x,a.y,a.z};
        out.b = {b.x,b.y,b.z};
        out.beta =
            std::clamp(
                token.beta,
                0.0f,
                1.0f);
        out.bank_signature_a = bank_a;
        out.bank_signature_b = bank_b;
        out.row_id_a = row_a;
        out.row_id_b = row_b;

        return
            std::isfinite(out.a[0]) &&
            std::isfinite(out.a[1]) &&
            std::isfinite(out.a[2]) &&
            std::isfinite(out.b[0]) &&
            std::isfinite(out.b[1]) &&
            std::isfinite(out.b[2]);
    }

    if (!g_pmetal_env_hook_armed.load() ||
        !g_draw_snapshot ||
        !g_draw_snapshot->pmetal_env_ready)
        return false;

    out.a = {
        g_draw_snapshot->pmetal_env_a.x,
        g_draw_snapshot->pmetal_env_a.y,
        g_draw_snapshot->pmetal_env_a.z
    };
    out.b = {
        g_draw_snapshot->pmetal_env_b.x,
        g_draw_snapshot->pmetal_env_b.y,
        g_draw_snapshot->pmetal_env_b.z
    };
    out.beta =
        g_draw_snapshot->pmetal_env_beta;
    out.bank_signature_a =
        g_draw_snapshot->pmetal_bank_a;
    out.bank_signature_b =
        g_draw_snapshot->pmetal_bank_b;
    out.row_id_a =
        g_draw_snapshot->pmetal_row_a;
    out.row_id_b =
        g_draw_snapshot->pmetal_row_b;

    return
        std::isfinite(out.a[0]) &&
        std::isfinite(out.a[1]) &&
        std::isfinite(out.a[2]) &&
        std::isfinite(out.b[0]) &&
        std::isfinite(out.b[1]) &&
        std::isfinite(out.b[2]) &&
        std::isfinite(out.beta);
}

void upper_lower_draw_runtime::release_prepared_draw(
    prepared_upper_lower_draw &prepared) noexcept
{
    prepared = {};
}

void upper_lower_draw_runtime::consume_draw_selection() noexcept
{
    g_draw_snapshot.reset();
    g_draw_reference_token = {};
}

void upper_lower_draw_runtime::on_destroy_device(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11)
        return;

    clear_snapshots();
}

upper_lower_telemetry
upper_lower_draw_runtime::telemetry() const noexcept
{
    return {
        g_wrapper5.load(),
        g_wrapper6.load(),
        g_steady_seen.load(),
        g_steady_pass.load(),
        g_blend_seen.load(),
        g_blend_upper.load(),
        g_blend_lower.load(),
        g_d123_steady.load(),
        g_d123_blend_direction.load(),
        g_d123_blend_color.load(),
        g_d123_snapshot_publish.load(),
        g_snapshot_publish.load(),
        g_selector_seen.load(),
        g_selector_match.load(),
        g_selector_miss.load(),
        g_tuple_mismatch.load(),
        g_b13_create.load(),
        g_b13_hit.load(),
        g_hemdir3_b13_create.load(),
        g_hemdir3_b13_hit.load(),
        g_requests.load(),
        g_hemdir3_carrier_requests.load(),
        g_pmetal_env_steady.load(),
        g_pmetal_env_blend.load(),
        g_pmetal_env_miss.load(),
        g_direct_ul_steady_inject.load(),
        g_direct_ul_blend_inject.load(),
        g_direct_ul_inject_fail.load(),
        g_enabled.load(),
        g_pmetal_env_hook_armed.load(),
        g_direct_ul_producer_active.load(),
        g_quarantined.load(),
        g_restore_failed.load()
    };
}

void upper_lower_draw_runtime::reset() noexcept
{
    clear_snapshots();

    g_wrapper5.store(0);
    g_wrapper6.store(0);
    g_steady_seen.store(0);
    g_steady_pass.store(0);
    g_blend_seen.store(0);
    g_blend_upper.store(0);
    g_blend_lower.store(0);
    g_d123_steady.store(0);
    g_d123_blend_direction.store(0);
    g_d123_blend_color.store(0);
    g_d123_snapshot_publish.store(0);
    g_snapshot_publish.store(0);
    g_selector_seen.store(0);
    g_selector_match.store(0);
    g_selector_miss.store(0);
    g_tuple_mismatch.store(0);
    g_b13_create.store(0);
    g_b13_hit.store(0);
    g_hemdir3_b13_create.store(0);
    g_hemdir3_b13_hit.store(0);
    g_requests.store(0);
    g_hemdir3_carrier_requests.store(0);
    g_pmetal_env_steady.store(0);
    g_pmetal_env_blend.store(0);
    g_pmetal_env_miss.store(0);
    g_direct_ul_steady_inject.store(0);
    g_direct_ul_blend_inject.store(0);
    g_direct_ul_inject_fail.store(0);
}

} // namespace dsrrl::runtime
