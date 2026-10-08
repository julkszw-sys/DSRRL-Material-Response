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
#include <reshade.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

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

#if !defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
bool inverse_envdiffuse_endpoint(
    const float *src,
    f4 &out) noexcept
{
    out = {};
    if (src == nullptr)
        return false;

    constexpr float k_inv_gamma =
        1.0f / 2.2f;

    const float q[3] = {
        src[0],
        src[1],
        src[2]
    };

    for (float value : q) {
        if (!std::isfinite(value) ||
            value < 0.0f)
            return false;
    }

    out.x = std::pow(q[0], k_inv_gamma);
    out.y = std::pow(q[1], k_inv_gamma);
    out.z = std::pow(q[2], k_inv_gamma);
    out.w = src[3];

    return
        std::isfinite(out.x) &&
        std::isfinite(out.y) &&
        std::isfinite(out.z);
}

bool capture_linear_envdiffuse(
    const float *host_out,
    f4 &a,
    f4 &b) noexcept
{
    // Legacy diagnostic only: recover the host q-domain endpoints. The full
    // PTDE HemEnv diagnostic never enters this path.
    return
        host_out != nullptr &&
        inverse_envdiffuse_endpoint(
            host_out + 0,
            a) &&
        inverse_envdiffuse_endpoint(
            host_out + 8,
            b);
}
#else
constexpr char k_ptde_envdiffuse_donor_relative_path[] =
    "DSRRL\\EnvSpec\\PackedGI\\PMETAL_PTDE_ENVDIFFUSE_DONOR_V1.bin";
constexpr std::uint64_t k_ptde_envdiffuse_donor_size = 15296ull;
constexpr std::array<std::uint8_t,32>
k_ptde_envdiffuse_donor_sha256 = {{
    0x29u,0xd3u,0x71u,0x5au,0xaeu,0x54u,0xe7u,0xaeu,
    0x2fu,0xe6u,0x6du,0xcdu,0x0cu,0x01u,0x6fu,0x5eu,
    0x34u,0x5cu,0xccu,0x6fu,0xd5u,0xafu,0x94u,0x77u,
    0x98u,0x5du,0x4du,0x72u,0x0fu,0x6fu,0xafu,0x92u
}};

struct exact_envdiffuse_bank {
    std::uint64_t signature = 0u;
    std::uint32_t first = 0u;
    std::uint32_t count = 0u;
};
struct exact_envdiffuse_row {
    std::uint32_t id = 0u;
    std::uint16_t r = 0u;
    std::uint16_t g = 0u;
    std::uint16_t b = 0u;
    std::uint16_t m = 0u;
};

std::array<exact_envdiffuse_bank,20>
    g_exact_envdiffuse_banks{};
std::array<exact_envdiffuse_row,1246>
    g_exact_envdiffuse_rows{};
bool g_exact_envdiffuse_ready = false;

std::filesystem::path pmetal_process_dir()
{
    std::wstring buffer(32768, L'\0');
    const DWORD size =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(
                buffer.size()));
    if (size == 0u ||
        size >= buffer.size())
        return {};
    buffer.resize(size);
    return std::filesystem::path(
        buffer).parent_path();
}

template <typename T>
bool donor_read(
    const std::vector<std::uint8_t> &bytes,
    std::size_t &offset,
    T &value) noexcept
{
    if (offset > bytes.size() ||
        sizeof(T) > bytes.size() - offset)
        return false;
    std::memcpy(
        &value,
        bytes.data() + offset,
        sizeof(T));
    offset += sizeof(T);
    return true;
}

bool load_exact_envdiffuse_donor() noexcept
{
    g_exact_envdiffuse_ready = false;
    g_exact_envdiffuse_banks = {};
    g_exact_envdiffuse_rows = {};

    try {
        const auto root =
            pmetal_process_dir();
        if (root.empty())
            return false;

        const auto path =
            root /
            std::filesystem::path(
                k_ptde_envdiffuse_donor_relative_path);

        std::error_code ec;
        const auto size =
            std::filesystem::file_size(
                path,
                ec);
        if (ec ||
            size !=
                k_ptde_envdiffuse_donor_size)
            return false;

        std::ifstream stream(
            path,
            std::ios::binary);
        if (!stream)
            return false;

        std::vector<std::uint8_t> bytes(
            static_cast<std::size_t>(size));
        if (!stream.read(
                reinterpret_cast<char *>(
                    bytes.data()),
                static_cast<std::streamsize>(
                    bytes.size())))
            return false;

        if (operators::legacy_plan::hashing::sha256(
                bytes.data(),
                bytes.size()) !=
            k_ptde_envdiffuse_donor_sha256)
            return false;

        std::size_t offset = 0u;
        std::array<char,8> magic{};
        if (offset + magic.size() >
            bytes.size())
            return false;
        std::memcpy(
            magic.data(),
            bytes.data(),
            magic.size());
        offset += magic.size();

        constexpr std::array<char,8>
            expected_magic{{
                'D','S','R','E','D','V','0','1'
            }};
        if (magic != expected_magic)
            return false;

        std::uint32_t version = 0u;
        std::uint32_t row_count = 0u;
        std::uint32_t bank_count = 0u;
        std::uint32_t row_stride = 0u;
        if (!donor_read(bytes,offset,version) ||
            !donor_read(bytes,offset,row_count) ||
            !donor_read(bytes,offset,bank_count) ||
            !donor_read(bytes,offset,row_stride) ||
            version != 1u ||
            row_count !=
                g_exact_envdiffuse_rows.size() ||
            bank_count !=
                g_exact_envdiffuse_banks.size() ||
            row_stride != 12u)
            return false;

        for (std::size_t i = 0u;
             i <
                g_exact_envdiffuse_banks.size();
             ++i) {
            auto &bank =
                g_exact_envdiffuse_banks[i];
            if (!donor_read(
                    bytes,
                    offset,
                    bank.signature) ||
                !donor_read(
                    bytes,
                    offset,
                    bank.first) ||
                !donor_read(
                    bytes,
                    offset,
                    bank.count))
                return false;

            const auto &authority =
                pmetal_env_source_authority::
                    k_banks[i];
            if (bank.signature !=
                    authority.signature ||
                bank.first !=
                    authority.first ||
                bank.count !=
                    authority.count ||
                bank.first >
                    g_exact_envdiffuse_rows.size() ||
                bank.count >
                    g_exact_envdiffuse_rows.size() -
                        bank.first)
                return false;
        }

        for (auto &row :
             g_exact_envdiffuse_rows) {
            if (!donor_read(
                    bytes,offset,row.id) ||
                !donor_read(
                    bytes,offset,row.r) ||
                !donor_read(
                    bytes,offset,row.g) ||
                !donor_read(
                    bytes,offset,row.b) ||
                !donor_read(
                    bytes,offset,row.m))
                return false;
        }

        if (offset != bytes.size())
            return false;

        for (const auto &bank :
             g_exact_envdiffuse_banks) {
            for (std::uint32_t j = 0u;
                 j < bank.count;
                 ++j) {
                const auto &row =
                    g_exact_envdiffuse_rows[
                        bank.first + j];
                if (row.id != j)
                    return false;
            }
        }

        g_exact_envdiffuse_ready = true;
        return true;
    } catch (...) {
        g_exact_envdiffuse_banks = {};
        g_exact_envdiffuse_rows = {};
        return false;
    }
}

