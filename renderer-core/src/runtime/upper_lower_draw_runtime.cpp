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
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
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

constexpr std::uint32_t k_pmetal_material_route = 345u;
constexpr const char *k_pmetal_material_name =
    "P_Metal[DSB].mtd";
constexpr const char *k_pmetal_material_sha256 =
    "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b";

struct f4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};


bool retail_lightbank_record_index(
    std::int32_t selector,
    std::uint32_t &index) noexcept
{
    index = 0u;
    if (selector < 0)
        return false;

    // Retail DSR record selection is byte-indexed, not int-indexed.
    // DarkSoulsRemastered.exe:
    //   0x140563B98 test r8d,r8d
    //   0x140563BA2 movzx edx,r8b
    // and the blend path repeats the same rule at 0x140563CF1/CFB
    // (and 0x14056389A/8A4 for the sibling packer).
    // The upper bits remain part of selector/freshness identity, but the
    // LightBank row-table consumer receives only the low byte.
    index =
        static_cast<std::uint32_t>(
            static_cast<std::uint8_t>(
                selector));
    return true;
}

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
    bool direct_ul_applied = false;
    std::array<f4,3> evaluated_directions{};
};

struct lightbank_reference_token {
    operators::lightbank::lightbank_snapshot_fingerprint fingerprint{};
    // Exact selector->draw cross-thread handoff identity. The producer thread
    // and monotonically unique publication serial are carried with the same
    // owner/selectorA/selectorB/beta fingerprint; they never replace it.
    std::uint32_t producer_tid = 0u;
    std::uint64_t producer_serial = 0u;
    void *source_a = nullptr;
    void *source_b = nullptr;
    std::int32_t selector_a = -1;
    std::int32_t selector_b = -1;
    float beta = 0.0f;
    // P_Metal donor source is decoded only after the exact selector+material
    // semantic join, while the engine source tuple is still the authenticated
    // input. Draw-time replay consumes this immutable payload instead of
    // dereferencing producer-owned source pointers later.
    pmetal_env_source pmetal_env{};
    bool pmetal_env_ready = false;
    std::array<f4,3> directions{};
    f4 upper{};
    f4 lower{};
    // A token is exact/fresh by fingerprint independently of which operator
    // payloads the producer happened to materialize. Keep source, evaluated
    // vectors and U/L readiness separate so an unrelated carrier failure
    // cannot suppress P_Metal EnvSpec source routing.
    bool source_ready = false;
    bool vectors_ready = false;
    bool upper_lower_ready = false;
    bool direct_ul_applied = false;
    bool valid = false;
    bool available = false;
};

struct pmetal_source_probe {
    pmetal_env_source_diag_status status =
        pmetal_env_source_diag_status::none;
    std::int32_t selector = -1;
    std::uint16_t bank_count = 0u;
    std::uint64_t bank_signature = 0u;
    std::uint32_t row_id = 0u;
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

thread_local std::array<pmetal_bank_cache_entry,k_steady_cache_entries>
    g_pmetal_bank_cache{};
thread_local std::array<std::uint8_t,k_steady_cache_sets>
    g_pmetal_bank_victim{};

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

constexpr std::size_t k_direct_ul_cache_stamp_sets = 2048u;
constexpr std::size_t k_direct_ul_cache_stamp_ways = 2u;
constexpr std::uintptr_t k_direct_ul_cache_empty = 0u;
constexpr std::uintptr_t k_direct_ul_cache_busy = 1u;

struct direct_ul_cache_stamp_slot {
    std::atomic<std::uintptr_t> record{
        k_direct_ul_cache_empty};
    std::array<std::atomic<std::uint32_t>,6>
        rgb_bits{};
};

std::array<
    direct_ul_cache_stamp_slot,
    k_direct_ul_cache_stamp_sets *
        k_direct_ul_cache_stamp_ways>
    g_direct_ul_cache_stamps{};

std::size_t direct_ul_cache_stamp_set(
    const void *record) noexcept
{
    const auto key =
        reinterpret_cast<std::uintptr_t>(
            record);
    return static_cast<std::size_t>(
        ((key >> 4u) ^
         (key >> 13u) ^
         (key >> 23u)) &
        (k_direct_ul_cache_stamp_sets - 1u));
}

bool direct_ul_cache_rgb_bits(
    const void *record,
    std::array<std::uint32_t,6> &bits) noexcept
{
    bits = {};
    if (record == nullptr)
        return false;

    const auto *bytes =
        static_cast<const std::uint8_t *>(
            record);
    std::memcpy(
        bits.data(),
        bytes + k_q_upper_offset,
        3u * sizeof(std::uint32_t));
    std::memcpy(
        bits.data() + 3u,
        bytes + k_q_lower_offset,
        3u * sizeof(std::uint32_t));
    return true;
}

bool mark_direct_ul_cache_record(
    const void *record) noexcept
{
    const auto key =
        reinterpret_cast<std::uintptr_t>(
            record);
    if (key <= k_direct_ul_cache_busy)
        return false;

    std::array<std::uint32_t,6> bits{};
    if (!direct_ul_cache_rgb_bits(
            record,
            bits))
        return false;

    const auto base =
        direct_ul_cache_stamp_set(record) *
        k_direct_ul_cache_stamp_ways;

    for (std::size_t way = 0u;
         way < k_direct_ul_cache_stamp_ways;
         ++way) {
        auto &slot =
            g_direct_ul_cache_stamps[
                base + way];

        auto observed =
            slot.record.load(
                std::memory_order_acquire);
        if (observed != key &&
            observed != k_direct_ul_cache_empty)
            continue;

        auto expected = observed;
        if (!slot.record.compare_exchange_strong(
                expected,
                k_direct_ul_cache_busy,
                std::memory_order_acq_rel,
                std::memory_order_acquire))
            continue;

        for (std::size_t i = 0u;
             i < bits.size();
             ++i)
            slot.rgb_bits[i].store(
                bits[i],
                std::memory_order_relaxed);

        slot.record.store(
            key,
            std::memory_order_release);
        return true;
    }

    // Collision pressure never authorizes the producer shortcut. A missing
    // stamp simply forces the exact b13 draw bridge for the selected draw.
    return false;
}

bool direct_ul_cache_record_matches(
    const void *record) noexcept
{
    const auto key =
        reinterpret_cast<std::uintptr_t>(
            record);
    if (key <= k_direct_ul_cache_busy)
        return false;

    std::array<std::uint32_t,6> current{};
    if (!direct_ul_cache_rgb_bits(
            record,
            current))
        return false;

    const auto base =
        direct_ul_cache_stamp_set(record) *
        k_direct_ul_cache_stamp_ways;

    for (std::size_t way = 0u;
         way < k_direct_ul_cache_stamp_ways;
         ++way) {
        const auto &slot =
            g_direct_ul_cache_stamps[
                base + way];

        const auto before =
            slot.record.load(
                std::memory_order_acquire);
        if (before != key)
            continue;

        bool equal = true;
        for (std::size_t i = 0u;
             i < current.size();
             ++i)
            if (slot.rgb_bits[i].load(
                    std::memory_order_relaxed) !=
                current[i]) {
                equal = false;
                break;
            }

        const auto after =
            slot.record.load(
                std::memory_order_acquire);
        return after == key && equal;
    }

    return false;
}

void clear_direct_ul_cache_stamps() noexcept
{
    for (auto &slot :
         g_direct_ul_cache_stamps)
        slot.record.store(
            k_direct_ul_cache_empty,
            std::memory_order_release);
}

const std::uint8_t *
selected_cache_record_attested(
    void *source,
    std::int32_t selector) noexcept
{
    if (source == nullptr ||
        selector < 0)
        return nullptr;

    const std::uint8_t *records = nullptr;
    std::memcpy(
        &records,
        static_cast<const std::uint8_t *>(
            source) + 0x20u,
        sizeof(records));
    if (records == nullptr)
        return nullptr;

    std::uint32_t retail_index = 0u;
    if (!retail_lightbank_record_index(
            selector,
            retail_index))
        return nullptr;

    const auto index =
        static_cast<std::size_t>(
            retail_index);
    return records +
        index * k_record_stride;
}

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
// a bounded cross-thread registry carrying only references plus the
// exact freshness fingerprint. Each set has a tiny spin guard: no heap,
// shared_ptr, global unordered_map or global epoch participates in this path.
//
// Keep this carrier sized like the proven steady hot caches. The old 32x2
// registry was vulnerable to collision eviction in geometry-heavy scenes:
// a valid producer token could disappear before the exact selector event and
// surface later as TOKEN_INVALID at the P_Metal EnvSpec source gate. 128x4
// remains bounded (~512 tokens), preserves exact owner/selector/beta matching,
// and changes capacity only -- never identity or fail-open semantics.
// The selected draw token remains TLS because it is consumed in draw order.
constexpr std::size_t k_reference_token_sets = 128u;
constexpr std::size_t k_reference_token_ways = 4u;
constexpr std::size_t k_reference_token_entries =
    k_reference_token_sets * k_reference_token_ways;
std::array<
    lightbank_reference_token,
    k_reference_token_entries>
    g_reference_tokens{};
std::array<std::uint8_t,k_reference_token_sets>
    g_reference_token_victim{};
struct reference_token_guard_state {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

std::array<
    reference_token_guard_state,
    k_reference_token_sets>
    g_reference_token_locks{};
thread_local lightbank_reference_token
    g_draw_reference_token{};
// Diagnostic-only selector->draw distance. This does not authorize reuse:
// consume_draw_selection still clears the actual draw token and latest serial.
thread_local std::uint64_t
    g_last_selected_producer_serial = 0u;
thread_local std::uint32_t
    g_completed_draws_since_selector = 0xFFFFFFFFu;
// The producer hook and the ReShade draw callback are allowed to execute on a
// different thread from the exact selector hook. Keep the selector-authenticated
// token in a second process-wide bounded bank keyed by producer thread. The
// current state is the latest EXACT SELECTOR-AUTHENTICATED token for that
// producer thread, not a draw-side TLS copy of the latest producer serial.
// Runtime dd6bb17 showed selector-authenticated publications accumulating for
// the exact producer/consumer TID while draw consumption remained zero and the
// P_Metal source gate reported TOKEN_INVALID. producer_serial remains exact
// provenance; selector publication owns state lifetime. No process-global
// latest-token fallback.
std::array<
    lightbank_reference_token,
    k_reference_token_entries>
    g_selected_reference_tokens{};
std::array<std::uint8_t,k_reference_token_sets>
    g_selected_reference_token_victim{};
// Lock-free invalidation stamp for the synchronized selected-token bank.
// A selector publication bumps only its hashed producer-TID set. Draw-side
// consumers can therefore reuse an exact TLS copy while the selector-owned
// state is unchanged, and take the set lock only after a selector transition
// (or a harmless hash collision from another producer TID in the same set).
std::array<
    std::atomic<std::uint64_t>,
    k_reference_token_sets>
    g_selected_reference_generation{};
std::atomic<std::uint64_t>
    g_reference_publish_serial{0u};
thread_local std::uint64_t
    g_draw_thread_latest_publish_serial = 0u;
thread_local std::uint64_t
    g_draw_selected_reference_generation = 0u;
std::atomic<std::uint64_t>
    g_reference_cross_thread_selected_publish{0u};
std::atomic<std::uint64_t>
    g_reference_cross_thread_draw_consume{0u};
std::atomic<std::uint64_t>
    g_reference_cross_thread_serial_miss{0u};
std::atomic<std::uint32_t>
    g_reference_source_consumer_tid{0u};
std::atomic<std::uint64_t>
    g_reference_source_consumer_serial{0u};

class reference_token_set_guard {
public:
    explicit reference_token_set_guard(std::size_t set) noexcept
        : flag_(g_reference_token_locks[set].flag)
    {
        while (flag_.test_and_set(
            std::memory_order_acquire)) {
        }
    }

