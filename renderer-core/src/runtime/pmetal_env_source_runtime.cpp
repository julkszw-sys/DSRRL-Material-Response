#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"
#include "dsrrl/runtime/pmetal_producer_state.hpp"
#include "dsrrl/runtime/pmetal_selector_policy.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/runtime/generated_pmetal_env_source_authority.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace dsrrl::runtime {
namespace {

constexpr std::uint32_t k_pmetal_material_route = 345u;
constexpr const char *k_pmetal_material_name =
    "P_Metal[DSB].mtd";
constexpr const char *k_pmetal_material_sha256 =
    "ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b";

constexpr std::uintptr_t k_ret_sel_1 = 0x20E019u;
constexpr std::uintptr_t k_ret_sel_2 = 0x20EB7Fu;
constexpr std::uintptr_t k_ret_sel_3 = 0x20FB9Eu;
constexpr std::uintptr_t k_parent_return = 0x220CF0u;

struct f4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

constexpr std::uintptr_t k_envspec_single_rva = 0x563B80u;
constexpr std::uintptr_t k_envspec_blend_rva = 0x563C30u;
constexpr std::array<std::uint8_t,14> k_envspec_single_preimage{{
    0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x40,0x48,0x8b,0x41,0x18
}};
constexpr std::array<std::uint8_t,14> k_envspec_blend_preimage{{
    0x40,0x53,0x48,0x83,0xec,0x50,0x44,0x8b,0x94,0x24,0x80,0x00,0x00,0x00
}};

struct source_hook {
    void *target = nullptr;
    void *trampoline = nullptr;
    void *detour = nullptr;
    std::size_t stolen = 0u;
    std::array<std::uint8_t,14> original{};
    bool patched = false;
};

using envspec_single_fn =
    void (__fastcall *)(void *, float *, int);
using envspec_blend_fn =
    void (__fastcall *)(float *, void *, int, void *, int, float);

source_hook g_envspec_single_hook{};
source_hook g_envspec_blend_hook{};
envspec_single_fn g_envspec_single_original = nullptr;
envspec_blend_fn g_envspec_blend_original = nullptr;

struct hook_source_record {
    pmetal_envspec_source source{};
    std::uint64_t serial = 0u;
    bool valid = false;
};

thread_local hook_source_record g_hook_source_tls{};
std::mutex g_hook_source_mutex;
hook_source_record g_hook_source_global{};
std::uint64_t g_hook_source_consumed_serial = 0u;
std::atomic<std::uint64_t> g_hook_source_serial{0u};
std::atomic<std::uint64_t> g_hook_single_seen{0u};
std::atomic<std::uint64_t> g_hook_blend_seen{0u};
std::atomic<std::uint64_t> g_hook_publish{0u};
std::atomic<std::uint64_t> g_hook_consume{0u};
std::atomic_bool g_hook_restore_failed{false};

struct readable_window {
    std::uintptr_t begin = 0u;
    std::uintptr_t end = 0u;
};

std::atomic<std::uint64_t> g_source_cache_generation{1u};
std::atomic<std::uint64_t> g_endpoint_cache_hit{0u};
std::atomic<std::uint64_t> g_endpoint_cache_miss{0u};
std::atomic<std::uint64_t> g_endpoint_cache_fill{0u};
std::atomic<std::uint64_t> g_region_cache_hit{0u};
std::atomic<std::uint64_t> g_region_cache_miss{0u};

struct readable_region_cache_entry {
    std::uintptr_t begin = 0u;
    std::uintptr_t end = 0u;
    std::uint64_t generation = 0u;
    bool readable = false;
    bool valid = false;
};

constexpr std::size_t k_region_cache_sets = 64u;
constexpr std::size_t k_region_cache_ways = 2u;
constexpr std::size_t k_region_cache_entries =
    k_region_cache_sets * k_region_cache_ways;

thread_local std::array<
    readable_region_cache_entry,
    k_region_cache_entries>
    g_region_cache{};
thread_local std::array<
    std::uint8_t,
    k_region_cache_sets>
    g_region_cache_victim{};

std::size_t region_cache_set(
    const void *ptr) noexcept
{
    const auto value =
        reinterpret_cast<std::uintptr_t>(ptr);
    const auto mixed =
        (value >> 12u) ^
        (value >> 21u) ^
        (value >> 31u);

    return static_cast<std::size_t>(
        mixed & (k_region_cache_sets - 1u));
}

bool query_region(
    const void *ptr,
    readable_window &window,
    bool &is_readable) noexcept
{
    window = {};
    is_readable = false;
    if (ptr == nullptr)
        return false;

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(
            ptr,
            &mbi,
            sizeof(mbi)) != sizeof(mbi))
        return false;

