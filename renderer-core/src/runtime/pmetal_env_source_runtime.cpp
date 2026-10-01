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

struct readable_window {
    std::uintptr_t begin = 0u;
    std::uintptr_t end = 0u;
};

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

        const DWORD access =
            mbi.Protect & 0xffu;
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

    const DWORD access =
        mbi.Protect & 0xffu;
    const bool readable =
        access == PAGE_READONLY ||
        access == PAGE_READWRITE ||
        access == PAGE_WRITECOPY ||
        access == PAGE_EXECUTE_READ ||
        access == PAGE_EXECUTE_READWRITE ||
        access == PAGE_EXECUTE_WRITECOPY;

    if (!readable)
        return false;

    const auto begin =
        reinterpret_cast<std::uintptr_t>(
            mbi.BaseAddress);
    const auto end =
        begin + mbi.RegionSize;

    if (end <= address ||
        end < begin)
        return false;

    window.begin = begin;
    window.end = end;
    return true;
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
            entry.base == base_ptr)
            return entry;
    }

    for (std::size_t way = 0u;
         way < k_bank_cache_ways;
         ++way) {
        auto &entry =
            g_bank_cache[base + way];
        if (!entry.valid) {
            entry = {};
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

    auto &cached =
        bank_cache_for(base);

    if (cached.valid &&
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
    cached.signature = decoded;
    cached.bank = bank;
    cached.valid = true;

    signature = decoded;
    return bank;
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
    std::memcpy(
        &base,
        static_cast<const std::uint8_t *>(
            source) + 0x18u,
        sizeof(base));

    if (base == nullptr)
        return false;

    std::uint32_t index = 0u;
    if (!retail_lightbank_record_index(
            selector,
            index))
        return false;

    auto &cached =
        bank_cache_for(base);

    const pmetal_env_source_authority::
        bank_donor *bank = nullptr;
    std::uint16_t count = 0u;

    if (cached.valid &&
        cached.base == base) {
        std::uint16_t version = 0u;
        std::uint16_t live_count = 0u;

        std::memcpy(
            &version,
            base + 8u,
            sizeof(version));
        std::memcpy(
            &live_count,
            base + 10u,
            sizeof(live_count));

        if (version != 4u ||
            live_count != cached.count ||
            live_count == 0u ||
            live_count > 256u) {
            cached = {};
            return false;
        }

        count = live_count;
        signature = cached.signature;
        bank = cached.bank;

        if (index >= count ||
            bank == nullptr)
            return false;
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
            count > 256u ||
            index >= count)
            return false;

        bank =
            resolve_bank(
                base,
                count,
                signature);
        if (bank == nullptr)
            return false;
    }

    const auto *entry =
        base +
        0x30u +
        static_cast<std::size_t>(
            index) * 12u;

    if (cached.valid &&
        cached.base == base) {
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

    return
        std::isfinite(out.x) &&
        std::isfinite(out.y) &&
        std::isfinite(out.z);
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

bool pmetal_env_source_runtime::install() noexcept
{
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

            const std::uint8_t *base =
                nullptr;

            if (!lookup_valid ||
                source == nullptr ||
                !safe_read(
                    static_cast<
                        const std::uint8_t *>(
                            source) +
                        0x18u,
                    base) ||
                base == nullptr ||
                !readable_range(
                    base,
                    0x30u))
                return false;

            std::uint16_t count = 0u;
            std::memcpy(
                &count,
                base + 10u,
                sizeof(count));

            if (count == 0u ||
                count > 256u ||
                !readable_range(
                    base,
                    0x30u +
                        static_cast<std::size_t>(
                            count) *
                            12u))
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
}

} // namespace dsrrl::runtime