bool exact_envdiffuse_endpoint(
    std::uint64_t signature,
    std::uint32_t row_id,
    f4 &out) noexcept
{
    out = {};
    if (!g_exact_envdiffuse_ready)
        return false;

    for (const auto &bank :
         g_exact_envdiffuse_banks) {
        if (bank.signature != signature)
            continue;
        if (row_id >= bank.count)
            return false;

        const auto &row =
            g_exact_envdiffuse_rows[
                bank.first + row_id];
        if (row.id != row_id)
            return false;

        const float scale =
            static_cast<float>(
                row.m) *
            0.01f;
        out = {
            static_cast<float>(row.r) /
                255.0f * scale,
            static_cast<float>(row.g) /
                255.0f * scale,
            static_cast<float>(row.b) /
                255.0f * scale,
            0.0f
        };
        return
            std::isfinite(out.x) &&
            std::isfinite(out.y) &&
            std::isfinite(out.z);
    }
    return false;
}
#endif

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
    std::uint64_t semantic_version = 0u;
    bool valid = false;
};

thread_local hook_source_record g_hook_source_tls{};

struct hook_selector_identity_v1 {
    void *source_a = nullptr;
    void *source_b = nullptr;
    const std::uint8_t *base_a = nullptr;
    const std::uint8_t *base_b = nullptr;
    std::int16_t selector_a = -1;
    std::int16_t selector_b = -1;
    std::uint16_t count_a = 0u;
    std::uint16_t count_b = 0u;
    std::uint32_t row_id_a = 0u;
    std::uint32_t row_id_b = 0u;
    std::uint32_t beta_bits = 0u;
    bool valid = false;
};

thread_local hook_selector_identity_v1
    g_hook_selector_identity_tls{};

std::mutex g_hook_source_mutex;
hook_source_record g_hook_source_global{};
std::atomic<std::uint64_t> g_hook_source_serial{0u};
std::atomic<std::uint64_t> g_hook_source_generation{0u};
std::atomic<std::uint64_t> g_hook_source_semantic_version{0u};

struct hook_producer_cache_entry_v2 {
    hook_selector_identity_v1 key{};
    hook_source_record record{};
    std::uint64_t cache_generation = 0u;
    bool valid = false;
};

constexpr std::size_t k_hook_producer_cache_sets = 256u;
constexpr std::size_t k_hook_producer_cache_ways = 2u;
constexpr std::size_t k_hook_producer_cache_entries =
    k_hook_producer_cache_sets * k_hook_producer_cache_ways;

std::array<hook_producer_cache_entry_v2,k_hook_producer_cache_entries>
    g_hook_producer_cache{};
std::array<std::mutex,k_hook_producer_cache_sets>
    g_hook_producer_cache_mutex{};
std::array<std::uint8_t,k_hook_producer_cache_sets>
    g_hook_producer_cache_victim{};
std::atomic<std::uint64_t> g_hook_producer_cache_generation{1u};
std::atomic<std::uint64_t> g_hook_producer_cache_hit{0u};
std::atomic<std::uint64_t> g_hook_producer_cache_miss{0u};
std::atomic<std::uint64_t> g_hook_producer_cache_busy{0u};
std::atomic<std::uint64_t> g_hook_producer_cache_publish{0u};
std::atomic<std::uint64_t> g_hook_single_seen{0u};
std::atomic<std::uint64_t> g_hook_blend_seen{0u};
std::atomic<std::uint64_t> g_hook_publish{0u};
std::atomic<std::uint64_t> g_hook_consume{0u};
std::atomic<std::uint32_t> g_hook_decode_stage{0u};
std::atomic<std::uint32_t> g_hook_decode_version{0u};
std::atomic<std::uint32_t> g_hook_decode_count{0u};
std::atomic<std::uint32_t> g_hook_decode_index{0u};
std::atomic<std::uint32_t> g_hook_decode_row_id{0u};
std::atomic<std::uint64_t> g_hook_decode_signature{0u};

// Diagnostic-only structural bank-signature frontier. These atomics are
// written by the existing scan itself; they never trigger an extra read or
// authorize a donor. scan_count makes the bounded PR179 retry observable.
std::atomic<std::uint64_t> g_bank_signature_scan_count{0u};
std::atomic<std::uint32_t> g_bank_signature_attempt{0u};
std::atomic<std::uint32_t> g_bank_signature_stage{0u};
std::atomic<std::uint32_t> g_bank_signature_entry{0u};
std::atomic<std::uint32_t> g_bank_signature_name_offset{0u};
std::atomic<std::uint32_t> g_bank_signature_consumed{0u};
std::atomic<std::uint64_t> g_bank_layout_signature{0u};

enum bank_signature_stage : std::uint32_t {
    bank_signature_none = 0u,
    bank_signature_header_range = 1u,
    bank_signature_header_semantic = 2u,
    bank_signature_table_range = 3u,
    bank_signature_layout_unknown = 4u,
    bank_signature_ok = 9u
};

void record_bank_signature_stage(
    std::uint32_t attempt,
    std::uint32_t stage,
    std::uint32_t entry,
    std::uint32_t name_offset,
    std::uint32_t consumed) noexcept
{
    g_bank_signature_attempt.store(attempt,std::memory_order_relaxed);
    g_bank_signature_stage.store(stage,std::memory_order_relaxed);
    g_bank_signature_entry.store(entry,std::memory_order_relaxed);
    g_bank_signature_name_offset.store(name_offset,std::memory_order_relaxed);
    g_bank_signature_consumed.store(consumed,std::memory_order_relaxed);
}

bool same_hook_source_payload(
    const pmetal_envspec_source &a,
    const pmetal_envspec_source &b) noexcept;

bool same_hook_selector_identity(
    const hook_selector_identity_v1 &a,
    const hook_selector_identity_v1 &b) noexcept
{
    if (!a.valid || !b.valid ||
        a.source_a != b.source_a ||
        a.base_a != b.base_a ||
        a.selector_a != b.selector_a ||
        a.count_a != b.count_a ||
        a.row_id_a != b.row_id_a ||
        a.beta_bits != b.beta_bits)
        return false;

    if (a.beta_bits == 0u)
        return true;

    return
        a.source_b == b.source_b &&
        a.base_b == b.base_b &&
        a.selector_b == b.selector_b &&
        a.count_b == b.count_b &&
        a.row_id_b == b.row_id_b;
}

std::size_t hook_producer_cache_set(
    const hook_selector_identity_v1 &key) noexcept
{
    auto h =
        reinterpret_cast<std::uintptr_t>(
            key.source_a);
    h ^= reinterpret_cast<std::uintptr_t>(
             key.base_a) >> 4u;
    h ^= static_cast<std::uintptr_t>(
             key.row_id_a) *
         0x9e3779b1u;
    h ^= static_cast<std::uintptr_t>(
             key.count_a) << 11u;
    h ^= reinterpret_cast<std::uintptr_t>(
             key.source_b) +
         0x9e3779b97f4a7c15ULL +
         (h << 6u) +
         (h >> 2u);
    h ^= reinterpret_cast<std::uintptr_t>(
             key.base_b) >> 7u;
    h ^= static_cast<std::uintptr_t>(
             key.row_id_b) << 29u;
    h ^= static_cast<std::uintptr_t>(
             static_cast<std::uint16_t>(
                 key.selector_a)) << 17u;
    h ^= static_cast<std::uintptr_t>(
             static_cast<std::uint16_t>(
                 key.selector_b)) << 33u;
    h ^= static_cast<std::uintptr_t>(
             key.beta_bits) *
         0x9e3779b1u;
    h ^= h >> 23u;
    h *= 0x2127599bf4325c37ULL;
    h ^= h >> 47u;

    return static_cast<std::size_t>(
        h &
        (k_hook_producer_cache_sets - 1u));
}