    const auto begin =
        reinterpret_cast<std::uintptr_t>(
            mbi.BaseAddress);
    const auto end =
        begin + mbi.RegionSize;
    const auto address =
        reinterpret_cast<std::uintptr_t>(ptr);

    if (end <= address ||
        end < begin)
        return false;

    const DWORD access =
        mbi.Protect & 0xffu;
    is_readable =
        mbi.State == MEM_COMMIT &&
        (mbi.Protect & PAGE_GUARD) == 0u &&
        (access == PAGE_READONLY ||
         access == PAGE_READWRITE ||
         access == PAGE_WRITECOPY ||
         access == PAGE_EXECUTE_READ ||
         access == PAGE_EXECUTE_READWRITE ||
         access == PAGE_EXECUTE_WRITECOPY);

    window.begin = begin;
    window.end = end;
    return true;
}

bool cached_readable_window(
    const void *ptr,
    readable_window &window) noexcept
{
    window = {};
    if (ptr == nullptr)
        return false;

    const auto address =
        reinterpret_cast<std::uintptr_t>(ptr);
    const auto generation =
        g_source_cache_generation.load(
            std::memory_order_relaxed);
    const auto set =
        region_cache_set(ptr);
    const auto base =
        set * k_region_cache_ways;

    for (std::size_t way = 0u;
         way < k_region_cache_ways;
         ++way) {
        const auto &entry =
            g_region_cache[base + way];
        if (entry.valid &&
            entry.generation == generation &&
            entry.begin <= address &&
            address < entry.end) {
            g_region_cache_hit.fetch_add(
                1u,
                std::memory_order_relaxed);
            if (!entry.readable)
                return false;
            window.begin = entry.begin;
            window.end = entry.end;
            return true;
        }
    }

    g_region_cache_miss.fetch_add(
        1u,
        std::memory_order_relaxed);

    readable_window queried{};
    bool is_readable = false;
    if (!query_region(
            ptr,
            queried,
            is_readable))
        return false;

    std::size_t target = k_region_cache_ways;
    for (std::size_t way = 0u;
         way < k_region_cache_ways;
         ++way) {
        const auto &entry =
            g_region_cache[base + way];
        if (!entry.valid ||
            entry.generation != generation) {
            target = way;
            break;
        }
    }

    if (target == k_region_cache_ways) {
        target =
            static_cast<std::size_t>(
                g_region_cache_victim[set]++ &
                static_cast<std::uint8_t>(
                    k_region_cache_ways - 1u));
    }

    auto &entry =
        g_region_cache[base + target];
    entry.begin = queried.begin;
    entry.end = queried.end;
    entry.generation = generation;
    entry.readable = is_readable;
    entry.valid = true;

    if (!is_readable)
        return false;

    window = queried;
    return true;
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
        readable_window window{};
        if (!cached_readable_window(
                reinterpret_cast<const void *>(cursor),
                window))
            return false;

        if (window.end <= cursor)
            return false;

        cursor =
            std::min(
                window.end,
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

bool ensure_readable_window(
    const void *ptr,
    readable_window &window) noexcept
{
    const auto address =
        reinterpret_cast<std::uintptr_t>(ptr);

    if (ptr != nullptr &&
        window.begin <= address &&
        address < window.end)
        return true;

    return cached_readable_window(
        ptr,
        window);
}

bool exact_pmetal_material_selection(
    const operators::material_response::
        material_identity &material) noexcept
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

bool retail_lightbank_record_index(
    std::int32_t selector,
    std::uint32_t &index) noexcept
{
    index = 0u;
    if (selector < 0)
        return false;

    index =
        static_cast<std::uint32_t>(
            static_cast<std::uint8_t>(
                selector));
    return true;
}

struct bank_cache_entry {
    const std::uint8_t *base = nullptr;
    std::uint16_t count = 0u;
    std::uint64_t generation = 0u;
    std::uint64_t signature = 0u;
    const pmetal_env_source_authority::
        bank_donor *bank = nullptr;
    bool valid = false;
};

constexpr std::size_t k_bank_cache_sets = 128u;
constexpr std::size_t k_bank_cache_ways = 4u;
constexpr std::size_t k_bank_cache_entries =
    k_bank_cache_sets *
    k_bank_cache_ways;

thread_local std::array<
    bank_cache_entry,
    k_bank_cache_entries>
    g_bank_cache{};
thread_local std::array<
    std::uint8_t,
    k_bank_cache_sets>
    g_bank_victim{};

std::size_t bank_cache_set(
    const void *ptr) noexcept
{
    const auto value =
        reinterpret_cast<std::uintptr_t>(ptr);
    const auto mixed =
        (value >> 4u) ^
        (value >> 13u) ^
        (value >> 23u) ^
        (value >> 33u);

    return static_cast<std::size_t>(
        mixed &
        (k_bank_cache_sets - 1u));
}

bank_cache_entry &bank_cache_for(
    const std::uint8_t *base_ptr) noexcept
{
    const auto generation =
        g_source_cache_generation.load(
            std::memory_order_relaxed);
    const auto set =
        bank_cache_set(base_ptr);
    const auto base =
        set * k_bank_cache_ways;

    for (std::size_t way = 0u;
         way < k_bank_cache_ways;
         ++way) {
        auto &entry =
            g_bank_cache[base + way];
        if (entry.valid &&
            entry.generation == generation &&
            entry.base == base_ptr)
            return entry;
    }

    for (std::size_t way = 0u;
         way < k_bank_cache_ways;
         ++way) {
        auto &entry =
            g_bank_cache[base + way];
        if (!entry.valid ||
            entry.generation != generation) {
            entry = {};
            entry.generation = generation;
            return entry;
        }
    }

    const auto victim =
        static_cast<std::size_t>(
            g_bank_victim[set]++ &
            static_cast<std::uint8_t>(
                k_bank_cache_ways - 1u));
    auto &entry =
        g_bank_cache[base + victim];
    entry = {};
    entry.generation = generation;
    return entry;
}

std::uint64_t fnv_byte(
    std::uint64_t hash,
    std::uint8_t value) noexcept
{
    hash ^= value;
    return
        hash *
        0x100000001b3ULL;
}

bool bank_signature(
    const std::uint8_t *base,
    std::uint64_t &signature) noexcept
{
    signature = 0u;
    if (base == nullptr ||
        !readable_range(
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
        static_cast<std::size_t>(
            count) * 12u;

    if (table_bytes < 8u ||
        !readable_range(
            base + 8u,
            table_bytes - 8u))
        return false;

    std::uint64_t hash =
        0xcbf29ce484222325ULL;
    hash =
        fnv_byte(
            hash,
            static_cast<std::uint8_t>(
                count));
    hash =
        fnv_byte(
            hash,
            static_cast<std::uint8_t>(
                count >> 8u));

    const std::uint32_t minimum_name =
        0x30u +
        static_cast<std::uint32_t>(
            count) * 12u;

    readable_window window{};

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto *entry =
            base +
            0x30u +
            static_cast<std::size_t>(i) *
                12u;

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
            hash =
                fnv_byte(
                    hash,
                    static_cast<std::uint8_t>(
                        row_id >> shift));

        const auto *name =
            base +
            static_cast<std::size_t>(
                name_offset);

        std::uint32_t consumed = 0u;
        bool terminated = false;

        while (consumed < 256u) {
            const auto *cursor =
                name + consumed;
            if (!ensure_readable_window(
                    cursor,
                    window))
                return false;

            const auto address =
                reinterpret_cast<std::uintptr_t>(
                    cursor);
            if (window.end <= address)
                return false;

            const auto room =
                static_cast<std::size_t>(
                    window.end - address);
            const auto chunk =
                std::min<std::size_t>(
                    256u - consumed,
                    room);
            if (chunk == 0u)
                return false;

            for (std::size_t j = 0u;
                 j < chunk;
                 ++j) {
                const auto ch =
                    cursor[j];
                hash =
                    fnv_byte(
                        hash,
                        ch);
                ++consumed;

                if (ch == 0u) {
                    terminated = true;
                    break;
                }
            }

            if (terminated)
                break;
        }

        if (!terminated)
            return false;
    }

    signature = hash;
    return true;
}

const pmetal_env_source_authority::bank_donor *
resolve_bank(
    const std::uint8_t *base,
    std::uint16_t count,
    std::uint64_t &signature) noexcept
{
    signature = 0u;
    if (base == nullptr ||
        count == 0u)
        return nullptr;

    const auto generation =
        g_source_cache_generation.load(
            std::memory_order_relaxed);
    auto &cached =
        bank_cache_for(base);

    if (cached.valid &&
        cached.generation == generation &&
        cached.base == base &&
        cached.count == count) {
        signature =
            cached.signature;
        return cached.bank;
    }

    std::uint64_t decoded = 0u;
    if (!bank_signature(
            base,
            decoded)) {
        cached = {};
        return nullptr;
    }

    const auto *bank =
        pmetal_env_source_authority::
            find_bank(decoded);

    cached.base = base;
    cached.count = count;
    cached.generation = generation;
    cached.signature = decoded;
    cached.bank = bank;
    cached.valid = true;

    signature = decoded;
    return bank;
}


struct endpoint_cache_entry {
    void *source = nullptr;
    const std::uint8_t *base = nullptr;
    std::uint16_t count = 0u;
    std::uint32_t index = 0u;
    std::uint32_t row_id = 0u;
    std::uint64_t generation = 0u;
    std::uint64_t signature = 0u;
    f4 value{};
    bool valid = false;
};

constexpr std::size_t k_endpoint_cache_sets = 256u;
constexpr std::size_t k_endpoint_cache_ways = 2u;
constexpr std::size_t k_endpoint_cache_entries =
    k_endpoint_cache_sets *
    k_endpoint_cache_ways;

thread_local std::array<
    endpoint_cache_entry,
    k_endpoint_cache_entries>
    g_endpoint_cache{};
thread_local std::array<
    std::uint8_t,
    k_endpoint_cache_sets>
    g_endpoint_cache_victim{};

std::size_t endpoint_cache_set(
    const void *source,
    const void *base_ptr,
    std::uint32_t index) noexcept
{
    auto value =
        reinterpret_cast<std::uintptr_t>(source);
    value ^=
        reinterpret_cast<std::uintptr_t>(base_ptr) >> 4u;
    value ^=
        static_cast<std::uintptr_t>(index) << 9u;
    value ^=
        value >> 17u;
    value ^=
        value >> 31u;

    return static_cast<std::size_t>(
        value & (k_endpoint_cache_sets - 1u));
}

bool endpoint_cache_lookup(
    void *source,
    const std::uint8_t *base,
    std::uint16_t count,
    std::uint32_t index,
    std::uint32_t row_id,
    f4 &value,
    std::uint64_t &signature) noexcept
{
    const auto generation =
        g_source_cache_generation.load(
            std::memory_order_relaxed);
    const auto set =
        endpoint_cache_set(
            source,
            base,
            index);
    const auto first =
        set * k_endpoint_cache_ways;

    for (std::size_t way = 0u;
         way < k_endpoint_cache_ways;
         ++way) {
        const auto &entry =
            g_endpoint_cache[first + way];
        if (entry.valid &&
            entry.generation == generation &&
            entry.source == source &&
            entry.base == base &&
            entry.count == count &&
            entry.index == index &&
            entry.row_id == row_id) {
            value = entry.value;
            signature = entry.signature;
            g_endpoint_cache_hit.fetch_add(
                1u,
                std::memory_order_relaxed);
            return true;
        }
    }

    g_endpoint_cache_miss.fetch_add(
        1u,
        std::memory_order_relaxed);
    return false;
}

void endpoint_cache_publish(
    void *source,
    const std::uint8_t *base,
    std::uint16_t count,
    std::uint32_t index,
    std::uint32_t row_id,
    const f4 &value,
    std::uint64_t signature) noexcept
{
    const auto generation =
        g_source_cache_generation.load(
            std::memory_order_relaxed);
    const auto set =
        endpoint_cache_set(
            source,
            base,
            index);
    const auto first =
        set * k_endpoint_cache_ways;

    std::size_t target = k_endpoint_cache_ways;
    for (std::size_t way = 0u;
         way < k_endpoint_cache_ways;
         ++way) {
        const auto &entry =
            g_endpoint_cache[first + way];
        if (!entry.valid ||
            entry.generation != generation ||
            (entry.source == source &&
             entry.base == base &&
             entry.index == index)) {
            target = way;
            break;
        }
    }

    if (target == k_endpoint_cache_ways) {
        target =
            static_cast<std::size_t>(
                g_endpoint_cache_victim[set]++ &
                static_cast<std::uint8_t>(
                    k_endpoint_cache_ways - 1u));
    }

    auto &entry =
        g_endpoint_cache[first + target];
    entry.source = source;
    entry.base = base;
    entry.count = count;
    entry.index = index;
    entry.row_id = row_id;
    entry.generation = generation;
    entry.signature = signature;
    entry.value = value;
    entry.valid = true;

    g_endpoint_cache_fill.fetch_add(
        1u,
        std::memory_order_relaxed);
}

bool read_exact_source(
    void *source,
    std::int32_t selector,
    f4 &out,
    std::uint64_t &signature,
    std::uint32_t &row_id) noexcept
{
    out = {};
    signature = 0u;
    row_id = 0u;

    if (source == nullptr ||
        selector < 0)
        return false;

    const std::uint8_t *base = nullptr;
    if (!safe_read(
            static_cast<const std::uint8_t *>(
                source) +
                0x18u,
            base) ||
        base == nullptr)
        return false;

    std::uint32_t index = 0u;
    if (!retail_lightbank_record_index(
            selector,
            index))
        return false;

    std::uint16_t version = 0u;
    std::uint16_t count = 0u;
    if (!safe_read(
            base + 8u,
            version) ||
        !safe_read(
            base + 10u,
            count) ||
        version != 4u ||
        count == 0u ||
        count > 256u ||
        index >= count)
        return false;

    const auto *entry =
        base +
        0x30u +
        static_cast<std::size_t>(
            index) * 12u;

    if (!safe_read(
            entry,
            row_id))
        return false;

    if (endpoint_cache_lookup(
            source,
            base,
            count,
            index,
            row_id,
            out,
            signature))
        return true;

    const auto *bank =
        resolve_bank(
            base,
            count,
            signature);
    if (bank == nullptr)
        return false;

    const auto *row =
        pmetal_env_source_authority::
            find_row(
                *bank,
                row_id);
    if (row == nullptr)
        return false;

    const float scale =
        static_cast<float>(
            row->m) *
        0.01f;

    out = {
        static_cast<float>(
            row->r) /
            255.0f * scale,
        static_cast<float>(
            row->g) /
            255.0f * scale,
        static_cast<float>(
            row->b) /
            255.0f * scale,
        0.0f
    };

    if (!std::isfinite(out.x) ||
        !std::isfinite(out.y) ||
        !std::isfinite(out.z))
        return false;

    endpoint_cache_publish(
        source,
        base,
        count,
        index,
        row_id,
        out,
        signature);
    return true;
}

std::uintptr_t g_source_base = 0u;
std::atomic_bool g_selector_enabled{false};
std::atomic<std::uint64_t> g_selector_epoch{1u};

std::atomic<std::uint64_t> g_publish{0u};
std::atomic<std::uint64_t> g_consumer_ok{0u};
std::atomic<std::uint64_t> g_consumer_fail{0u};
std::atomic<std::uint64_t> g_decode_fail{0u};
std::atomic<std::uint64_t> g_steady_seen{0u};
std::atomic<std::uint64_t> g_blend_seen{0u};

std::atomic<std::uint32_t> g_last_publish_tid{0u};
std::atomic<std::uint32_t> g_last_consumer_tid{0u};
std::atomic_bool g_last_consumer_local_valid{false};

std::atomic_bool g_selector_exact_seen{false};
std::atomic_bool g_parent_gate_ok{false};
std::atomic_bool g_descriptor_gate_ok{false};
std::atomic_bool g_endpoint_gate_ok{false};
std::atomic_bool g_manager_gate_ok{false};
std::atomic_bool g_source_a_decode_ok{false};
std::atomic_bool g_source_b_decode_ok{false};

} // namespace

void pmetal_env_source_cache_invalidate() noexcept
{
    g_source_cache_generation.fetch_add(
        1u,
        std::memory_order_relaxed);
}

bool pmetal_env_source_runtime::install() noexcept
{
    pmetal_env_source_cache_invalidate();

    g_source_base =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));

    g_selector_epoch.fetch_add(
        1u,
        std::memory_order_relaxed);

    g_selector_enabled.store(
        g_source_base != 0u,
        std::memory_order_release);

    return g_source_base != 0u;
}