    ~reference_token_set_guard()
    {
        flag_.clear(std::memory_order_release);
    }

    reference_token_set_guard(
        const reference_token_set_guard &) = delete;
    reference_token_set_guard &operator=(
        const reference_token_set_guard &) = delete;

private:
    std::atomic_flag &flag_;
};

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
std::atomic<std::uint64_t> g_reference_publish_tuple_mismatch{0};
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

std::atomic<std::uint32_t> g_pmetal_diag_status_a{0u};
std::atomic<std::uint32_t> g_pmetal_diag_status_b{0u};
std::atomic<std::int32_t> g_pmetal_diag_selector_a{-1};
std::atomic<std::int32_t> g_pmetal_diag_selector_b{-1};
std::atomic<std::uint32_t> g_pmetal_diag_count_a{0u};
std::atomic<std::uint32_t> g_pmetal_diag_count_b{0u};
std::atomic<std::uint64_t> g_pmetal_diag_signature_a{0u};
std::atomic<std::uint64_t> g_pmetal_diag_signature_b{0u};
std::atomic<std::uint32_t> g_pmetal_diag_row_a{0u};
std::atomic<std::uint32_t> g_pmetal_diag_row_b{0u};
std::atomic<std::uint32_t> g_pmetal_diag_beta_bits{0u};
std::atomic_bool g_pmetal_diag_observed{false};

std::atomic<std::uint32_t> g_reference_publish_tid{0u};
std::atomic<std::uint32_t> g_reference_selector_tid{0u};
std::atomic_bool g_reference_publish_seen{false};
std::atomic_bool g_reference_selector_relevant_seen{false};
std::atomic_bool g_reference_selector_candidate_found{false};
std::atomic_bool g_reference_selector_tuple_read{false};
std::atomic_bool g_reference_selector_tuple_match{false};
std::atomic_bool g_reference_draw_token_selected{false};
std::atomic_bool g_reference_draw_token_source_ready{false};
std::atomic_bool g_reference_draw_token_vectors_ready{false};
std::atomic_bool g_reference_draw_token_upper_lower_ready{false};
std::atomic_bool g_direct_ul_producer_active{false};
std::atomic_bool g_steady_cache_builder_active{false};
std::atomic_bool g_reference_only_transport{false};
std::atomic_bool g_direct_ul_operator_changed{false};
std::atomic<std::uint64_t> g_direct_ul_steady_inject{0};
std::atomic<std::uint64_t> g_direct_ul_blend_inject{0};
std::atomic<std::uint64_t> g_direct_ul_inject_fail{0};
std::atomic<std::uint64_t> g_direct_ul_draw_ready{0};
std::atomic<std::uint64_t> g_direct_ul_draw_fallback{0};

// Owner pixel falsifier 2026-09-28: direct producer-level U/L cache
// mutation was live in the session that produced broad black world surfaces
// and a repeatable dark FaceEye wedge. Keep the reference/token transport
// active, but preserve stock DSR LightBank cache/output by default. This
// mutation can be re-enabled only for an explicit focused diagnostic.
bool direct_ul_mutation_enabled() noexcept
{
    static const bool enabled =
        telemetry::environment_flag(
            "DSRRL_EXPERIMENTAL_DIRECT_UPPER_LOWER");
    return enabled;
}


void latch_thread_id_once(
    std::atomic<std::uint32_t> &slot) noexcept
{
    if (!telemetry::effect_enabled())
        return;

    std::uint32_t expected = 0u;
    (void)slot.compare_exchange_strong(
        expected,
        static_cast<std::uint32_t>(
            GetCurrentThreadId()),
        std::memory_order_relaxed,
        std::memory_order_relaxed);
}

void latch_bool_once(
    std::atomic_bool &slot) noexcept
{
    if (!telemetry::effect_enabled() ||
        slot.load(std::memory_order_relaxed))
        return;

    slot.store(
        true,
        std::memory_order_relaxed);
}

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

pmetal_bank_cache_entry &pmetal_bank_cache_for(
    const std::uint8_t *base_ptr) noexcept
{
    const auto set =
        d123_cache_set(base_ptr);
    const auto base =
        set * k_steady_cache_ways;

    for (std::size_t way = 0u;
         way < k_steady_cache_ways;
         ++way) {
        auto &entry =
            g_pmetal_bank_cache[base + way];
        if (entry.valid &&
            entry.base == base_ptr)
            return entry;
    }

    for (std::size_t way = 0u;
         way < k_steady_cache_ways;
         ++way) {
        auto &entry =
            g_pmetal_bank_cache[base + way];
        if (!entry.valid) {
            entry = {};
            return entry;
        }
    }

    const auto victim =
        static_cast<std::size_t>(
            g_pmetal_bank_victim[set]++ &
            static_cast<std::uint8_t>(
                k_steady_cache_ways - 1u));
    auto &entry =
        g_pmetal_bank_cache[base + victim];
    entry = {};
    return entry;
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

    // The fingerprint and the source payload must describe the same producer
    // event. The assignment tuple is what the selector later authenticates;
    // never attach packer source pointers/selectors from a different tuple to
    // that fingerprint. A mismatch is fail-open, not a best-effort remap.
    std::uint32_t producer_beta_bits = 0u;
    static_assert(
        sizeof(producer_beta_bits) ==
        sizeof(producer.source_beta));
    std::memcpy(
        &producer_beta_bits,
        &producer.source_beta,
        sizeof(producer_beta_bits));

    if (producer.selector_a !=
            static_cast<std::int32_t>(selector_a) ||
        producer.selector_b !=
            static_cast<std::int32_t>(selector_b) ||
        producer_beta_bits != beta_bits) {
        telemetry::hot_count(
            g_reference_publish_tuple_mismatch);
        return;
    }

    latch_thread_id_once(
        g_reference_publish_tid);
    latch_bool_once(
        g_reference_publish_seen);

    token.producer_tid =
        static_cast<std::uint32_t>(
            GetCurrentThreadId());
    token.producer_serial =
        g_reference_publish_serial.fetch_add(
            1u,
            std::memory_order_relaxed) + 1u;
    token.source_a = producer.source_a;
    token.source_b = producer.source_b;
    token.selector_a = producer.selector_a;
    token.selector_b = producer.selector_b;
    token.beta = assignment_beta;
    token.source_ready = true;
    token.pmetal_env_ready =
        producer.have_pmetal_env;
    if (token.pmetal_env_ready) {
        token.pmetal_env.a = {
            producer.pmetal_env_a.x,
            producer.pmetal_env_a.y,
            producer.pmetal_env_a.z
        };
        token.pmetal_env.b = {
            producer.pmetal_env_b.x,
            producer.pmetal_env_b.y,
            producer.pmetal_env_b.z
        };
        token.pmetal_env.beta =
            std::clamp(
                producer.pmetal_env_beta,
                0.0f,
                1.0f);
        token.pmetal_env.bank_signature_a =
            producer.pmetal_bank_a;
        token.pmetal_env.bank_signature_b =
            producer.pmetal_bank_b;
        token.pmetal_env.row_id_a =
            producer.pmetal_row_a;
        token.pmetal_env.row_id_b =
            producer.pmetal_row_b;
    }
    token.vectors_ready =
        producer.evaluated_vectors_ready;
    token.upper_lower_ready =
        producer.have_upper &&
        producer.have_lower;
    token.direct_ul_applied =
        producer.direct_ul_applied;
    if (token.vectors_ready)
        token.directions =
            producer.evaluated_directions;
    if (token.upper_lower_ready) {
        token.upper = producer.upper;
        token.lower = producer.lower;
    }
    token.valid = true;
    token.available = true;

    // Draw-side consumption may happen on this producer thread after an exact
    // selector event on another engine thread. Retain only the serial identity
    // of this thread's most recent exact publication; the payload itself stays
    // in the bounded synchronized registries.
    g_draw_thread_latest_publish_serial =
        token.producer_serial;

    const auto set =
        reference_token_set(
            producer.owner);
    reference_token_set_guard guard(set);
    const auto base =
        set * k_reference_token_ways;

    for (std::size_t way = 0u;
         way < k_reference_token_ways;
         ++way) {
        auto &entry =
            g_reference_tokens[base + way];
        if (entry.valid &&
            operators::lightbank::
                lightbank_snapshot_matches_draw(
                    entry.fingerprint,
                    token.fingerprint)) {
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

void invalidate_selected_reference_token_for_producer(
    std::uint32_t producer_tid) noexcept
{
    if (producer_tid == 0u)
        return;

    // This helper is called only after exact P_Metal material identity has
    // been proven in the second phase of the retail selector callback. Keep
    // invalidation O(1): the selected-token bank is keyed by producer TID, so
    // scanning every 128x4 set here is unnecessary hot-path work.
    const auto set =
        reference_token_set(
            static_cast<std::uintptr_t>(
                producer_tid));
    reference_token_set_guard guard(set);
    const auto base =
        set * k_reference_token_ways;

    for (std::size_t way = 0u;
         way < k_reference_token_ways;
         ++way) {
        auto &entry =
            g_selected_reference_tokens[base + way];
        if (entry.valid &&
            entry.producer_tid ==
                producer_tid)
            entry = {};
    }

    // Always invalidate the draw-side TLS generation for this producer set.
    // A stale token may already have been evicted from the bounded bank while
    // a consumer thread still holds a cached copy.
    g_selected_reference_generation[set].
        fetch_add(
            1u,
            std::memory_order_release);
}

bool exact_pmetal_material_selection(
    const operators::material_response::material_identity &material) noexcept
{
    namespace mr =
        operators::material_response;
    namespace hashing =
        operators::legacy_plan::hashing;

    return
        material.valid &&
        material.owner_tuple_exact &&
        material.material_slot_valid &&
        material.route_index ==
            k_pmetal_material_route &&
        material.semantic_name_hash ==
            mr::mtd_semantic_hash(
                k_pmetal_material_name) &&
        hashing::matches_hex(
            material.raw_mtd_sha256,
            k_pmetal_material_sha256);
}

void publish_selected_reference_token(
    const lightbank_reference_token &token) noexcept
{
    if (!token.valid ||
        token.producer_tid == 0u ||
        token.producer_serial == 0u)
        return;

    const auto set =
        reference_token_set(
            static_cast<std::uintptr_t>(
                token.producer_tid));
    reference_token_set_guard guard(set);
    const auto base =
        set * k_reference_token_ways;

    for (std::size_t way = 0u;
         way < k_reference_token_ways;
         ++way) {
        auto &entry =
            g_selected_reference_tokens[base + way];
        if (entry.valid &&
            entry.producer_tid ==
                token.producer_tid) {
            // One current exact selector-owned state per producer thread.
            // A later exact selector replaces it even when its producer serial
            // is not the thread's numerically latest publication.
            entry = token;
            entry.available = true;
            g_selected_reference_generation[set].
                fetch_add(
                    1u,
                    std::memory_order_release);
            telemetry::hot_count(
                g_reference_cross_thread_selected_publish);
            return;
        }
    }

    for (std::size_t way = 0u;
         way < k_reference_token_ways;
         ++way) {
        auto &entry =
            g_selected_reference_tokens[base + way];
        if (!entry.valid ||
            !entry.available) {
            entry = token;
            entry.available = true;
            g_selected_reference_generation[set].
                fetch_add(
                    1u,
                    std::memory_order_release);
            telemetry::hot_count(
                g_reference_cross_thread_selected_publish);
            return;
        }
    }

    const auto victim =
        static_cast<std::size_t>(
            g_selected_reference_token_victim[set]++ &
            static_cast<std::uint8_t>(
                k_reference_token_ways - 1u));
    g_selected_reference_tokens[base + victim] =
        token;
    g_selected_reference_tokens[base + victim].
        available = true;
    g_selected_reference_generation[set].
        fetch_add(
            1u,
            std::memory_order_release);
    telemetry::hot_count(
        g_reference_cross_thread_selected_publish);
}

bool consume_selected_reference_token_for_current_thread(
    lightbank_reference_token &out) noexcept
{
    out = {};

    const auto tid =
        static_cast<std::uint32_t>(
            GetCurrentThreadId());

    if (tid == 0u)
        return false;

    const auto set =
        reference_token_set(
            static_cast<std::uintptr_t>(tid));
    reference_token_set_guard guard(set);
    const auto base =
        set * k_reference_token_ways;

    for (std::size_t way = 0u;
         way < k_reference_token_ways;
         ++way) {
        const auto &entry =
            g_selected_reference_tokens[base + way];
        if (!entry.valid ||
            !entry.available ||
            entry.producer_tid != tid ||
            entry.producer_serial == 0u)
            continue;

        // Selector-owned state is persistent across ReShade draw callbacks.
        // Do not consume it one-shot and do not require equality with the
        // separate draw-side latest-publication TLS.
        out = entry;
        out.available = false;
        telemetry::hot_count(
            g_reference_cross_thread_draw_consume);
        g_reference_source_consumer_serial.store(
            entry.producer_serial,
            std::memory_order_relaxed);
        return true;
    }

    return false;
}

bool consume_reference_token(
    std::uintptr_t owner,
    const operators::lightbank::
        lightbank_snapshot_fingerprint &draw,
    lightbank_reference_token &out,
    bool &tuple_mismatch) noexcept
{
    out = {};
    tuple_mismatch = false;

    if (owner == 0u)
        return false;

    const auto set =
        reference_token_set(owner);
    reference_token_set_guard guard(set);
    const auto base =
        set * k_reference_token_ways;

    bool owner_candidate = false;

    for (std::size_t way = 0u;
         way < k_reference_token_ways;
         ++way) {
        auto &entry =
            g_reference_tokens[base + way];
        if (!entry.valid ||
            !entry.available ||
            entry.fingerprint.owner != owner)
            continue;

        owner_candidate = true;

        if (!operators::lightbank::
                lightbank_snapshot_matches_draw(
                    entry.fingerprint,
                    draw))
            continue;

        entry.available = false;
        out = entry;
        out.available = false;
        return true;
    }

    // A same-owner token for another selector/beta tuple is not consumed.
    // The second way can therefore carry a parallel draw for the same bank.
    // The current draw still fails open if its exact tuple is absent.
    tuple_mismatch = owner_candidate;
    return false;
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
        !token.source_ready ||
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
    if (!token.valid ||
        !token.source_ready ||
        !token.vectors_ready ||
        !token.upper_lower_ready)
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

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto *entry =
            base + 0x30u +
            static_cast<std::size_t>(i) * 12u;

        std::uint32_t row_id = 0u;
        std::uint32_t name_offset = 0u;
        if (!safe_read(entry, row_id) ||
            !safe_read(entry + 8u, name_offset))
            return false;

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

            std::uint8_t ch = 0u;
            if (!safe_read(name_byte, ch))
                return false;

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
        pmetal_bank_cache_for(base);

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

void publish_pmetal_source_probe(
    bool endpoint_b,
    const pmetal_source_probe &probe,
    float beta) noexcept
{
    if (!telemetry::effect_enabled())
        return;

    auto &status = endpoint_b
        ? g_pmetal_diag_status_b
        : g_pmetal_diag_status_a;
    auto &selector = endpoint_b
        ? g_pmetal_diag_selector_b
        : g_pmetal_diag_selector_a;
    auto &count = endpoint_b
        ? g_pmetal_diag_count_b
        : g_pmetal_diag_count_a;
    auto &signature = endpoint_b
        ? g_pmetal_diag_signature_b
        : g_pmetal_diag_signature_a;
    auto &row = endpoint_b
        ? g_pmetal_diag_row_b
        : g_pmetal_diag_row_a;

    selector.store(
        probe.selector,
        std::memory_order_relaxed);
    count.store(
        probe.bank_count,
        std::memory_order_relaxed);
    signature.store(
        probe.bank_signature,
        std::memory_order_relaxed);
    row.store(
        probe.row_id,
        std::memory_order_relaxed);

    std::uint32_t beta_bits = 0u;
    static_assert(sizeof(beta_bits) == sizeof(beta));
    std::memcpy(
        &beta_bits,
        &beta,
        sizeof(beta_bits));
    g_pmetal_diag_beta_bits.store(
        beta_bits,
        std::memory_order_relaxed);

    status.store(
        static_cast<std::uint32_t>(
            probe.status),
        std::memory_order_release);
    g_pmetal_diag_observed.store(
        true,
        std::memory_order_release);
}

bool read_exact_pmetal_env_source(
    void *source,
    std::int32_t selector,
    f4 &out,
    std::uint64_t &bank_signature,
    std::uint32_t &row_id,
    pmetal_source_probe *probe = nullptr) noexcept
{
    out = {};
    bank_signature = 0u;
    row_id = 0u;

    pmetal_source_probe local{};
    local.selector = selector;
    auto finish = [&](pmetal_env_source_diag_status status) noexcept {
        local.status = status;
        local.bank_signature = bank_signature;
        local.row_id = row_id;
        if (probe != nullptr)
            *probe = local;
    };

    if (source == nullptr ||
        selector < 0) {
        finish(
            pmetal_env_source_diag_status::
                token_invalid);
        return false;
    }

    // Producer-hook callers arrive with an engine-attested source. The
    // selector/material P_Metal path additionally revalidates the carried
    // source object before entering here. Once source+0x18 is authorized,
    // reuse the immutable bank verdict by exact base pointer.
    const std::uint8_t *base = nullptr;
    std::memcpy(
        &base,
        static_cast<const std::uint8_t *>(
            source) + 0x18u,
        sizeof(base));
    if (base == nullptr) {
        finish(
            pmetal_env_source_diag_status::
                base_null);
        return false;
    }

    std::uint32_t index = 0u;
    if (!retail_lightbank_record_index(
            selector,
            index)) {
        finish(
            pmetal_env_source_diag_status::
                token_invalid);
        return false;
    }

    auto &cached_bank =
        pmetal_bank_cache_for(base);

    const pmetal_env_source_authority::bank_donor *bank = nullptr;
    std::uint16_t count = 0u;

    if (cached_bank.valid &&
        cached_bank.base == base) {
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

        local.bank_count = live_count;

        if (live_version != 4u ||
            live_count != cached_bank.count ||
            live_count == 0u ||
            live_count > 256u) {
            cached_bank = {};
            finish(
                pmetal_env_source_diag_status::
                    header_invalid);
            return false;
        }

        count = live_count;
        bank_signature =
            cached_bank.signature;
        bank = cached_bank.bank;

        if (index >= count) {
            finish(
                pmetal_env_source_diag_status::
                    selector_oob);
            return false;
        }

        if (bank == nullptr) {
            finish(
                bank_signature == 0u
                    ? pmetal_env_source_diag_status::
                        signature_invalid
                    : pmetal_env_source_diag_status::
                        bank_unknown);
            return false;
        }
    } else {
        std::uint16_t version = 0u;
        if (!safe_read(
                base + 8u,
                version) ||
            !safe_read(
                base + 10u,
                count) ||
            version != 4u ||
            count == 0u ||
            count > 256u) {
            local.bank_count = count;
            finish(
                pmetal_env_source_diag_status::
                    header_invalid);
            return false;
        }

        local.bank_count = count;

        if (index >= count) {
            finish(
                pmetal_env_source_diag_status::
                    selector_oob);
            return false;
        }

        bank =
            resolve_pmetal_bank(
                base,
                count,
                bank_signature);
        if (bank == nullptr) {
            finish(
                bank_signature == 0u
                    ? pmetal_env_source_diag_status::
                        signature_invalid
                    : pmetal_env_source_diag_status::
                        bank_unknown);
            return false;
        }
    }

    local.bank_count = count;

    const auto *entry =
        base + 0x30u +
        static_cast<std::size_t>(index) * 12u;

    if (cached_bank.valid &&
        cached_bank.base == base) {
        std::memcpy(
            &row_id,
            entry,
            sizeof(row_id));
    } else if (!safe_read(
                   entry,
                   row_id)) {
        finish(
            pmetal_env_source_diag_status::
                row_read_failed);
        return false;
    }

    const auto *row =
        pmetal_env_source_authority::find_row(
            *bank,
            row_id);
    if (row == nullptr) {
        finish(
            pmetal_env_source_diag_status::
                row_unknown);
        return false;
    }

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

    if (!std::isfinite(out.x) ||
        !std::isfinite(out.y) ||
        !std::isfinite(out.z)) {
        finish(
            pmetal_env_source_diag_status::
                nonfinite);
        return false;
    }

    finish(
        pmetal_env_source_diag_status::
            success);
    return true;
}

bool materialize_selected_pmetal_env_source(
    lightbank_reference_token &token) noexcept
{
    // Source A/B ownership ends at the retail LightBank producer. The token
    // must already contain immutable V13 donor state by the time the exact
    // P_Metal material gate runs.
    if (!token.valid ||
        !token.source_ready ||
        !token.pmetal_env_ready ||
        !std::isfinite(token.pmetal_env.a[0]) ||
        !std::isfinite(token.pmetal_env.a[1]) ||
        !std::isfinite(token.pmetal_env.a[2]) ||
        !std::isfinite(token.pmetal_env.b[0]) ||
        !std::isfinite(token.pmetal_env.b[1]) ||
        !std::isfinite(token.pmetal_env.b[2]) ||
        !std::isfinite(token.pmetal_env.beta))
        return false;

    token.pmetal_env.beta =
        std::clamp(
            token.pmetal_env.beta,
            0.0f,
            1.0f);
    return true;
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

    std::uint32_t retail_index = 0u;
    if (!retail_lightbank_record_index(
            selector,
            retail_index))
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
                retail_index < live_count) {
                const auto index =
                    static_cast<std::size_t>(
                        retail_index);
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
        retail_index >= count)
        return nullptr;

    const std::size_t index =
        static_cast<std::size_t>(
            retail_index);

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
        !direct_ul_mutation_enabled() ||
        dst == nullptr ||
        raw_row == nullptr)
        return;

    if (rewrite_steady_cache_ptde_ul(
            dst,
            raw_row)) {
        (void)mark_direct_ul_cache_record(
            dst);
        if (!g_direct_ul_operator_changed.load(
                std::memory_order_relaxed))
            g_direct_ul_operator_changed.store(
                true,
                std::memory_order_relaxed);
        telemetry::hot_count(
            g_direct_ul_steady_inject);
    } else
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

    std::uint32_t retail_index = 0u;
    if (!retail_lightbank_record_index(
            selector,
            retail_index))
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

            if (retail_index < live_count)
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
            retail_index >= count)
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
                retail_index) *
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

bool decode_reference_upper_lower(
    void *source_a,
    std::int32_t selector_a,
    void *source_b,
    std::int32_t selector_b,
    float beta,
    f4 &upper,
    f4 &lower) noexcept
{
    upper = {};
    lower = {};

    if (!std::isfinite(beta))
        return false;

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

    if (eval_a == nullptr ||
        eval_b == nullptr)
        return false;

    raw_rgbm upper_a{};
    raw_rgbm lower_a{};
    raw_rgbm upper_b{};
    raw_rgbm lower_b{};
    std::memcpy(
        &upper_a,
        eval_a + 0x24u,
        sizeof(upper_a));
    std::memcpy(
        &lower_a,
        eval_a + 0x2Cu,
        sizeof(lower_a));
    std::memcpy(
        &upper_b,
        eval_b + 0x24u,
        sizeof(upper_b));
    std::memcpy(
        &lower_b,
        eval_b + 0x2Cu,
        sizeof(lower_b));

    const auto ua = decode_rgbm(upper_a);
    const auto la = decode_rgbm(lower_a);
    const auto ub = decode_rgbm(upper_b);
    const auto lb = decode_rgbm(lower_b);

    upper =
        eval_beta == 0.0f
            ? ua
            : lerp4(
                ua,
                ub,
                eval_beta);
    lower =
        eval_beta == 0.0f
            ? la
            : lerp4(
                la,
                lb,
                eval_beta);

    return
        std::isfinite(upper.x) &&
        std::isfinite(upper.y) &&
        std::isfinite(upper.z) &&
        std::isfinite(lower.x) &&
        std::isfinite(lower.y) &&
        std::isfinite(lower.z);
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
                g_producer)) {
            if (g_reference_only_transport.load(
                    std::memory_order_acquire)) {
                // V13 source semantics are materialized at this retail
                // LightBank producer cut while the source is engine-live.
                // Keep visible U/L and D123 disabled; only the immutable
                // P_Metal EnvSpec donor is captured.
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
                    telemetry::hot_count(
                        g_pmetal_env_steady);
                } else {
                    g_producer.have_pmetal_env = false;
                    telemetry::hot_count(
                        g_pmetal_env_miss);
                }
                telemetry::hot_count(
                    g_steady_pass);
                return;
            }