bool hook_producer_cache_lookup(
    const hook_selector_identity_v1 &key,
    hook_source_record &out,
    const pmetal_envspec_source *payload = nullptr) noexcept
{
    out = {};
    if (!key.valid)
        return false;

    const auto set =
        hook_producer_cache_set(key);
    std::unique_lock<std::mutex> lock(
        g_hook_producer_cache_mutex[set],
        std::try_to_lock);
    if (!lock.owns_lock()) {
        g_hook_producer_cache_busy.fetch_add(
            1u,
            std::memory_order_relaxed);
        return false;
    }

    const auto generation =
        g_hook_producer_cache_generation.load(
            std::memory_order_relaxed);
    const auto base =
        set * k_hook_producer_cache_ways;

    for (std::size_t way = 0u;
         way < k_hook_producer_cache_ways;
         ++way) {
        const auto &entry =
            g_hook_producer_cache[base + way];
        if (!entry.valid ||
            entry.cache_generation != generation ||
            !same_hook_selector_identity(
                entry.key,
                key))
            continue;

        if (payload != nullptr &&
            !same_hook_source_payload(
                entry.record.source,
                *payload))
            continue;

        out = entry.record;
        g_hook_producer_cache_hit.fetch_add(
            1u,
            std::memory_order_relaxed);
        return true;
    }

    g_hook_producer_cache_miss.fetch_add(
        1u,
        std::memory_order_relaxed);
    return false;
}

void hook_producer_cache_publish_exact(
    const hook_selector_identity_v1 &key,
    const hook_source_record &record) noexcept
{
    if (!key.valid || !record.valid)
        return;

    const auto set =
        hook_producer_cache_set(key);
    std::lock_guard<std::mutex> lock(
        g_hook_producer_cache_mutex[set]);

    const auto generation =
        g_hook_producer_cache_generation.load(
            std::memory_order_relaxed);
    const auto base =
        set * k_hook_producer_cache_ways;

    std::size_t target =
        k_hook_producer_cache_ways;
    for (std::size_t way = 0u;
         way < k_hook_producer_cache_ways;
         ++way) {
        const auto &entry =
            g_hook_producer_cache[base + way];
        if (!entry.valid ||
            entry.cache_generation != generation ||
            same_hook_selector_identity(
                entry.key,
                key)) {
            target = way;
            break;
        }
    }

    if (target ==
        k_hook_producer_cache_ways) {
        target =
            static_cast<std::size_t>(
                g_hook_producer_cache_victim[set]++ &
                static_cast<std::uint8_t>(
                    k_hook_producer_cache_ways - 1u));
    }

    auto &entry =
        g_hook_producer_cache[base + target];
    entry.key = key;
    entry.record = record;
    entry.cache_generation = generation;
    entry.valid = true;

    g_hook_producer_cache_publish.fetch_add(
        1u,
        std::memory_order_relaxed);
}

std::atomic_bool g_hook_restore_failed{false};

enum hook_decode_stage : std::uint32_t {
    hook_decode_none = 0u,
    hook_decode_source_invalid = 1u,
    hook_decode_base_invalid = 2u,
    hook_decode_selector_invalid = 3u,
    hook_decode_header_invalid = 4u,
    hook_decode_row_read_invalid = 5u,
    hook_decode_bank_unknown = 6u,
    hook_decode_row_unknown = 7u,
    hook_decode_nonfinite = 8u,
    hook_decode_ok = 9u
};

void record_hook_decode(
    std::uint32_t stage,
    std::uint32_t version,
    std::uint32_t count,
    std::uint32_t index,
    std::uint32_t row_id,
    std::uint64_t signature) noexcept
{
    g_hook_decode_stage.store(stage,std::memory_order_relaxed);
    g_hook_decode_version.store(version,std::memory_order_relaxed);
    g_hook_decode_count.store(count,std::memory_order_relaxed);
    g_hook_decode_index.store(index,std::memory_order_relaxed);
    g_hook_decode_row_id.store(row_id,std::memory_order_relaxed);
    g_hook_decode_signature.store(signature,std::memory_order_relaxed);
}

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

bool capture_hook_endpoint_identity(
    void *source,
    std::int16_t selector,
    const std::uint8_t *&base,
    std::uint16_t &count,
    std::uint32_t &row_id) noexcept
{
    base = nullptr;
    count = 0u;
    row_id = 0u;

    if (source == nullptr ||
        selector < 0 ||
        !safe_read(
            static_cast<const std::uint8_t *>(
                source) + 0x18u,
            base) ||
        base == nullptr)
        return false;

    std::uint16_t version = 0u;
    if (!safe_read(base + 8u,version) ||
        !safe_read(base + 10u,count) ||
        version != 4u ||
        count == 0u ||
        count > 256u)
        return false;

    std::uint32_t index = 0u;
    if (!retail_lightbank_record_index(
            selector,
            index) ||
        index >= count)
        return false;

    const auto *entry =
        base +
        0x30u +
        static_cast<std::size_t>(
            index) * 12u;
    return safe_read(
        entry,
        row_id);
}