void pmetal_env_source_runtime::uninstall() noexcept
{
    g_selector_enabled.store(
        false,
        std::memory_order_release);
    g_selector_epoch.fetch_add(
        1u,
        std::memory_order_relaxed);
    pmetal_env_source_cache_invalidate();
    pmetal_env_source_selector_clear();
}

void pmetal_env_source_selector_clear() noexcept
{
    pmetal_producer_state_clear();
}

void pmetal_env_source_selector_event(
    void *owner,
    void *return_address,
    void *r14,
    void *r15,
    const void *selector_stack,
    const operators::material_response::
        material_identity &material) noexcept
{
    pmetal_producer_state_clear();

    if (!g_selector_enabled.load(
            std::memory_order_acquire) ||
        !exact_pmetal_material_selection(
            material) ||
        owner == nullptr ||
        return_address == nullptr)
        return;

    if (telemetry::effect_enabled())
        g_selector_exact_seen.store(
            true,
            std::memory_order_relaxed);

    const auto absolute =
        reinterpret_cast<std::uintptr_t>(
            return_address);

    if (absolute < g_source_base)
        return;

    const auto rva =
        absolute - g_source_base;

    const auto *descriptor =
        static_cast<const std::uint8_t *>(
            (rva == k_ret_sel_1 ||
             rva == k_ret_sel_3)
                ? r15
                : rva == k_ret_sel_2
                    ? r14
                    : nullptr);

    if (descriptor == nullptr ||
        selector_stack == nullptr)
        return;

    std::uintptr_t parent_return = 0u;
    const auto parent_offset =
        rva == k_ret_sel_1
            ? 0x170u
            : 0xF0u;

    if (!safe_read(
            static_cast<const std::uint8_t *>(
                selector_stack) +
                parent_offset,
            parent_return) ||
        parent_return !=
            g_source_base +
                k_parent_return)
        return;

    if (telemetry::effect_enabled())
        g_parent_gate_ok.store(
            true,
            std::memory_order_relaxed);

    if (!readable_range(
            descriptor,
            0x151u))
        return;

    void *descriptor_owner = nullptr;
    std::uint16_t raw_a = 0u;
    std::uint16_t raw_b = 0u;
    float beta = 0.0f;

    std::memcpy(
        &descriptor_owner,
        descriptor,
        sizeof(descriptor_owner));
    std::memcpy(
        &raw_a,
        descriptor + 0x4Cu,
        sizeof(raw_a));
    std::memcpy(
        &raw_b,
        descriptor + 0x4Eu,
        sizeof(raw_b));
    std::memcpy(
        &beta,
        descriptor + 0x50u,
        sizeof(beta));

    const auto character =
        descriptor[0x150u];

    if (descriptor_owner != owner ||
        character > 1u)
        return;

    if (telemetry::effect_enabled())
        g_descriptor_gate_ok.store(
            true,
            std::memory_order_relaxed);

    const auto endpoints =
        pmetal_selector_policy::select(
            static_cast<std::int16_t>(
                raw_a),
            static_cast<std::int16_t>(
                raw_b),
            beta);

    if (!endpoints.valid)
        return;

    if (telemetry::effect_enabled())
        g_endpoint_gate_ok.store(
            true,
            std::memory_order_relaxed);

    const std::uint8_t *wrapper = nullptr;
    const std::uint8_t *manager = nullptr;

    if (!safe_read(
            static_cast<const std::uint8_t *>(
                owner) +
                0x2318u,
            wrapper) ||
        wrapper == nullptr ||
        !safe_read(
            wrapper + 0x40u,
            manager) ||
        manager == nullptr)
        return;

    if (telemetry::effect_enabled())
        g_manager_gate_ok.store(
            true,
            std::memory_order_relaxed);

    bool lookup_valid = true;

    auto lookup =
        [&](unsigned area,
            unsigned type) noexcept
            -> void * {
            const std::uint8_t *table =
                nullptr;
            void *source = nullptr;

            if (!safe_read(
                    manager +
                        0x20u +
                        area * 0x1B0u,
                    table) ||
                table == nullptr ||
                !safe_read(
                    table +
                        type * 0x10u +
                        8u,
                    source)) {
                lookup_valid = false;
                return nullptr;
            }

            return source;
        };

    auto decode =
        [&](std::int16_t selector,
            f4 &value,
            std::uint64_t &bank,
            std::uint32_t &row) noexcept {
            void *source =
                pmetal_selector_policy::
                    source(
                        selector,
                        character != 0u,
                        lookup);

            if (!lookup_valid ||
                source == nullptr)
                return false;

            return read_exact_source(
                source,
                selector,
                value,
                bank,
                row);
        };

    f4 a{};
    f4 b{};
    pmetal_envspec_source next{};

    if (!decode(
            endpoints.a,
            a,
            next.bank_signature_a,
            next.row_id_a)) {
        telemetry::hot_count(
            g_decode_fail);
        return;
    }

    if (telemetry::effect_enabled())
        g_source_a_decode_ok.store(
            true,
            std::memory_order_relaxed);

    if (endpoints.a == endpoints.b) {
        b = a;
        next.bank_signature_b =
            next.bank_signature_a;
        next.row_id_b =
            next.row_id_a;
    } else if (!decode(
                   endpoints.b,
                   b,
                   next.bank_signature_b,
                   next.row_id_b)) {
        telemetry::hot_count(
            g_decode_fail);
        return;
    }

    if (telemetry::effect_enabled())
        g_source_b_decode_ok.store(
            true,
            std::memory_order_relaxed);

    next.a = {
        a.x,
        a.y,
        a.z
    };
    next.b = {
        b.x,
        b.y,
        b.z
    };
    next.beta =
        endpoints.beta;

    const auto epoch =
        g_selector_epoch.load(
            std::memory_order_relaxed);

    next.serial = epoch;

    pmetal_producer_state_publish(
        material,
        next,
        epoch);

    if (telemetry::effect_enabled())
        g_last_publish_tid.store(
            static_cast<std::uint32_t>(
                GetCurrentThreadId()),
            std::memory_order_relaxed);

    telemetry::hot_count(
        g_publish);
    telemetry::hot_count(
        endpoints.beta == 0.0f
            ? g_steady_seen
            : g_blend_seen);
}