            if (capture_evaluated_vectors(
                    dst,
                    g_producer)) {
                const auto *selected_record =
                    selected_cache_record_attested(
                        source,
                        selector);
                g_producer.direct_ul_applied =
                    selected_record != nullptr &&
                    direct_ul_cache_record_matches(
                        selected_record);

                if (!g_producer.direct_ul_applied) {
                    f4 exact_upper{};
                    f4 exact_lower{};
                    g_producer.have_upper = false;
                    g_producer.have_lower = false;
                    if (decode_reference_upper_lower(
                            source,
                            selector,
                            source,
                            selector,
                            0.0f,
                            exact_upper,
                            exact_lower)) {
                        g_producer.upper =
                            exact_upper;
                        g_producer.lower =
                            exact_lower;
                        g_producer.have_upper =
                            true;
                        g_producer.have_lower =
                            true;
                    }
                }

                telemetry::hot_count(
                    g_steady_pass);
            }
        }
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

    if (g_reference_only_transport.load(
            std::memory_order_acquire)) {
        return g_blend_orig != nullptr
            ? g_blend_orig(
                dst,
                a,
                b,
                beta)
            : nullptr;
    }

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
        g_producer.direct_ul_applied = true;
        if (!g_direct_ul_operator_changed.load(
                std::memory_order_relaxed))
            g_direct_ul_operator_changed.store(
                true,
                std::memory_order_relaxed);
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
                g_producer)) {
            if (g_reference_only_transport.load(
                    std::memory_order_acquire)) {
                f4 a{};
                f4 b{};
                std::uint64_t bank_a = 0u;
                std::uint64_t bank_b = 0u;
                std::uint32_t row_a = 0u;
                std::uint32_t row_b = 0u;

                const bool have_a =
                    read_exact_pmetal_env_source(
                        source_a,
                        selector_a,
                        a,
                        bank_a,
                        row_a);
                bool have_b = false;
                if (beta <= 0.0f ||
                    (source_a == source_b &&
                     selector_a == selector_b)) {
                    b = a;
                    bank_b = bank_a;
                    row_b = row_a;
                    have_b = have_a;
                } else {
                    have_b =
                        read_exact_pmetal_env_source(
                            source_b,
                            selector_b,
                            b,
                            bank_b,
                            row_b);
                }

                if (have_a && have_b &&
                    std::isfinite(beta)) {
                    if (beta >= 1.0f) {
                        a = b;
                        bank_a = bank_b;
                        row_a = row_b;
                    }
                    g_producer.have_pmetal_env = true;
                    g_producer.pmetal_env_a = a;
                    g_producer.pmetal_env_b = b;
                    g_producer.pmetal_env_beta =
                        std::clamp(beta, 0.0f, 1.0f);
                    g_producer.pmetal_bank_a = bank_a;
                    g_producer.pmetal_bank_b = bank_b;
                    g_producer.pmetal_row_a = row_a;
                    g_producer.pmetal_row_b = row_b;
                    telemetry::hot_count(
                        g_pmetal_env_blend);
                } else {
                    g_producer.have_pmetal_env = false;
                    telemetry::hot_count(
                        g_pmetal_env_miss);
                }
                return result;
            }

            if (capture_evaluated_vectors(
                    dst,
                    g_producer) &&
                !g_producer.direct_ul_applied) {
                f4 exact_upper{};
                f4 exact_lower{};
                g_producer.have_upper = false;
                g_producer.have_lower = false;
                if (decode_reference_upper_lower(
                        source_a,
                        selector_a,
                        source_b,
                        selector_b,
                        beta,
                        exact_upper,
                        exact_lower)) {
                    g_producer.upper =
                        exact_upper;
                    g_producer.lower =
                        exact_lower;
                    g_producer.have_upper =
                        true;
                    g_producer.have_lower =
                        true;
                }
            }
        }
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

    // Keep the narrow reference/token carrier live for exact downstream
    // consumers, but do not mutate the shared DSR LightBank producer unless
    // an explicit diagnostic opts in. This prevents unproven renderer-wide
    // propagation into unrelated receiver families.
    g_direct_ul_producer_active.store(
        direct_ul_mutation_enabled(),
        std::memory_order_release);

    // The old standalone P_Metal blend hook stays disabled. Exact V13 donor
    // capture now lives in the already-required steady/blend packer taps and
    // carries only immutable P_Metal source data; visible U/L and HemDir3
    // evaluation remain disabled in reference-only mode.
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
    g_draw_thread_latest_publish_serial = 0u;
    g_draw_selected_reference_generation = 0u;
    g_last_selected_producer_serial = 0u;
    g_completed_draws_since_selector = 0xFFFFFFFFu;
    for (std::size_t set = 0u;
         set < k_reference_token_sets;
         ++set) {
        reference_token_set_guard guard(set);
        const auto base =
            set * k_reference_token_ways;
        for (std::size_t way = 0u;
             way < k_reference_token_ways;
             ++way) {
            g_reference_tokens[base + way] = {};
            g_selected_reference_tokens[base + way] = {};
        }
        g_reference_token_victim[set] = 0u;
        g_selected_reference_token_victim[set] = 0u;
        g_selected_reference_generation[set].
            store(
                0u,
                std::memory_order_release);
    }
    clear_b13_upload_slots();
    clear_direct_ul_cache_stamps();
    // The bank verdict is keyed by an engine-owned base pointer. Drop the
    // current thread's verdicts at lifecycle boundaries so allocator address
    // reuse cannot resurrect a stale exact bank identity after teardown.
    g_pmetal_bank_cache = {};
    g_pmetal_bank_victim = {};
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