bool capture_hook_selector_identity(
    void *source_a,
    std::int16_t selector_a,
    void *source_b,
    std::int16_t selector_b,
    float beta,
    hook_selector_identity_v1 &out) noexcept
{
    out = {};
    if (source_a == nullptr ||
        selector_a < 0 ||
        !std::isfinite(beta))
        return false;

    const float effective_beta =
        std::clamp(
            beta,
            0.0f,
            1.0f);
    std::uint32_t beta_bits = 0u;
    std::memcpy(
        &beta_bits,
        &effective_beta,
        sizeof(beta_bits));
    if (effective_beta == 0.0f)
        beta_bits = 0u;

    out.source_a = source_a;
    out.source_b = source_b;
    out.selector_a = selector_a;
    out.selector_b = selector_b;
    out.beta_bits = beta_bits;

    if (!capture_hook_endpoint_identity(
            source_a,
            selector_a,
            out.base_a,
            out.count_a,
            out.row_id_a))
        return false;

    if (beta_bits == 0u) {
        out.source_b = source_a;
        out.base_b = out.base_a;
        out.selector_b = selector_a;
        out.count_b = out.count_a;
        out.row_id_b = out.row_id_a;
        out.valid = true;
        return true;
    }

    if (source_b == nullptr ||
        selector_b < 0 ||
        !capture_hook_endpoint_identity(
            source_b,
            selector_b,
            out.base_b,
            out.count_b,
            out.row_id_b))
        return false;

    out.valid = true;
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

bool bank_layout_signature(
    const std::uint8_t *base,
    std::uint64_t &layout_signature) noexcept
{
    layout_signature = 0u;
    g_bank_signature_scan_count.fetch_add(
        1u,
        std::memory_order_relaxed);
    g_bank_layout_signature.store(
        0u,
        std::memory_order_relaxed);
    record_bank_signature_stage(
        1u,
        bank_signature_none,
        0u,
        0u,
        0u);

    if (base == nullptr ||
        !readable_range(
            base + 8u,
            sizeof(std::uint16_t) * 2u)) {
        record_bank_signature_stage(
            1u,
            bank_signature_header_range,
            0u,
            0u,
            0u);
        return false;
    }

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
        count > 256u) {
        record_bank_signature_stage(
            1u,
            bank_signature_header_semantic,
            0u,
            0u,
            0u);
        return false;
    }

    const std::size_t table_bytes =
        0x30u +
        static_cast<std::size_t>(
            count) * 12u;

    if (table_bytes < 8u ||
        !readable_range(
            base + 8u,
            table_bytes - 8u)) {
        record_bank_signature_stage(
            1u,
            bank_signature_table_range,
            0u,
            0u,
            0u);
        return false;
    }

    // Exact bank identity must not depend on the authored LightBank values
    // that this bridge intentionally replaces. PR180 runtime also proved that
    // a legal vanilla DSR LightBank can contain name_offset == 0, invalidating
    // the inherited V13 assumption that every row exposes a readable name.
    //
    // Hash only stable PARAM table structure: version, count and every
    // (row_id,row_offset,name_offset) tuple. The 20 homologous V13 donor banks
    // are joined to exact vanilla-DSR layout signatures by the generated
    // authority. Unknown layouts (including a99) fail open.
    std::uint64_t hash =
        0xcbf29ce484222325ULL;

    auto hash_u16 =
        [&](std::uint16_t value) noexcept {
            hash = fnv_byte(
                hash,
                static_cast<std::uint8_t>(value));
            hash = fnv_byte(
                hash,
                static_cast<std::uint8_t>(value >> 8u));
        };

    auto hash_u32 =
        [&](std::uint32_t value) noexcept {
            for (std::uint32_t shift = 0u;
                 shift < 32u;
                 shift += 8u)
                hash = fnv_byte(
                    hash,
                    static_cast<std::uint8_t>(
                        value >> shift));
        };

    hash_u16(version);
    hash_u16(count);

    for (std::uint32_t i = 0u;
         i < count;
         ++i) {
        const auto *entry =
            base +
            0x30u +
            static_cast<std::size_t>(i) *
                12u;

        std::uint32_t row_id = 0u;
        std::uint32_t row_offset = 0u;
        std::uint32_t name_offset = 0u;
        std::memcpy(
            &row_id,
            entry,
            sizeof(row_id));
        std::memcpy(
            &row_offset,
            entry + 4u,
            sizeof(row_offset));
        std::memcpy(
            &name_offset,
            entry + 8u,
            sizeof(name_offset));

        hash_u32(row_id);
        hash_u32(row_offset);
        hash_u32(name_offset);
    }

    layout_signature = hash;
    g_bank_layout_signature.store(
        hash,
        std::memory_order_relaxed);
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

    std::uint64_t layout = 0u;
    if (!bank_layout_signature(
            base,
            layout)) {
        cached.base = base;
        cached.count = count;
        cached.generation = generation;
        cached.signature = 0u;
        cached.bank = nullptr;
        cached.valid = true;
        return nullptr;
    }

    const auto *bank =
        pmetal_env_source_authority::
            find_bank_by_layout(
                layout,
                static_cast<std::uint32_t>(
                    count));

    if (bank == nullptr) {
        record_bank_signature_stage(
            1u,
            bank_signature_layout_unknown,
            static_cast<std::uint32_t>(count),
            0u,
            0u);
        cached.base = base;
        cached.count = count;
        cached.generation = generation;
        cached.signature = 0u;
        cached.bank = nullptr;
        cached.valid = true;
        return nullptr;
    }

    // Preserve the canonical V13 donor signature in downstream state and
    // telemetry. The new layout signature is identity machinery only.
    signature = bank->signature;

    cached.base = base;
    cached.count = count;
    cached.generation = generation;
    cached.signature = signature;
    cached.bank = bank;
    cached.valid = true;

    record_bank_signature_stage(
        1u,
        bank_signature_ok,
        static_cast<std::uint32_t>(count),
        0u,
        0u);
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

    std::uint32_t version_u32 = 0u;
    std::uint32_t count_u32 = 0u;
    std::uint32_t index = 0u;

    if (source == nullptr || selector < 0) {
        record_hook_decode(hook_decode_source_invalid,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    const std::uint8_t *base = nullptr;
    if (!safe_read(static_cast<const std::uint8_t *>(source) + 0x18u,base) || base == nullptr) {
        record_hook_decode(hook_decode_base_invalid,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    if (!retail_lightbank_record_index(selector,index)) {
        record_hook_decode(hook_decode_selector_invalid,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    std::uint16_t version = 0u;
    std::uint16_t count = 0u;
    if (!safe_read(base + 8u,version) || !safe_read(base + 10u,count)) {
        record_hook_decode(hook_decode_header_invalid,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    version_u32 = version;
    count_u32 = count;

    if (version != 4u || count == 0u || count > 256u || index >= count) {
        record_hook_decode(hook_decode_header_invalid,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    const auto *entry = base + 0x30u + static_cast<std::size_t>(index) * 12u;
    if (!safe_read(entry,row_id)) {
        record_hook_decode(hook_decode_row_read_invalid,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    if (endpoint_cache_lookup(source,base,count,index,row_id,out,signature)) {
        record_hook_decode(hook_decode_ok,version_u32,count_u32,index,row_id,signature);
        return true;
    }

    const auto *bank = resolve_bank(base,count,signature);
    if (bank == nullptr) {
        record_hook_decode(hook_decode_bank_unknown,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    const auto *row = pmetal_env_source_authority::find_row(*bank,row_id);
    if (row == nullptr) {
        record_hook_decode(hook_decode_row_unknown,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    const float scale = static_cast<float>(row->m) * 0.01f;
    out = {
        static_cast<float>(row->r) / 255.0f * scale,
        static_cast<float>(row->g) / 255.0f * scale,
        static_cast<float>(row->b) / 255.0f * scale,
        0.0f
    };

    if (!std::isfinite(out.x) || !std::isfinite(out.y) || !std::isfinite(out.z)) {
        record_hook_decode(hook_decode_nonfinite,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    endpoint_cache_publish(source,base,count,index,row_id,out,signature);
    record_hook_decode(hook_decode_ok,version_u32,count_u32,index,row_id,signature);
    return true;
}


bool write_source_hook_code(
    void *target,
    const void *bytes,
    std::size_t size) noexcept
{
    DWORD old = 0u;
    if (target == nullptr ||
        bytes == nullptr ||
        size == 0u ||
        !VirtualProtect(
            target,
            size,
            PAGE_EXECUTE_READWRITE,
            &old))
        return false;

    std::memcpy(
        target,
        bytes,
        size);
    const bool flushed =
        FlushInstructionCache(
            GetCurrentProcess(),
            target,
            size) != FALSE;

    DWORD ignored = 0u;
    VirtualProtect(
        target,
        size,
        old,
        &ignored);
    return flushed;
}

bool prepare_source_hook(
    source_hook &hook,
    std::uintptr_t base,
    std::uintptr_t rva,
    const std::array<std::uint8_t,14> &preimage,
    void *detour) noexcept
{
    auto *target =
        reinterpret_cast<std::uint8_t *>(
            base + rva);

    if (base == 0u ||
        detour == nullptr ||
        !readable_range(
            target,
            preimage.size()) ||
        std::memcmp(
            target,
            preimage.data(),
            preimage.size()) != 0)
        return false;

    auto *trampoline =
        static_cast<std::uint8_t *>(
            VirtualAlloc(
                nullptr,
                preimage.size() + 14u,
                MEM_COMMIT | MEM_RESERVE,
                PAGE_EXECUTE_READWRITE));
    if (trampoline == nullptr)
        return false;

    std::memcpy(
        trampoline,
        target,
        preimage.size());

    auto *tail =
        trampoline +
        preimage.size();
    tail[0] = 0xffu;
    tail[1] = 0x25u;
    std::uint32_t zero = 0u;
    std::memcpy(
        tail + 2u,
        &zero,
        sizeof(zero));
    const auto return_address =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(
                target + preimage.size()));
    std::memcpy(
        tail + 6u,
        &return_address,
        sizeof(return_address));

    if (FlushInstructionCache(
            GetCurrentProcess(),
            trampoline,
            preimage.size() + 14u) == FALSE) {
        VirtualFree(
            trampoline,
            0u,
            MEM_RELEASE);
        return false;
    }

    hook.target = target;
    hook.trampoline = trampoline;
    hook.detour = detour;
    hook.stolen = preimage.size();
    hook.original = preimage;
    hook.patched = false;
    return true;
}

bool arm_source_hook(
    source_hook &hook) noexcept
{
    if (hook.target == nullptr ||
        hook.trampoline == nullptr ||
        hook.detour == nullptr ||
        hook.stolen != 14u)
        return false;

    std::array<std::uint8_t,14> patch{};
    patch[0] = 0xffu;
    patch[1] = 0x25u;
    std::uint32_t zero = 0u;
    std::memcpy(
        patch.data() + 2u,
        &zero,
        sizeof(zero));
    const auto destination =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(
                hook.detour));
    std::memcpy(
        patch.data() + 6u,
        &destination,
        sizeof(destination));

    if (!write_source_hook_code(
            hook.target,
            patch.data(),
            patch.size()))
        return false;

    hook.patched = true;
    return true;
}

bool restore_source_hook(
    source_hook &hook) noexcept
{
    if (hook.patched) {
        if (hook.target == nullptr ||
            !write_source_hook_code(
                hook.target,
                hook.original.data(),
                hook.stolen) ||
            std::memcmp(
                hook.target,
                hook.original.data(),
                hook.stolen) != 0)
            return false;
        hook.patched = false;
    }

    if (hook.trampoline != nullptr &&
        VirtualFree(
            hook.trampoline,
            0u,
            MEM_RELEASE) == FALSE)
        return false;

    hook = {};
    return true;
}

bool same_hook_source_payload(
    const pmetal_envspec_source &a,
    const pmetal_envspec_source &b) noexcept
{
    return
        a.a == b.a &&
        a.b == b.b &&
        a.envdiffuse_a == b.envdiffuse_a &&
        a.envdiffuse_b == b.envdiffuse_b &&
        a.envdiffuse_linear_valid ==
            b.envdiffuse_linear_valid &&
        a.beta == b.beta &&
        a.bank_signature_a == b.bank_signature_a &&
        a.bank_signature_b == b.bank_signature_b &&
        a.row_id_a == b.row_id_a &&
        a.row_id_b == b.row_id_b;
}

void publish_hook_source(
    const f4 &a,
    const f4 &b,
    const f4 &envdiffuse_a,
    const f4 &envdiffuse_b,
    float beta,
    std::uint64_t bank_a,
    std::uint64_t bank_b,
    std::uint32_t row_a,
    std::uint32_t row_b,
    void *source_a,
    std::int16_t selector_a,
    void *source_b,
    std::int16_t selector_b) noexcept
{
    if (!std::isfinite(beta))
        return;

    pmetal_envspec_source next{};
    next.a = {a.x,a.y,a.z};
    next.b = {b.x,b.y,b.z};
    next.envdiffuse_a = {
        envdiffuse_a.x,
        envdiffuse_a.y,
        envdiffuse_a.z
    };
    next.envdiffuse_b = {
        envdiffuse_b.x,
        envdiffuse_b.y,
        envdiffuse_b.z
    };
    next.envdiffuse_linear_valid = true;
    next.beta =
        std::clamp(
            beta,
            0.0f,
            1.0f);
    next.bank_signature_a = bank_a;
    next.bank_signature_b = bank_b;
    next.row_id_a = row_a;
    next.row_id_b = row_b;

    const auto serial =
        g_hook_source_serial.fetch_add(
            1u,
            std::memory_order_relaxed) +
        1u;
    next.serial = serial;

    hook_selector_identity_v1
        selector_identity{};
    (void)capture_hook_selector_identity(
        source_a,
        selector_a,
        source_b,
        selector_b,
        next.beta,
        selector_identity);

    // R43: producer-local reuse is keyed by the exact live LightBank
    // identity, not by a process-global "latest source" version. Independent
    // sources may interleave without invalidating each other.
    if (g_hook_source_tls.valid &&
        g_hook_selector_identity_tls.valid &&
        same_hook_selector_identity(
            g_hook_selector_identity_tls,
            selector_identity) &&
        same_hook_source_payload(
            g_hook_source_tls.source,
            next)) {
        next.generation =
            g_hook_source_tls.source.generation;
        g_hook_source_tls = {
            next,
            serial,
            g_hook_source_tls.semantic_version,
            true
        };
        g_hook_selector_identity_tls =
            selector_identity;
        hook_producer_cache_publish_exact(
            selector_identity,
            g_hook_source_tls);
        #if !defined(DSRRL_RELEASE_CLEANUP)
g_hook_publish.fetch_add(
            1u,
            std::memory_order_relaxed);
#endif
        return;
    }

    hook_source_record cached_record{};
    if (hook_producer_cache_lookup(
            selector_identity,
            cached_record,
            &next)) {
        next.generation =
            cached_record.source.generation;
        g_hook_source_tls = {
            next,
            serial,
            cached_record.semantic_version,
            true
        };
        g_hook_selector_identity_tls =
            selector_identity;
        hook_producer_cache_publish_exact(
            selector_identity,
            g_hook_source_tls);
        #if !defined(DSRRL_RELEASE_CLEANUP)
g_hook_publish.fetch_add(
            1u,
            std::memory_order_relaxed);
#endif

        static std::atomic_bool
            producer_cross_thread_hit_logged{
                false};
        if (!producer_cross_thread_hit_logged.exchange(
                true,
                std::memory_order_relaxed))
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL PMETAL R43] producer_per_key_cache_hit=1 exact_key=SOURCE_BASE_COUNT_ROW_SELECTOR_BETA exact_payload=ON global_publish_mutex=SKIPPED");

        return;
    }

    std::uint64_t resolved_version = 0u;
    {
        std::lock_guard<std::mutex> lock(
            g_hook_source_mutex);

        const bool unchanged =
            g_hook_source_global.valid &&
            same_hook_source_payload(
                g_hook_source_global.source,
                next);

        if (unchanged) {
            next.generation =
                g_hook_source_global.source.generation;
            resolved_version =
                g_hook_source_global.semantic_version;
        } else {
            next.generation =
                g_hook_source_generation.fetch_add(
                    1u,
                    std::memory_order_relaxed) +
                1u;
            resolved_version =
                g_hook_source_semantic_version.fetch_add(
                    1u,
                    std::memory_order_acq_rel) +
                1u;
        }

        g_hook_source_global = {
            next,
            serial,
            resolved_version,
            true
        };
    }

    // TLS gets the same semantic generation/version decided against the
    // authoritative global record. serial remains event identity only.
    g_hook_source_tls = {
        next,
        serial,
        resolved_version,
        true
    };
    g_hook_selector_identity_tls =
        selector_identity;
    hook_producer_cache_publish_exact(
        selector_identity,
        g_hook_source_tls);

    #if !defined(DSRRL_RELEASE_CLEANUP)
g_hook_publish.fetch_add(
        1u,
        std::memory_order_relaxed);
#endif
}

bool latest_hook_source_exact_selector(
    void *source_a,
    std::int16_t selector_a,
    void *source_b,
    std::int16_t selector_b,
    float beta,
    pmetal_envspec_source &out) noexcept
{
    out = {};
    if (source_a == nullptr ||
        selector_a < 0 ||
        !std::isfinite(beta))
        return false;

    hook_selector_identity_v1 query{};
    if (!capture_hook_selector_identity(
            source_a,
            selector_a,
            source_b,
            selector_b,
            beta,
            query))
        return false;

    if (g_hook_source_tls.valid &&
        g_hook_selector_identity_tls.valid &&
        same_hook_selector_identity(
            g_hook_selector_identity_tls,
            query)) {
        out = g_hook_source_tls.source;
        #if !defined(DSRRL_RELEASE_CLEANUP)
g_hook_consume.fetch_add(
            1u,
            std::memory_order_relaxed);
#endif
        return true;
    }

    hook_source_record cached{};
    if (!hook_producer_cache_lookup(
            query,
            cached))
        return false;

    out = cached.source;
    g_hook_source_tls = cached;
    g_hook_selector_identity_tls = query;
    #if !defined(DSRRL_RELEASE_CLEANUP)
g_hook_consume.fetch_add(
        1u,
        std::memory_order_relaxed);
#endif

    static std::atomic_bool
        selector_cross_thread_hit_logged{
            false};
    if (!selector_cross_thread_hit_logged.exchange(
            true,
            std::memory_order_relaxed))
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL PMETAL R43] selector_per_key_cache_hit=1 exact_key=SOURCE_BASE_COUNT_ROW_SELECTOR_BETA global_semantic_version=IGNORED donor_redecode=OFF lock_wait=OFF");

    return true;
}

bool latest_hook_source(
    pmetal_envspec_source &out) noexcept
{
    out = {};

    if (g_hook_source_tls.valid) {
        out =
            g_hook_source_tls.source;
#if defined(DSRRL_PMETAL_SPEC_CUT_TRACE)
        out.diagnostic_origin = 2u;
#endif
        #if !defined(DSRRL_RELEASE_CLEANUP)
g_hook_consume.fetch_add(
            1u,
            std::memory_order_relaxed);
#endif
        return true;
    }

    std::lock_guard<std::mutex> lock(
        g_hook_source_mutex);
    if (!g_hook_source_global.valid ||
        g_hook_source_global.serial == 0u)
        return false;

    out =
        g_hook_source_global.source;
#if defined(DSRRL_PMETAL_SPEC_CUT_TRACE)
    out.diagnostic_origin = 3u;
#endif
    #if !defined(DSRRL_RELEASE_CLEANUP)
g_hook_consume.fetch_add(
        1u,
        std::memory_order_relaxed);
#endif
    return true;
}

void clear_hook_source() noexcept
{
    g_hook_source_tls = {};
    g_hook_selector_identity_tls = {};
    g_hook_producer_cache_generation.fetch_add(
        1u,
        std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(
        g_hook_source_mutex);
    g_hook_source_global = {};
}

void __fastcall envspec_single_hook_entry(
    void *source,
    float *host_out,
    int selector) noexcept
{
    #if !defined(DSRRL_RELEASE_CLEANUP)
g_hook_single_seen.fetch_add(
        1u,
        std::memory_order_relaxed);
#endif

    f4 donor{};
    std::uint64_t bank = 0u;
    std::uint32_t row = 0u;
    const bool donor_ok =
        read_exact_source(
            source,
            selector,
            donor,
            bank,
            row);

    if (g_envspec_single_original != nullptr)
        g_envspec_single_original(
            source,
            host_out,
            selector);

    f4 envdiffuse_a{};
    f4 envdiffuse_b{};
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    const bool envdiffuse_ok =
        donor_ok &&
        exact_envdiffuse_endpoint(
            bank,
            row,
            envdiffuse_a);
    envdiffuse_b = envdiffuse_a;
#else
    const bool envdiffuse_ok =
        capture_linear_envdiffuse(
            host_out,
            envdiffuse_a,
            envdiffuse_b);
#endif

    if (donor_ok && envdiffuse_ok)
        publish_hook_source(
            donor,
            donor,
            envdiffuse_a,
            envdiffuse_b,
            0.0f,
            bank,
            bank,
            row,
            row,
            source,
            static_cast<std::int16_t>(
                selector),
            source,
            static_cast<std::int16_t>(
                selector));
}

void __fastcall envspec_blend_hook_entry(
    float *host_out,
    void *source_a,
    int selector_a,
    void *source_b,
    int selector_b,
    float beta) noexcept
{
    #if !defined(DSRRL_RELEASE_CLEANUP)
g_hook_blend_seen.fetch_add(
        1u,
        std::memory_order_relaxed);
#endif

    const auto endpoints =
        pmetal_selector_policy::select(
            static_cast<std::int16_t>(
                selector_a),
            static_cast<std::int16_t>(
                selector_b),
            beta);

    f4 a{};
    f4 b{};
    std::uint64_t bank_a = 0u;
    std::uint64_t bank_b = 0u;
    std::uint32_t row_a = 0u;
    std::uint32_t row_b = 0u;

    bool donor_ok = false;
    if (endpoints.valid) {
        donor_ok =
            read_exact_source(
                source_a,
                endpoints.a,
                a,
                bank_a,
                row_a);

        if (donor_ok) {
            if (endpoints.beta == 0.0f ||
                endpoints.a == endpoints.b) {
                b = a;
                bank_b = bank_a;
                row_b = row_a;
            } else {
                donor_ok =
                    read_exact_source(
                        source_b,
                        endpoints.b,
                        b,
                        bank_b,
                        row_b);
            }
        }
    }

    if (g_envspec_blend_original != nullptr)
        g_envspec_blend_original(
            host_out,
            source_a,
            selector_a,
            source_b,
            selector_b,
            beta);

    f4 envdiffuse_a{};
    f4 envdiffuse_b{};
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    const bool envdiffuse_ok =
        donor_ok &&
        exact_envdiffuse_endpoint(
            bank_a,
            row_a,
            envdiffuse_a) &&
        exact_envdiffuse_endpoint(
            bank_b,
            row_b,
            envdiffuse_b);
#else
    const bool envdiffuse_ok =
        capture_linear_envdiffuse(
            host_out,
            envdiffuse_a,
            envdiffuse_b);
#endif

    if (donor_ok && envdiffuse_ok)
        publish_hook_source(
            a,
            b,
            envdiffuse_a,
            envdiffuse_b,
            endpoints.beta,
            bank_a,
            bank_b,
            row_a,
            row_b,
            source_a,
            endpoints.a,
            source_b,
            endpoints.b);
}

bool install_envspec_source_hooks(
    std::uintptr_t base) noexcept
{
    if (g_envspec_single_hook.patched ||
        g_envspec_blend_hook.patched)
        return false;

    if (!prepare_source_hook(
            g_envspec_single_hook,
            base,
            k_envspec_single_rva,
            k_envspec_single_preimage,
            reinterpret_cast<void *>(
                &envspec_single_hook_entry)))
        return false;

    g_envspec_single_original =
        reinterpret_cast<envspec_single_fn>(
            g_envspec_single_hook.trampoline);

    if (!prepare_source_hook(
            g_envspec_blend_hook,
            base,
            k_envspec_blend_rva,
            k_envspec_blend_preimage,
            reinterpret_cast<void *>(
                &envspec_blend_hook_entry))) {
        (void)restore_source_hook(
            g_envspec_single_hook);
        g_envspec_single_original = nullptr;
        return false;
    }

    g_envspec_blend_original =
        reinterpret_cast<envspec_blend_fn>(
            g_envspec_blend_hook.trampoline);

    if (!arm_source_hook(
            g_envspec_single_hook) ||
        !arm_source_hook(
            g_envspec_blend_hook)) {
        const bool blend_restored =
            restore_source_hook(
                g_envspec_blend_hook);
        const bool single_restored =
            restore_source_hook(
                g_envspec_single_hook);
        g_hook_restore_failed.store(
            !(blend_restored &&
              single_restored),
            std::memory_order_relaxed);
        g_envspec_single_original = nullptr;
        g_envspec_blend_original = nullptr;
        return false;
    }

    return true;
}

bool uninstall_envspec_source_hooks() noexcept
{
    const bool blend_ok =
        restore_source_hook(
            g_envspec_blend_hook);
    const bool single_ok =
        restore_source_hook(
            g_envspec_single_hook);

    if (!(blend_ok &&
          single_ok)) {
        g_hook_restore_failed.store(
            true,
            std::memory_order_relaxed);
        return false;
    }

    g_envspec_single_original = nullptr;
    g_envspec_blend_original = nullptr;
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
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    if (!load_exact_envdiffuse_donor()) {
        reshade::log::message(
            reshade::log::level::warning,
            "DSRRL Full PTDE HemEnv: exact PTDE EnvDiffuse LightBank donor sidecar unavailable or invalid; source carrier fails open.");
        return false;
    }
    reshade::log::message(
        reshade::log::level::info,
        "[DSRRL PMETAL FULL PTDE HEMENV] exact PTDE EnvDiffuse LightBank donor admitted.");
#endif
    pmetal_env_source_cache_invalidate();
    clear_hook_source();
    g_hook_restore_failed.store(
        false,
        std::memory_order_relaxed);

    g_source_base =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));

    g_selector_epoch.fetch_add(
        1u,
        std::memory_order_relaxed);

    if (g_source_base == 0u ||
        !install_envspec_source_hooks(
            g_source_base)) {
        g_selector_enabled.store(
            false,
            std::memory_order_release);
        return false;
    }

    g_selector_enabled.store(
        true,
        std::memory_order_release);

    static std::atomic_bool
        r41_cache_scope_logged{false};
    if (!r41_cache_scope_logged.exchange(
            true,
            std::memory_order_relaxed))
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL PMETAL R41] flver_lifecycle_cache_flush=OFF source_cache_generation=SOURCE_RUNTIME_RESET_ONLY endpoint_cache=EXACT_SOURCE_BASE_COUNT_INDEX_ROW bank_cache=BASE_COUNT_LAYOUT region_cache=VM_WINDOW selector_shadow=EXACT_TLS_SOURCE_PTR_SELECTOR_BETA selector_source=ON envspec=ON material_response=ON");

    static std::atomic_bool
        r42_cross_thread_cache_logged{false};
    if (!r42_cross_thread_cache_logged.exchange(
            true,
            std::memory_order_relaxed))
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL PMETAL R42] producer_cache=SET64_WAY2 cross_thread=ON exact_key=SOURCE_PTR_SELECTOR_BETA semantic_version_gate=CURRENT selector_try_lock=ON producer_payload_reuse=ON donor_redecode_fallback=ON islands_preserved=ON");

    static std::atomic_bool
        r43_per_key_freshness_logged{false};
    if (!r43_per_key_freshness_logged.exchange(
            true,
            std::memory_order_relaxed))
        reshade::log::message(
            reshade::log::level::info,
            "[DSRRL PMETAL R43] producer_cache=SET256_WAY2 freshness=SOURCE_BASE_COUNT_ROW_SELECTOR_BETA global_semantic_version_gate=OFF selector_try_lock=ON producer_payload_exact=ON fallback_full_decode=ON islands_preserved=ON");

    return true;
}

void pmetal_env_source_runtime::uninstall() noexcept
{
    g_selector_enabled.store(
        false,
        std::memory_order_release);
    g_selector_epoch.fetch_add(
        1u,
        std::memory_order_relaxed);

    if (!uninstall_envspec_source_hooks())
        g_hook_restore_failed.store(
            true,
            std::memory_order_relaxed);

    pmetal_env_source_cache_invalidate();
    pmetal_env_source_selector_clear();
    clear_hook_source();
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

    const auto epoch =
        g_selector_epoch.load(
            std::memory_order_relaxed);

    // This exact material now owns the next synchronized source publication.
    // Invalidate any older selector fallback before downstream decode.
    pmetal_producer_state_begin(
        material,
        epoch);

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

    // The parent return address is an implementation-specific call-stack
    // fingerprint, not an intrinsic LightBank semantic input. Previously it
    // discarded virtually every exact owner P_Metal selector while the retail
    // LightBank packer was demonstrably active (owner R44 runtime).
    const bool parent_verified =
        safe_read(
            static_cast<const std::uint8_t *>(
                selector_stack) +
                parent_offset,
            parent_return) &&
        parent_return ==
            g_source_base +
                k_parent_return;
#if !defined(DSRRL_PMETAL_PARENT_SEMANTIC_JOIN_DIAG)
    if (!parent_verified)
        return;
#endif
    // In the diagnostic alternate branch every subsequent structural gate
    // still applies: RVA allowlist, exact FLVER/MTD/slot, descriptor_owner==
    // native owner, character<=1, valid endpoints, manager table, exact
    // source/selector, bank signature, row and PTDE sidecar lookup. No
    // unqualified latest hook snapshot becomes authorized.
    if (parent_verified && telemetry::effect_enabled())
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

    void *source_a =
        pmetal_selector_policy::source(
            endpoints.a,
            character != 0u,
            lookup);
    void *source_b =
        source_a;
    if (lookup_valid &&
        endpoints.beta != 0.0f &&
        endpoints.a != endpoints.b)
        source_b =
            pmetal_selector_policy::source(
                endpoints.b,
                character != 0u,
                lookup);

    if (!lookup_valid ||
        source_a == nullptr ||
        source_b == nullptr)
        return;

    pmetal_envspec_source next{};
#if defined(DSRRL_PMETAL_SPEC_CUT_TRACE) && defined(DSRRL_PMETAL_PARENT_SEMANTIC_JOIN_DIAG)
    // 4=selector-sourced LightBank with full owner/endpoint/sidecar
    // authentication; only the legacy parent stack fingerprint differs.
    next.diagnostic_origin = parent_verified ? 1u : 4u;
#endif

    // R41 producer-driven shadow join. The native EnvSpec single/blend packer
    // already resolved this exact LightBank source before the material selector
    // consumes it. Reuse only a same-thread snapshot with exact source pointer,
    // selected endpoint(s), and bit-identical effective beta. Any mismatch
    // falls through to the complete selector-side donor reconstruction below.
    if (latest_hook_source_exact_selector(
            source_a,
            endpoints.a,
            source_b,
            endpoints.b,
            endpoints.beta,
            next)) {
#if defined(DSRRL_PMETAL_SPEC_CUT_TRACE) && defined(DSRRL_PMETAL_PARENT_SEMANTIC_JOIN_DIAG)
        next.diagnostic_origin = parent_verified ? 1u : 4u;
#endif
        next.serial = epoch;
        pmetal_producer_state_publish(
            material,
            next,
            epoch);

        static std::atomic_bool
            selector_shadow_hit_logged{false};
        if (!selector_shadow_hit_logged.exchange(
                true,
                std::memory_order_relaxed))
            reshade::log::message(
                reshade::log::level::info,
                "[DSRRL PMETAL R41] selector_source_shadow_hit=1 exact_source_ptr=ON exact_selector_beta=ON donor_redecode=OFF fail_open_fallback=ON");

        if (telemetry::effect_enabled()) {
            g_source_a_decode_ok.store(
                true,
                std::memory_order_relaxed);
            g_source_b_decode_ok.store(
                true,
                std::memory_order_relaxed);
            g_last_publish_tid.store(
                static_cast<std::uint32_t>(
                    GetCurrentThreadId()),
                std::memory_order_relaxed);
        }

        telemetry::hot_count(
            g_publish);
        telemetry::hot_count(
            endpoints.beta == 0.0f
                ? g_steady_seen
                : g_blend_seen);
        return;
    }

    auto decode =
        [&](void *source,
            std::int16_t selector,
            f4 &value,
            std::uint64_t &bank,
            std::uint32_t &row) noexcept {
            return
                source != nullptr &&
                read_exact_source(
                    source,
                    selector,
                    value,
                    bank,
                    row);
        };

    f4 a{};
    f4 b{};

    if (!decode(
            source_a,
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
                   source_b,
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

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_DIAG)
    f4 envdiffuse_a{};
    f4 envdiffuse_b{};
    if (!exact_envdiffuse_endpoint(
            next.bank_signature_a,
            next.row_id_a,
            envdiffuse_a) ||
        !exact_envdiffuse_endpoint(
            next.bank_signature_b,
            next.row_id_b,
            envdiffuse_b)) {
        telemetry::hot_count(
            g_decode_fail);
        return;
    }
    next.envdiffuse_a = {
        envdiffuse_a.x,
        envdiffuse_a.y,
        envdiffuse_a.z
    };
    next.envdiffuse_b = {
        envdiffuse_b.x,
        envdiffuse_b.y,
        envdiffuse_b.z
    };
    next.envdiffuse_linear_valid = true;
#endif

    next.serial = epoch;

    pmetal_producer_state_publish(
        material,
        next,
        epoch);

#if defined(DSRRL_PMETAL_PARENT_SEMANTIC_JOIN_DIAG)
    if (!parent_verified) {
        static std::atomic_bool semantic_join_logged{false};
        if (!semantic_join_logged.exchange(
                true,
                std::memory_order_relaxed)) {
            char line[384]{};
            std::snprintf(
                line,sizeof(line),
                "[DSRRL PMETAL PARENT SEMANTIC JOIN] exact_owner=1 exact_mtd=1 descriptor_owner=1 endpoint=1 manager=1 bank_row=1 envdiffuse_sidecar=1 parent_return_mismatch=1 rva=%llx flver_sha0=%02x%02x%02x%02x slot=%u row=%u bank=%016llx",
                static_cast<unsigned long long>(rva),
                static_cast<unsigned>(material.flver_sha256[0]),
                static_cast<unsigned>(material.flver_sha256[1]),
                static_cast<unsigned>(material.flver_sha256[2]),
                static_cast<unsigned>(material.flver_sha256[3]),
                static_cast<unsigned>(material.material_slot),
                static_cast<unsigned>(next.row_id_a),
                static_cast<unsigned long long>(
                    next.bank_signature_a));
            reshade::log::message(
                reshade::log::level::info,
                line);
        }
    }
#endif
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
            material)) {
        telemetry::hot_count(
            g_consumer_fail);
        return false;
    }

    if (pmetal_producer_state_latest(
            material,
            epoch,
            out)) {
#if defined(DSRRL_PMETAL_SPEC_CUT_TRACE)
        if (out.diagnostic_origin != 4u)
            out.diagnostic_origin = 1u;
#endif
        telemetry::hot_count(
            g_consumer_ok);
        return true;
    }

    // R44 keyed-rendezvous: producer lookup alone is material authority.
    // The last same-thread/global packer snapshot is NOT tied to this exact
    // FLVER/slot. Camera-driven draw ordering changed its LightBank row while
    // the stationary owner's probe/SpecRGB remained constant in owner logs.
    // Do not put that value into the PTDE island. The enlarged/guarded
    // producer-state cache now retains independently verified material
    // producers across unrelated selector attempts. If this precise material
    // never published one, leave the entire P_Metal draw stock DSR.
    telemetry::hot_count(
        g_consumer_fail);
    return false;
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
    out.hook_single_seen =
        g_hook_single_seen.load(
            std::memory_order_relaxed);
    out.hook_blend_seen =
        g_hook_blend_seen.load(
            std::memory_order_relaxed);
    out.hook_publish =
        g_hook_publish.load(
            std::memory_order_relaxed);
    out.hook_consume =
        g_hook_consume.load(
            std::memory_order_relaxed);
    out.hook_decode_stage = g_hook_decode_stage.load(std::memory_order_relaxed);
    out.hook_decode_version = g_hook_decode_version.load(std::memory_order_relaxed);
    out.hook_decode_count = g_hook_decode_count.load(std::memory_order_relaxed);
    out.hook_decode_index = g_hook_decode_index.load(std::memory_order_relaxed);
    out.hook_decode_row_id = g_hook_decode_row_id.load(std::memory_order_relaxed);
    out.hook_decode_signature = g_hook_decode_signature.load(std::memory_order_relaxed);
    out.bank_signature_scan_count =
        g_bank_signature_scan_count.load(std::memory_order_relaxed);
    out.bank_signature_attempt =
        g_bank_signature_attempt.load(std::memory_order_relaxed);
    out.bank_signature_stage =
        g_bank_signature_stage.load(std::memory_order_relaxed);
    out.bank_signature_entry =
        g_bank_signature_entry.load(std::memory_order_relaxed);
    out.bank_signature_name_offset =
        g_bank_signature_name_offset.load(std::memory_order_relaxed);
    out.bank_signature_consumed =
        g_bank_signature_consumed.load(std::memory_order_relaxed);
    out.bank_layout_signature =
        g_bank_layout_signature.load(std::memory_order_relaxed);

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
    out.hook_single_armed =
        g_envspec_single_hook.patched;
    out.hook_blend_armed =
        g_envspec_blend_hook.patched;
    out.restore_failed =
        g_hook_restore_failed.load(
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
    clear_hook_source();

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
    g_hook_single_seen.store(
        0u,
        std::memory_order_relaxed);
    g_hook_blend_seen.store(
        0u,
        std::memory_order_relaxed);
    g_hook_publish.store(
        0u,
        std::memory_order_relaxed);
    g_hook_consume.store(
        0u,
        std::memory_order_relaxed);
    g_hook_source_serial.store(
        0u,
        std::memory_order_relaxed);
    g_hook_source_generation.store(
        0u,
        std::memory_order_relaxed);
    g_hook_source_semantic_version.store(
        0u,
        std::memory_order_relaxed);
    g_hook_decode_stage.store(0u,std::memory_order_relaxed);
    g_hook_decode_version.store(0u,std::memory_order_relaxed);
    g_hook_decode_count.store(0u,std::memory_order_relaxed);
    g_hook_decode_index.store(0u,std::memory_order_relaxed);
    g_hook_decode_row_id.store(0u,std::memory_order_relaxed);
    g_hook_decode_signature.store(0u,std::memory_order_relaxed);
    g_bank_signature_scan_count.store(0u,std::memory_order_relaxed);
    g_bank_signature_attempt.store(0u,std::memory_order_relaxed);
    g_bank_signature_stage.store(0u,std::memory_order_relaxed);
    g_bank_signature_entry.store(0u,std::memory_order_relaxed);
    g_bank_signature_name_offset.store(0u,std::memory_order_relaxed);
    g_bank_signature_consumed.store(0u,std::memory_order_relaxed);
    g_bank_layout_signature.store(0u,std::memory_order_relaxed);
    g_hook_restore_failed.store(
        false,
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