bool pmetal_env_source_runtime::latest(
    const operators::material_response::
        material_identity &material,
    pmetal_envspec_source &out) const noexcept
{
    out = {};

    const bool local_valid =
        pmetal_producer_state_valid();

    if (telemetry::effect_enabled()) {
        g_last_consumer_tid.store(
            static_cast<std::uint32_t>(
                GetCurrentThreadId()),
            std::memory_order_relaxed);
        g_last_consumer_local_valid.store(
            local_valid,
            std::memory_order_relaxed);
    }

    const auto epoch =
        g_selector_epoch.load(
            std::memory_order_relaxed);

    if (!g_selector_enabled.load(
            std::memory_order_acquire) ||
        !exact_pmetal_material_selection(
            material) ||
        !pmetal_producer_state_latest(
            material,
            epoch,
            out)) {
        telemetry::hot_count(
            g_consumer_fail);
        return false;
    }

    telemetry::hot_count(
        g_consumer_ok);
    return true;
}

pmetal_env_source_runtime_telemetry
pmetal_env_source_runtime::telemetry() const noexcept
{
    pmetal_env_source_runtime_telemetry out{};

    out.steady_seen =
        g_steady_seen.load(
            std::memory_order_relaxed);
    out.blend_seen =
        g_blend_seen.load(
            std::memory_order_relaxed);
    out.exact_publish =
        g_publish.load(
            std::memory_order_relaxed);
    out.decode_fail =
        g_decode_fail.load(
            std::memory_order_relaxed);
    out.consumer_ok =
        g_consumer_ok.load(
            std::memory_order_relaxed);
    out.consumer_fail =
        g_consumer_fail.load(
            std::memory_order_relaxed);
    out.endpoint_cache_hit =
        g_endpoint_cache_hit.load(
            std::memory_order_relaxed);
    out.endpoint_cache_miss =
        g_endpoint_cache_miss.load(
            std::memory_order_relaxed);
    out.endpoint_cache_fill =
        g_endpoint_cache_fill.load(
            std::memory_order_relaxed);
    out.region_cache_hit =
        g_region_cache_hit.load(
            std::memory_order_relaxed);
    out.region_cache_miss =
        g_region_cache_miss.load(
            std::memory_order_relaxed);
    out.cache_generation =
        g_source_cache_generation.load(
            std::memory_order_relaxed);

    out.last_publish_tid =
        g_last_publish_tid.load(
            std::memory_order_relaxed);
    out.last_consumer_tid =
        g_last_consumer_tid.load(
            std::memory_order_relaxed);
    out.last_consumer_local_valid =
        g_last_consumer_local_valid.load(
            std::memory_order_relaxed);

    out.selector_exact_seen =
        g_selector_exact_seen.load(
            std::memory_order_relaxed);
    out.parent_gate_ok =
        g_parent_gate_ok.load(
            std::memory_order_relaxed);
    out.descriptor_gate_ok =
        g_descriptor_gate_ok.load(
            std::memory_order_relaxed);
    out.endpoint_gate_ok =
        g_endpoint_gate_ok.load(
            std::memory_order_relaxed);
    out.manager_gate_ok =
        g_manager_gate_ok.load(
            std::memory_order_relaxed);
    out.source_a_decode_ok =
        g_source_a_decode_ok.load(
            std::memory_order_relaxed);
    out.source_b_decode_ok =
        g_source_b_decode_ok.load(
            std::memory_order_relaxed);

    out.selector_carrier_active =
        g_selector_enabled.load(
            std::memory_order_relaxed);

    return out;
}