void upper_lower_pmetal_material_event_bridge(
    void *owner,
    const operators::material_response::material_identity &material) noexcept
{
    if (g_runtime != nullptr)
        g_runtime->pmetal_material_event(
            owner,
            material);
}

bool upper_lower_draw_runtime::install(
    bool reference_only) noexcept
{
    if (g_enabled.load())
        return
            g_runtime == this &&
            g_reference_only_transport.load(
                std::memory_order_acquire) ==
                reference_only;

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
    g_reference_only_transport.store(
        reference_only,
        std::memory_order_release);

    if (g_base == 0u ||
        !install_producer_hooks()) {
        (void)restore_producer_hooks();
        g_reference_only_transport.store(
            false,
            std::memory_order_release);
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
    g_reference_only_transport.store(
        false,
        std::memory_order_release);
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

    latch_thread_id_once(
        g_reference_selector_tid);
    latch_bool_once(
        g_reference_selector_relevant_seen);

    if (descriptor == nullptr) {
        telemetry::hot_count(g_selector_miss);
        return;
    }

    if (g_steady_cache_builder_active.load(
            std::memory_order_acquire)) {
        const auto owner_key =
            reinterpret_cast<std::uintptr_t>(
                owner);

        std::uint16_t selector_a = 0u;
        std::uint16_t selector_b = 0u;
        std::uint32_t beta_bits = 0u;
        if (!read_selector_tuple_attested(
                descriptor,
                selector_a,
                selector_b,
                beta_bits)) {
            telemetry::hot_count(
                g_selector_miss);
            return;
        }

        latch_bool_once(
            g_reference_selector_tuple_read);

        const operators::lightbank::
            lightbank_snapshot_fingerprint draw{
                owner_key,
                selector_a,
                selector_b,
                beta_bits
            };

        lightbank_reference_token selected{};
        bool tuple_mismatch = false;
        if (!consume_reference_token(
                owner_key,
                draw,
                selected,
                tuple_mismatch)) {
            if (tuple_mismatch)
                telemetry::hot_count(
                    g_tuple_mismatch);
            else
                telemetry::hot_count(
                    g_selector_miss);
            return;
        }

        latch_bool_once(
            g_reference_selector_candidate_found);
        latch_bool_once(
            g_reference_selector_tuple_match);

        // The exact selector tuple authenticates which LightBank
        // selection is current, but it does not identify the material that
        // will consume it. Stage the token only. Do not invalidate current
        // P_Metal state here: this generic selector path also runs for
        // non-P_Metal materials sharing the same producer thread. The second
        // phase of this exact callback owns P_Metal-specific invalidation.
        g_draw_reference_token =
            selected;
        g_last_selected_producer_serial =
            selected.producer_serial;
        g_completed_draws_since_selector = 0u;
        latch_bool_once(
            g_reference_draw_token_selected);
        if (selected.source_ready)
            latch_bool_once(
                g_reference_draw_token_source_ready);
        if (selected.vectors_ready)
            latch_bool_once(
                g_reference_draw_token_vectors_ready);
        if (selected.upper_lower_ready)
            latch_bool_once(
                g_reference_draw_token_upper_lower_ready);
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

void upper_lower_draw_runtime::pmetal_material_event(
    void *owner,
    const operators::material_response::material_identity &material) noexcept
{
    if (!g_enabled.load(
            std::memory_order_acquire) ||
        g_quarantined.load(
            std::memory_order_acquire) ||
        owner == nullptr ||
        !exact_pmetal_material_selection(
            material))
        return;

    // selector_event() and this call are two phases of the same exact retail
    // FLVER selector callback. Only this exact P_Metal material phase is
    // allowed to replace/invalidate persistent P_Metal source state. A generic
    // LightBank selector event for another material must leave it untouched.
    const auto owner_key =
        reinterpret_cast<std::uintptr_t>(
            owner);
    const auto producer_tid =
        static_cast<std::uint32_t>(
            GetCurrentThreadId());

    // selector_event() and pmetal_material_event() are phases of the same
    // retail callback, so producer TID is the exact selected-token bank key.
    // Invalidate only after the P_Metal material gate, but do it in O(1)
    // rather than scanning every selected-token set.
    invalidate_selected_reference_token_for_producer(
        producer_tid);

    // If this exact P_Metal selector has no matching staged source token, the
    // current producer-thread state has already been invalidated above and we
    // fail open instead of resurrecting a donor from a prior P_Metal draw.
    if (!g_draw_reference_token.valid ||
        !g_draw_reference_token.source_ready ||
        producer_tid == 0u ||
        g_draw_reference_token.producer_tid !=
            producer_tid ||
        g_draw_reference_token.producer_serial == 0u ||
        g_draw_reference_token.fingerprint.owner !=
            owner_key)
        return;

    auto selected_token =
        g_draw_reference_token;

    // V13 decoded the donor while the engine source tuple was live. The
    // reference token must already carry immutable A/B+beta+bank/row state.
    // This exact material gate authorizes publication but never re-dereferences
    // delayed engine source pointers.
    if (!materialize_selected_pmetal_env_source(
            selected_token))
        return;

    g_draw_reference_token =
        selected_token;
    publish_selected_reference_token(
        selected_token);
}

bool upper_lower_draw_runtime::direct_producer_active() const noexcept
{
    return
        g_enabled.load(std::memory_order_acquire) &&
        !g_quarantined.load(std::memory_order_acquire) &&
        g_direct_ul_producer_active.load(
            std::memory_order_acquire);
}

bool upper_lower_draw_runtime::
direct_producer_ready_for_draw() const noexcept
{
    if (!direct_producer_active())
        return false;

    const bool ready =
        g_draw_reference_token.valid &&
        g_draw_reference_token.upper_lower_ready &&
        g_draw_reference_token.direct_ul_applied;

    telemetry::hot_count(
        ready
            ? g_direct_ul_draw_ready
            : g_direct_ul_draw_fallback);
    return ready;
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
        if (!g_draw_reference_token.valid ||
            !g_draw_reference_token.upper_lower_ready)
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
        const auto source_consumer_tid =
            static_cast<std::uint32_t>(
                GetCurrentThreadId());
        std::uint32_t expected_zero = 0u;
        (void)g_reference_source_consumer_tid.
            compare_exchange_strong(
                expected_zero,
                source_consumer_tid,
                std::memory_order_relaxed,
                std::memory_order_relaxed);

        // Runtime dd6bb17 proved that exact selector-owned LightBank tokens
        // reach the synchronized selected bank for the producer/consumer TID
        // while the draw-side TLS latest-serial gate can still remain
        // unavailable and force TOKEN_INVALID. Therefore the exact
        // selector-authenticated per-producer state, not a separate TLS copy
        // of "latest producer serial", owns P_Metal source lifetime.
        lightbank_reference_token
            cross_thread_token{};
        const lightbank_reference_token *token_ptr =
            nullptr;

        const auto selected_set =
            reference_token_set(
                static_cast<std::uintptr_t>(
                    source_consumer_tid));
        const auto selected_generation =
            g_selected_reference_generation[
                selected_set].load(
                    std::memory_order_acquire);

        const bool cached_selected_current =
            g_draw_reference_token.valid &&
            g_draw_reference_token.producer_tid ==
                source_consumer_tid &&
            g_draw_reference_token.producer_serial != 0u &&
            g_draw_selected_reference_generation ==
                selected_generation;

        if (cached_selected_current) {
            token_ptr =
                &g_draw_reference_token;
        } else if (
            consume_selected_reference_token_for_current_thread(
                cross_thread_token)) {
            // 86f runtime proved this path can be extremely hot (~90k
            // synchronized consumes in one diagnostic session). Cache the
            // exact selector-owned token and invalidate it with the lock-free
            // per-set generation stamp instead of taking the spin lock on
            // every P_Metal source query.
            g_draw_reference_token =
                cross_thread_token;
            g_last_selected_producer_serial =
                cross_thread_token.producer_serial;
            g_draw_selected_reference_generation =
                selected_generation;
            token_ptr =
                &g_draw_reference_token;
        }

        const lightbank_reference_token
            invalid_token{};
        const auto &token =
            token_ptr != nullptr
                ? *token_ptr
                : invalid_token;

        if (!token.valid ||
            !token.pmetal_env_ready ||
            !std::isfinite(token.pmetal_env.beta)) {
            pmetal_source_probe invalid_a{};
            invalid_a.status =
                pmetal_env_source_diag_status::
                    token_invalid;
            invalid_a.selector =
                token.selector_a;
            pmetal_source_probe invalid_b =
                invalid_a;
            invalid_b.selector =
                token.selector_b;
            publish_pmetal_source_probe(
                false,
                invalid_a,
                token.beta);
            publish_pmetal_source_probe(
                true,
                invalid_b,
                token.beta);
            return false;
        }

        out = token.pmetal_env;
        return
            std::isfinite(out.a[0]) &&
            std::isfinite(out.a[1]) &&
            std::isfinite(out.a[2]) &&
            std::isfinite(out.b[0]) &&
            std::isfinite(out.b[1]) &&
            std::isfinite(out.b[2]) &&
            std::isfinite(out.beta);
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

pmetal_env_source_diagnostic
upper_lower_draw_runtime::pmetal_source_diagnostic() const noexcept
{
    pmetal_env_source_diagnostic out{};

    out.a.status =
        static_cast<pmetal_env_source_diag_status>(
            g_pmetal_diag_status_a.load(
                std::memory_order_acquire));
    out.a.selector =
        g_pmetal_diag_selector_a.load(
            std::memory_order_relaxed);
    out.a.bank_count =
        static_cast<std::uint16_t>(
            g_pmetal_diag_count_a.load(
                std::memory_order_relaxed));
    out.a.bank_signature =
        g_pmetal_diag_signature_a.load(
            std::memory_order_relaxed);
    out.a.row_id =
        g_pmetal_diag_row_a.load(
            std::memory_order_relaxed);

    out.b.status =
        static_cast<pmetal_env_source_diag_status>(
            g_pmetal_diag_status_b.load(
                std::memory_order_acquire));
    out.b.selector =
        g_pmetal_diag_selector_b.load(
            std::memory_order_relaxed);
    out.b.bank_count =
        static_cast<std::uint16_t>(
            g_pmetal_diag_count_b.load(
                std::memory_order_relaxed));
    out.b.bank_signature =
        g_pmetal_diag_signature_b.load(
            std::memory_order_relaxed);
    out.b.row_id =
        g_pmetal_diag_row_b.load(
            std::memory_order_relaxed);

    const auto beta_bits =
        g_pmetal_diag_beta_bits.load(
            std::memory_order_relaxed);
    static_assert(
        sizeof(beta_bits) ==
        sizeof(out.beta));
    std::memcpy(
        &out.beta,
        &beta_bits,
        sizeof(out.beta));

    out.observed =
        g_pmetal_diag_observed.load(
            std::memory_order_acquire);

    out.producer_publish_tid =
        g_reference_publish_tid.load(
            std::memory_order_relaxed);
    out.selector_tid =
        g_reference_selector_tid.load(
            std::memory_order_relaxed);
    out.producer_publish_seen =
        g_reference_publish_seen.load(
            std::memory_order_relaxed);
    out.selector_relevant_seen =
        g_reference_selector_relevant_seen.load(
            std::memory_order_relaxed);
    out.selector_candidate_found =
        g_reference_selector_candidate_found.load(
            std::memory_order_relaxed);
    out.selector_tuple_read =
        g_reference_selector_tuple_read.load(
            std::memory_order_relaxed);
    out.selector_tuple_match =
        g_reference_selector_tuple_match.load(
            std::memory_order_relaxed);
    out.draw_token_selected =
        g_reference_draw_token_selected.load(
            std::memory_order_relaxed);
    out.draw_token_source_ready =
        g_reference_draw_token_source_ready.load(
            std::memory_order_relaxed);
    out.draw_token_vectors_ready =
        g_reference_draw_token_vectors_ready.load(
            std::memory_order_relaxed);
    out.draw_token_upper_lower_ready =
        g_reference_draw_token_upper_lower_ready.load(
            std::memory_order_relaxed);
    out.source_consumer_tid =
        g_reference_source_consumer_tid.load(
            std::memory_order_relaxed);
    out.source_consumer_serial =
        g_reference_source_consumer_serial.load(
            std::memory_order_relaxed);
    out.cross_thread_selected_publish =
        g_reference_cross_thread_selected_publish.load(
            std::memory_order_relaxed);
    out.cross_thread_draw_consume =
        g_reference_cross_thread_draw_consume.load(
            std::memory_order_relaxed);
    out.cross_thread_serial_miss =
        g_reference_cross_thread_serial_miss.load(
            std::memory_order_relaxed);
    return out;
}

pmetal_draw_token_frontier
upper_lower_draw_runtime::pmetal_draw_token_state() const noexcept
{
    pmetal_draw_token_frontier out{};
    out.current_tid =
        static_cast<std::uint32_t>(
            GetCurrentThreadId());
    out.latest_publish_serial =
        g_draw_thread_latest_publish_serial;

    // In reference-only P_Metal transport, the draw-side latest-publication
    // TLS is not authoritative for selector-owned state. Freshness here is
    // exact producer-thread ownership plus a nonzero selector-proven producer
    // serial; latest_publish_serial remains diagnostic telemetry only.
    const bool fresh =
        g_draw_reference_token.valid &&
        g_draw_reference_token.producer_tid ==
            out.current_tid &&
        g_draw_reference_token.producer_serial != 0u;

    out.local_token_valid = fresh;
    out.local_source_ready =
        fresh &&
        g_draw_reference_token.source_ready;
    if (fresh) {
        out.token_producer_tid =
            g_draw_reference_token.producer_tid;
        out.token_producer_serial =
            g_draw_reference_token.producer_serial;
        out.selector_a =
            g_draw_reference_token.selector_a;
        out.selector_b =
            g_draw_reference_token.selector_b;
        out.beta =
            g_draw_reference_token.beta;
    }
    out.last_selected_producer_serial =
        g_last_selected_producer_serial;
    out.completed_draws_since_selector =
        g_completed_draws_since_selector;
    return out;
}

void upper_lower_draw_runtime::release_prepared_draw(
    prepared_upper_lower_draw &prepared) noexcept
{
    prepared = {};
}

void upper_lower_draw_runtime::consume_draw_selection() noexcept
{
    g_draw_snapshot.reset();

    if (g_reference_only_transport.load(
            std::memory_order_acquire)) {
        // Reference-only mode carries persistent engine LightBank selection
        // state. ReShade draw completion must not invalidate it solely because
        // the draw-side latest-publication TLS does not mirror the selector
        // handoff. The synchronized selected-token bank is replaced only by a
        // later exact selector event for the same producer thread; source
        // queries refresh this TLS cache from that bank.
        const auto tid =
            static_cast<std::uint32_t>(
                GetCurrentThreadId());
        if (g_draw_reference_token.valid &&
            (g_draw_reference_token.producer_tid != tid ||
             g_draw_reference_token.producer_serial == 0u))
            g_draw_reference_token = {};
    } else {
        // Non-reference U/L/HemDir3 transport retains the historical
        // draw-scoped lifetime.
        g_draw_reference_token = {};
        g_draw_thread_latest_publish_serial = 0u;
    }

    if (g_completed_draws_since_selector != 0xFFFFFFFFu &&
        g_completed_draws_since_selector != 0xFFFFFFFEu)
        ++g_completed_draws_since_selector;
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
        g_reference_publish_tuple_mismatch.load(),
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
        g_direct_ul_draw_ready.load(),
        g_direct_ul_draw_fallback.load(),
        g_enabled.load(),
        g_pmetal_env_hook_armed.load(),
        g_direct_ul_producer_active.load(),
        g_direct_ul_operator_changed.load(),
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
    g_reference_publish_tuple_mismatch.store(0);
    g_b13_create.store(0);
    g_b13_hit.store(0);
    g_hemdir3_b13_create.store(0);
    g_hemdir3_b13_hit.store(0);
    g_requests.store(0);
    g_hemdir3_carrier_requests.store(0);
    g_pmetal_env_steady.store(0);
    g_pmetal_env_blend.store(0);
    g_pmetal_env_miss.store(0);
    g_pmetal_diag_status_a.store(0u);
    g_pmetal_diag_status_b.store(0u);
    g_pmetal_diag_selector_a.store(-1);
    g_pmetal_diag_selector_b.store(-1);
    g_pmetal_diag_count_a.store(0u);
    g_pmetal_diag_count_b.store(0u);
    g_pmetal_diag_signature_a.store(0u);
    g_pmetal_diag_signature_b.store(0u);
    g_pmetal_diag_row_a.store(0u);
    g_pmetal_diag_row_b.store(0u);
    g_pmetal_diag_beta_bits.store(0u);
    g_pmetal_diag_observed.store(false);
    g_reference_publish_tid.store(0u);
    g_reference_selector_tid.store(0u);
    g_reference_publish_seen.store(false);
    g_reference_selector_relevant_seen.store(false);
    g_reference_selector_candidate_found.store(false);
    g_reference_selector_tuple_read.store(false);
    g_reference_selector_tuple_match.store(false);
    g_reference_draw_token_selected.store(false);
    g_reference_draw_token_source_ready.store(false);
    g_reference_draw_token_vectors_ready.store(false);
    g_reference_draw_token_upper_lower_ready.store(false);
    g_reference_publish_serial.store(0u);
    g_reference_cross_thread_selected_publish.store(0u);
    g_reference_cross_thread_draw_consume.store(0u);
    g_reference_cross_thread_serial_miss.store(0u);
    g_reference_source_consumer_tid.store(0u);
    g_reference_source_consumer_serial.store(0u);
    g_direct_ul_steady_inject.store(0);
    g_direct_ul_blend_inject.store(0);
    g_direct_ul_inject_fail.store(0);
    g_direct_ul_operator_changed.store(false);
}

} // namespace dsrrl::runtime