void pmetal_env_source_runtime::reset() noexcept
{
    g_selector_epoch.fetch_add(
        1u,
        std::memory_order_relaxed);
    pmetal_env_source_cache_invalidate();
    pmetal_env_source_selector_clear();

    g_publish.store(
        0u,
        std::memory_order_relaxed);
    g_consumer_ok.store(
        0u,
        std::memory_order_relaxed);
    g_consumer_fail.store(
        0u,
        std::memory_order_relaxed);
    g_decode_fail.store(
        0u,
        std::memory_order_relaxed);
    g_steady_seen.store(
        0u,
        std::memory_order_relaxed);
    g_blend_seen.store(
        0u,
        std::memory_order_relaxed);
    g_endpoint_cache_hit.store(
        0u,
        std::memory_order_relaxed);
    g_endpoint_cache_miss.store(
        0u,
        std::memory_order_relaxed);
    g_endpoint_cache_fill.store(
        0u,
        std::memory_order_relaxed);
    g_region_cache_hit.store(
        0u,
        std::memory_order_relaxed);
    g_region_cache_miss.store(
        0u,
        std::memory_order_relaxed);

    g_last_publish_tid.store(
        0u,
        std::memory_order_relaxed);
    g_last_consumer_tid.store(
        0u,
        std::memory_order_relaxed);
    g_last_consumer_local_valid.store(
        false,
        std::memory_order_relaxed);

    g_selector_exact_seen.store(
        false,
        std::memory_order_relaxed);
    g_parent_gate_ok.store(
        false,
        std::memory_order_relaxed);
    g_descriptor_gate_ok.store(
        false,
        std::memory_order_relaxed);
    g_endpoint_gate_ok.store(
        false,
        std::memory_order_relaxed);
    g_manager_gate_ok.store(
        false,
        std::memory_order_relaxed);
    g_source_a_decode_ok.store(
        false,
        std::memory_order_relaxed);
    g_source_b_decode_ok.store(
        false,
        std::memory_order_relaxed);

    g_bank_cache = {};
    g_bank_victim = {};
    g_endpoint_cache = {};
    g_endpoint_cache_victim = {};
    g_region_cache = {};
    g_region_cache_victim = {};
}

} // namespace dsrrl::runtime
