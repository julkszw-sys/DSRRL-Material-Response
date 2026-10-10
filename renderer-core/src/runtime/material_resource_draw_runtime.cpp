#include "dsrrl/runtime/material_resource_draw_runtime.hpp"
#include "dsrrl/runtime/texture_identity_transport.hpp"
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
#include "dsrrl/runtime/spc25_physical_t1_probe.hpp"
#endif
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include "dsrrl/runtime/stutter_profiler.hpp"
#include "dsrrl/runtime/resource_view_epoch.hpp"
#include "dsrrl/runtime/companion_tls_way_selector.hpp"
#include "dsrrl/operators/resource_bridges/spec_rgb_bridge.hpp"
#include "dsrrl/operators/resource_bridges/fixed_pointlight_spec_rgb_bridge.hpp"
#include "dsrrl/operators/resource_bridges/diffuse_bridge.hpp"
#include "dsrrl/operators/resource_bridges/normal_bridge.hpp"
#include "dsrrl/runtime/generated_spec_routes_v12.hpp"
#include "dsrrl/runtime/generated_diffuse_routes_v12.hpp"
#include "dsrrl/runtime/generated_normal_routes_v12.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d11.h>

#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
// WKPDID_D3DDebugObjectName is declared by the Windows SDK in
// d3dcommon.h and exported as an external GUID from dxguid.lib.
#pragma comment(lib, "dxguid.lib")
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dsrrl::runtime {
namespace {

enum class asset_class : std::uint8_t {
    specular = 0,
    diffuse,
    normal
};

enum class load_status : std::uint8_t {
    missing = 0,
    ready,
    unsupported,
    create_failed,
    hash_mismatch
};

struct companion_set {
    ID3D11Device *device = nullptr;
    std::uint64_t logical_hash = 0;
    ID3D11ShaderResourceView *specular = nullptr;
    ID3D11ShaderResourceView *diffuse = nullptr;
    ID3D11ShaderResourceView *normal = nullptr;
};

struct load_result {
    ID3D11ShaderResourceView *view = nullptr;
    load_status status = load_status::missing;
};

#pragma pack(push, 1)
struct dds_pixel_format {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t fourcc;
    std::uint32_t rgb_bit_count;
    std::uint32_t r_mask;
    std::uint32_t g_mask;
    std::uint32_t b_mask;
    std::uint32_t a_mask;
};

struct dds_header {
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t height;
    std::uint32_t width;
    std::uint32_t pitch_or_linear_size;
    std::uint32_t depth;
    std::uint32_t mip_count;
    std::uint32_t reserved1[11];
    dds_pixel_format pixel_format;
    std::uint32_t caps;
    std::uint32_t caps2;
    std::uint32_t caps3;
    std::uint32_t caps4;
    std::uint32_t reserved2;
};

struct dds_header_dx10 {
    std::uint32_t dxgi_format;
    std::uint32_t resource_dimension;
    std::uint32_t misc_flag;
    std::uint32_t array_size;
    std::uint32_t misc_flags2;
};
#pragma pack(pop)

static_assert(sizeof(dds_pixel_format) == 32u);
static_assert(sizeof(dds_header) == 124u);
static_assert(sizeof(dds_header_dx10) == 20u);

constexpr std::uint32_t k_dds_magic = 0x20534444u;
constexpr std::uint32_t k_fourcc_dxt1 = 0x31545844u;
constexpr std::uint32_t k_fourcc_dxt3 = 0x33545844u;
constexpr std::uint32_t k_fourcc_dxt5 = 0x35545844u;
constexpr std::uint32_t k_fourcc_dx10 = 0x30315844u;
constexpr std::uint32_t k_resource_dimension_texture2d = 3u;
constexpr std::uint32_t k_misc_texturecube = 0x4u;
constexpr std::uint32_t k_caps2_cubemap = 0x200u;

core::renderer_core *g_core = nullptr;
std::mutex g_mutex;
std::unordered_map<std::uint64_t, companion_set> g_cache;
// A live native SRV handle observed under more than one exact logical texture
// identity is not resolvable at draw time. Keep the handle quarantined until
// its resource-view lifetime ends instead of allowing last-writer-wins identity.
std::unordered_map<std::uint64_t, ID3D11Device *> g_ambiguous_view_device;

#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
// Experimental, exact-name-only resource->SRV handoff. The retail texture
// name is already validated by the existing inlined name hook. A resource
// creation can occur within the naming interval while its SRV is created
// after the clear hook. Never infer identity from dimensions/format/MTD.
struct attested_resource_name {
    ID3D11Device *device = nullptr; // borrowed; cleared on destroy_device
    std::wstring logical_name{};
    std::uint64_t hash = 0u;
};
std::unordered_map<std::uint64_t,attested_resource_name>
    g_attested_names_by_resource;
std::unordered_map<std::uint64_t,ID3D11Device *>
    g_ambiguous_named_resource_device;
std::atomic_bool g_attested_resource_view_join_logged{false};
std::atomic_bool g_early_view_join_logged{false};
std::atomic_bool g_native_debug_name_join_logged{false};
std::atomic_bool g_late_native_t1_recovered_logged{false};
std::atomic<std::uint64_t> g_name_exact_init_resource{0u};
std::atomic<std::uint64_t> g_name_exact_create_view{0u};
std::atomic<std::uint64_t> g_name_exact_init_view{0u};
std::atomic_bool g_name_producer_cut_logged{false};

std::atomic<std::uint32_t> g_late_native_t1_stage_mask{0u};
void log_late_native_stage_once(
    std::uint32_t ordinal, const char *stage) noexcept
{
    if (ordinal >= 30u || stage == nullptr)
        return;
    const auto bit = std::uint32_t{1u} << ordinal;
    if ((g_late_native_t1_stage_mask.fetch_or(
            bit, std::memory_order_relaxed) & bit) != 0u)
        return;
    char msg[256]{};
    std::snprintf(msg, sizeof(msg),
        "[DSRRL SPC25 LATE T1] stage=%s scope=ALL_EXACT_SPC25 pixel=OPEN",
        stage);
    reshade::log::message(reshade::log::level::info, msg);
}

std::unordered_map<std::uint64_t,std::uint8_t> g_late_native_t1_attempts;



// ReShade create_resource_view is a synchronous, pre-D3D11 creation
// callback. A retail name may be live here even when it is gone by
// init_resource_view. Carry only a verified exact loader name across
// that specific pre/post callback pair, never across draws or frames.
struct early_view_name {
    ID3D11Device *device = nullptr; // borrowed within callback pair only
    std::uint64_t resource = 0u;
    reshade::api::resource_usage usage{};
    std::wstring name{};
    bool pending = false;
    bool ambiguous = false;
    void clear() noexcept {
        device = nullptr;
        resource = 0u;
        name.clear();
        pending = false;
        ambiguous = false;
    }
};
thread_local early_view_name g_early_view_name{};

#endif
#if !defined(DSRRL_RESOURCE_EPOCH_SHARD_SYNC)
std::atomic<std::uint64_t> g_cache_epoch{1u};
#endif
thread_local bool g_internal_create = false;

struct companion_tls_entry {
    std::uint64_t key = 0u;
    std::uint64_t epoch = 0u;
    std::uint64_t logical_hash = 0u;
    ID3D11ShaderResourceView *specular = nullptr;
    ID3D11ShaderResourceView *diffuse = nullptr;
    ID3D11ShaderResourceView *normal = nullptr;
    bool present = false;

    ~companion_tls_entry()
    {
        clear();
    }

    void clear() noexcept
    {
        if (specular != nullptr)
            specular->Release();
        if (diffuse != nullptr)
            diffuse->Release();
        if (normal != nullptr)
            normal->Release();

        key = 0u;
        epoch = 0u;
        logical_hash = 0u;
        specular = nullptr;
        diffuse = nullptr;
        normal = nullptr;
        present = false;
    }

    void assign_owned(
        std::uint64_t stock_key,
        std::uint64_t cache_epoch,
        std::uint64_t hash,
        ID3D11ShaderResourceView *spec,
        ID3D11ShaderResourceView *diff,
        ID3D11ShaderResourceView *norm,
        bool exists) noexcept
    {
        clear();
        key = stock_key;
        epoch = cache_epoch;
        logical_hash = hash;
        specular = spec;
        diffuse = diff;
        normal = norm;
        present = exists;
    }
};

constexpr std::size_t k_companion_tls_slots = 256u;
thread_local std::array<
    companion_tls_entry,
    k_companion_tls_slots>
    g_companion_tls{};

std::size_t companion_tls_index(
    std::uint64_t key) noexcept
{
    return static_cast<std::size_t>(
        ((key >> 4u) ^ (key >> 13u) ^ (key >> 23u)) &
        (k_companion_tls_slots - 1u));
}

#if defined(DSRRL_COMPANION_TLS_4WAY)
using companion_way_policy = companion_tls_way_selector<
    k_companion_tls_slots, 4u>;
thread_local std::array<
    std::uint8_t,
    companion_way_policy::sets> g_companion_tls_victims{};

std::size_t companion_tls_slot(std::uint64_t key) noexcept
{
    return companion_way_policy::choose(
        g_companion_tls, g_companion_tls_victims, key);
}
#else
std::size_t companion_tls_slot(std::uint64_t key) noexcept
{
    return companion_tls_index(key);
}
#endif

#if defined(DSRRL_RESOURCE_EPOCH_SHARD_SYNC)
resource_view_epoch<k_companion_tls_slots> g_view_epochs{};
#endif

std::uint64_t companion_epoch(std::uint64_t key) noexcept
{
#if defined(DSRRL_RESOURCE_EPOCH_SHARD_SYNC)
    return g_view_epochs.current(key);
#else
    (void)key;
    return g_cache_epoch.load(std::memory_order_acquire);
#endif
}

void invalidate_companion_epoch(std::uint64_t key) noexcept
{
#if defined(DSRRL_RESOURCE_EPOCH_SHARD_SYNC)
    g_view_epochs.invalidate(key);
#else
    (void)key;
    g_cache_epoch.fetch_add(1u, std::memory_order_release);
#endif
}

void invalidate_all_companion_epochs() noexcept
{
#if defined(DSRRL_RESOURCE_EPOCH_SHARD_SYNC)
    g_view_epochs.invalidate_all();
#else
    g_cache_epoch.fetch_add(1u, std::memory_order_release);
#endif
}

#if defined(DSRRL_STUTTER_PROFILE)
// Sampled on all draw threads, flushed only every 128 decisions to avoid
// turning contention diagnostics into a new shared cache-line bottleneck.
// Remainders are thread-local; 5s log estimates have a <=127/thread error.
std::atomic<std::uint64_t> g_companion_tls_hit_samples{0u};
std::atomic<std::uint64_t> g_companion_tls_mutex_samples{0u};
thread_local std::uint32_t g_companion_tls_hit_local = 0u;
thread_local std::uint32_t g_companion_tls_mutex_local = 0u;

void profile_companion_snapshot(bool tls_hit) noexcept
{
    auto &local = tls_hit
        ? g_companion_tls_hit_local : g_companion_tls_mutex_local;
    if (++local == 128u) {
        auto &total = tls_hit
            ? g_companion_tls_hit_samples : g_companion_tls_mutex_samples;
        total.fetch_add(128u, std::memory_order_relaxed);
        local = 0u;
    }
}
#endif


std::atomic<std::uint64_t> g_named_views{0};
std::atomic<std::uint64_t> g_sidecar_ready{0};
std::atomic<std::uint64_t> g_sidecar_missing{0};
std::atomic<std::uint64_t> g_sidecar_unsupported{0};
std::atomic<std::uint64_t> g_spec_requests{0};
std::atomic<std::uint64_t> g_fixed_pointlight_spec_requests{0};
std::atomic<std::uint64_t> g_fixed_pointlight_diffuse_requests{0};
std::atomic<std::uint64_t> g_diffuse_requests{0};
std::atomic<std::uint64_t> g_normal_requests{0};
std::atomic<std::uint64_t> g_fail_open{0};
std::atomic_bool g_quarantined{false};
std::atomic_bool g_subsurface_body_f_diag_logged{false};
std::atomic_bool g_subsurface_body_m_diag_logged{false};
std::atomic_bool g_subsurface_body_unknown_diag_logged{false};
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R3)
std::atomic_bool g_pmetal_r3_spec_attest_ok_logged{false};
std::atomic_bool g_pmetal_r3_spec_attest_fail_logged{false};
#endif
#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R5)
std::atomic_bool g_pmetal_r5_diffuse_attest_ok_logged{false};
std::atomic_bool g_pmetal_r5_diffuse_attest_fail_logged{false};
std::atomic_bool g_pmetal_r5_normal_attest_ok_logged{false};
std::atomic_bool g_pmetal_r5_normal_attest_fail_logged{false};
#endif
bool g_hot_telemetry_enabled = false;

// Exact DSR body SpecMap identities used only by the Ps_Body[DSBT]
// Subsurface -> PTDE plain-surface bridge. They are certified tuple members
// but intentionally absent from the generic equipment SpecRGB allowlist.
constexpr std::uint64_t k_subsurface_body_f_spec_hash =
    0x724f5fe11b342205ull;
constexpr std::uint64_t k_subsurface_body_m_spec_hash =
    0x777ede2aecc3d102ull;

bool exact_subsurface_body_spec_hash(
    std::uint64_t hash) noexcept
{
    return
        hash == k_subsurface_body_f_spec_hash ||
        hash == k_subsurface_body_m_spec_hash;
}

bool runtime_hot_telemetry_requested() noexcept
{
    return dsrrl::runtime::telemetry::
        hot_enabled();
}

// PTDE texture sidecars are authorized only on authenticated equipment draws.
// Folder membership or basename identity is never sufficient authority. World,
// map and static-environment draws fail open even if they collide by filename.
void hot_count(
    std::atomic<std::uint64_t> &counter) noexcept
{
    if (g_hot_telemetry_enabled)
        counter.fetch_add(
            1u,
            std::memory_order_relaxed);
}

std::uint64_t fnv_name(
    const wchar_t *name,
    std::size_t length) noexcept
{
    std::uint64_t h = 14695981039346656037ull;

    if (name == nullptr)
        return h;

    for (std::size_t i = 0u;
         i < length;
         ++i) {
        std::uint32_t c =
            static_cast<std::uint32_t>(
                name[i]);

        if (c >= static_cast<std::uint32_t>(L'A') &&
            c <= static_cast<std::uint32_t>(L'Z'))
            c += 32u;

        h ^= static_cast<std::uint64_t>(c);
        h *= 1099511628211ull;
    }

    return h;
}

std::uint64_t fnv_name(
    const std::wstring &name) noexcept
{
    return fnv_name(
        name.data(),
        name.size());
}

void release_view(
    ID3D11ShaderResourceView *&view) noexcept
{
    if (view != nullptr) {
        view->Release();
        view = nullptr;
    }
}

void release_set(companion_set &set) noexcept
{
    release_view(set.specular);
    release_view(set.diffuse);
    release_view(set.normal);

    if (set.device != nullptr) {
        set.device->Release();
        set.device = nullptr;
    }

    set.logical_hash = 0u;
}

void release_cache() noexcept
{
    std::unordered_map<
        std::uint64_t,
        companion_set> dead;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        invalidate_all_companion_epochs();
        dead.swap(g_cache);
        g_ambiguous_view_device.clear();
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
        g_attested_names_by_resource.clear();
        g_ambiguous_named_resource_device.clear();
        g_late_native_t1_attempts.clear();
#endif
    }

    for (auto &entry : dead)
        release_set(entry.second);
}

void release_cache_for_device(
    ID3D11Device *device) noexcept
{
    if (device == nullptr)
        return;

    std::vector<companion_set> dead;

    {
        std::lock_guard<std::mutex> lock(g_mutex);

        bool changed = false;
        for (auto it = g_cache.begin();
             it != g_cache.end();) {
            if (it->second.device == device) {
                dead.push_back(it->second);
                it = g_cache.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }

        for (auto it = g_ambiguous_view_device.begin();
             it != g_ambiguous_view_device.end();) {
            if (it->second == device) {
                it = g_ambiguous_view_device.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }

#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
        // Attempts are cheap negative lookups; a device-lifetime boundary
        // must never retain any old view-handle tombstones.
        g_late_native_t1_attempts.clear();
        for (auto it = g_attested_names_by_resource.begin();
             it != g_attested_names_by_resource.end();) {
            if (it->second.device == device)
                it = g_attested_names_by_resource.erase(it);
            else ++it;
        }
        for (auto it = g_ambiguous_named_resource_device.begin();
             it != g_ambiguous_named_resource_device.end();) {
            if (it->second == device)
                it = g_ambiguous_named_resource_device.erase(it);
            else ++it;
        }
#endif
        if (changed)
            invalidate_all_companion_epochs();
    }

    for (auto &set : dead)
        release_set(set);
}

std::filesystem::path process_dir()
{
    std::wstring buffer(32768, L'\0');

    const DWORD n =
        GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));

    if (n == 0u ||
        n >= buffer.size())
        return {};

    buffer.resize(n);
    return std::filesystem::path(
        buffer).parent_path();
}

const wchar_t *class_dir(
    asset_class cls) noexcept
{
    switch (cls) {
    case asset_class::specular:
        return L"Specular";
    case asset_class::diffuse:
        return L"Diffuse";
    case asset_class::normal:
        return L"Normals";
    }

    return L"";
}

std::filesystem::path sidecar_path(
    asset_class cls,
    const std::wstring &logical_name)
{
    auto root = process_dir();
    if (root.empty() ||
        logical_name.empty())
        return {};

    std::filesystem::path leaf(
        logical_name);
    std::wstring filename =
        leaf.filename().wstring();

    if (filename.size() < 4u ||
        _wcsicmp(
            filename.c_str() +
                filename.size() - 4u,
            L".dds") != 0)
        filename += L".dds";

    return root /
        L"DSRRL" /
        class_dir(cls) /
        filename;
}

template <typename T>
bool read_struct(
    const std::vector<std::uint8_t> &bytes,
    std::size_t offset,
    T &out) noexcept
{
    if (offset > bytes.size() ||
        sizeof(T) >
            bytes.size() - offset)
        return false;

    std::memcpy(
        &out,
        bytes.data() + offset,
        sizeof(T));
    return true;
}

load_result load_dds(
    ID3D11Device *device,
    const std::filesystem::path &path,
    asset_class cls,
    std::uint64_t logical_hash) noexcept
{
    if (device == nullptr ||
        path.empty())
        return {};

    try {
        if (!std::filesystem::is_regular_file(path))
            return {};

        std::ifstream stream(
            path,
            std::ios::binary);
        if (!stream)
            return {};

        stream.seekg(
            0,
            std::ios::end);
        const auto end =
            stream.tellg();
        if (end <= 0)
            return {
                nullptr,
                load_status::unsupported
            };

        // A 16384x16384 BC7 2D texture with its complete mip chain
        // is below 512 MiB. Reject oversized/corrupted sidecar files
        // BEFORE allocating their full contents in an init-view callback.
        constexpr std::streamoff k_max_sidecar_file_bytes =
            512ll * 1024ll * 1024ll;
        if (end > k_max_sidecar_file_bytes)
            return {
                nullptr,
                load_status::unsupported
            };

        const auto size =
            static_cast<std::size_t>(end);

        stream.seekg(
            0,
            std::ios::beg);

        std::vector<std::uint8_t> bytes(size);
        if (!stream.read(
                reinterpret_cast<char *>(
                    bytes.data()),
                static_cast<std::streamsize>(
                    bytes.size())))
            return {
                nullptr,
                load_status::unsupported
            };

#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R3)
        // R3 closes the remaining content-authority hole on the exact
        // material seen in the owner's active black/red R2 draw. R2 only
        // authenticated logical identity + a non-null companion SRV, so a
        // stale same-name DDS could silently enter t10. For HD_A_9550_s we
        // now require the exact retail PTDE DDS bytes recovered by the PTDE
        // equipment extractor.
        constexpr std::uint64_t k_hd_a_9550_s_hash =
            0xaedd13872d25cffbull;
        constexpr char k_hd_a_9550_s_sha256[] =
            "988d288dc65c7cb0856b65243ed33cbcc16862ea8c233d48ca0a377f0e8b1f77";

        if (cls == asset_class::specular &&
            logical_hash == k_hd_a_9550_s_hash) {
            const auto digest =
                operators::legacy_plan::hashing::sha256(
                    bytes.data(),
                    bytes.size());
            if (!operators::legacy_plan::hashing::matches_hex(
                    digest,
                    k_hd_a_9550_s_sha256)) {
                if (!g_pmetal_r3_spec_attest_fail_logged.exchange(
                        true,
                        std::memory_order_relaxed))
                    reshade::log::message(
                        reshade::log::level::warning,
                        "[DSRRL PMETAL R3 SPEC ATTEST] HD_A_9550_s exact PTDE SHA mismatch; fail-open, stale/wrong sidecar rejected.");
                return {
                    nullptr,
                    load_status::hash_mismatch
                };
            }

            if (!g_pmetal_r3_spec_attest_ok_logged.exchange(
                    true,
                    std::memory_order_relaxed))
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL PMETAL R3 SPEC ATTEST] HD_A_9550_s exact PTDE SHA PASS.");
        }


#if defined(DSRRL_PMETAL_FULL_PTDE_HEMENV_R5)
        // PR216 exact-content authority: for the active HD_A_9550 P_Metal
        // equipment draw, t0 diffuse and t2 normal must be exact retail PTDE
        // bytes. Same-name stale/wrong sidecars fail open.
        constexpr std::uint64_t k_hd_a_9550_diffuse_hash =
            0x00b515c367a88e31ull;
        constexpr std::uint64_t k_hd_a_9550_normal_hash =
            0xaedcfe872d25ac4cull;
        constexpr char k_hd_a_9550_diffuse_sha256[] =
            "ed4e1d104b5db59d51ace7fdaafbeeb2a8c36eea830233edf29b01907ed46803";
        constexpr char k_hd_a_9550_normal_sha256[] =
            "3c83caa2a95a85e1fc2d0397edfd48a271a3829330d7a0d5aa74b0cd599de78c";

        if (cls == asset_class::diffuse &&
            logical_hash == k_hd_a_9550_diffuse_hash) {
            const auto digest =
                operators::legacy_plan::hashing::sha256(
                    bytes.data(),
                    bytes.size());
            if (!operators::legacy_plan::hashing::matches_hex(
                    digest,
                    k_hd_a_9550_diffuse_sha256)) {
                if (!g_pmetal_r5_diffuse_attest_fail_logged.exchange(
                        true,
                        std::memory_order_relaxed))
                    reshade::log::message(
                        reshade::log::level::warning,
                        "[DSRRL PMETAL R5 DIFFUSE ATTEST] HD_A_9550 exact PTDE SHA mismatch; fail-open, stale/wrong diffuse sidecar rejected.");
                return {
                    nullptr,
                    load_status::hash_mismatch
                };
            }

            if (!g_pmetal_r5_diffuse_attest_ok_logged.exchange(
                    true,
                    std::memory_order_relaxed))
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL PMETAL R5 DIFFUSE ATTEST] HD_A_9550 exact PTDE SHA PASS.");
        }

        if (cls == asset_class::normal &&
            logical_hash == k_hd_a_9550_normal_hash) {
            const auto digest =
                operators::legacy_plan::hashing::sha256(
                    bytes.data(),
                    bytes.size());
            if (!operators::legacy_plan::hashing::matches_hex(
                    digest,
                    k_hd_a_9550_normal_sha256)) {
                if (!g_pmetal_r5_normal_attest_fail_logged.exchange(
                        true,
                        std::memory_order_relaxed))
                    reshade::log::message(
                        reshade::log::level::warning,
                        "[DSRRL PMETAL R5 NORMAL ATTEST] HD_A_9550_n exact PTDE SHA mismatch; fail-open, stale/wrong normal sidecar rejected.");
                return {
                    nullptr,
                    load_status::hash_mismatch
                };
            }

            if (!g_pmetal_r5_normal_attest_ok_logged.exchange(
                    true,
                    std::memory_order_relaxed))
                reshade::log::message(
                    reshade::log::level::info,
                    "[DSRRL PMETAL R5 NORMAL ATTEST] HD_A_9550_n exact PTDE SHA PASS.");
        }
#endif
#else
        (void)cls;
        (void)logical_hash;
#endif

        std::uint32_t magic = 0u;
        dds_header header{};

        if (!read_struct(
                bytes,
                0u,
                magic) ||
            magic != k_dds_magic ||
            !read_struct(
                bytes,
                4u,
                header) ||
            header.size != 124u ||
            header.pixel_format.size != 32u ||
            header.width == 0u ||
            header.height == 0u)
            return {
                nullptr,
                load_status::unsupported
            };

        DXGI_FORMAT format =
            DXGI_FORMAT_UNKNOWN;
        std::size_t data_offset =
            4u + sizeof(dds_header);

        if (header.pixel_format.fourcc ==
            k_fourcc_dxt1) {
            format = DXGI_FORMAT_BC1_UNORM;
        } else if (
            header.pixel_format.fourcc ==
            k_fourcc_dxt3) {
            format = DXGI_FORMAT_BC2_UNORM;
        } else if (
            header.pixel_format.fourcc ==
            k_fourcc_dxt5) {
            format = DXGI_FORMAT_BC3_UNORM;
        } else if (
            header.pixel_format.fourcc ==
            k_fourcc_dx10) {
            dds_header_dx10 dx10{};

            if (!read_struct(
                    bytes,
                    data_offset,
                    dx10))
                return {
                    nullptr,
                    load_status::unsupported
                };

            data_offset +=
                sizeof(dx10);

            if (dx10.resource_dimension !=
                    k_resource_dimension_texture2d ||
                dx10.array_size != 1u ||
                (dx10.misc_flag &
                    k_misc_texturecube) != 0u)
                return {
                    nullptr,
                    load_status::unsupported
                };

            switch (
                static_cast<DXGI_FORMAT>(
                    dx10.dxgi_format)) {
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC7_UNORM:
                format =
                    static_cast<DXGI_FORMAT>(
                        dx10.dxgi_format);
                break;
            default:
                return {
                    nullptr,
                    load_status::unsupported
                };
            }
        } else {
            return {
                nullptr,
                load_status::unsupported
            };
        }

        if ((header.caps2 &
             k_caps2_cubemap) != 0u ||
            header.width > 16384u ||
            header.height > 16384u ||
            header.depth > 1u)
            return {
                nullptr,
                load_status::unsupported
            };

        const std::uint32_t mip_count =
            std::max(
                1u,
                header.mip_count);

        std::uint32_t max_mips = 1u;
        for (auto extent =
                 std::max(
                     header.width,
                     header.height);
             extent > 1u;
             extent >>= 1u)
            ++max_mips;

        if (mip_count > max_mips)
            return {
                nullptr,
                load_status::unsupported
            };

        const std::uint32_t block_bytes =
            format == DXGI_FORMAT_BC1_UNORM
                ? 8u
                : 16u;

        std::vector<
            D3D11_SUBRESOURCE_DATA> initial;
        initial.reserve(mip_count);

        std::size_t cursor =
            data_offset;
        std::uint32_t width =
            header.width;
        std::uint32_t height =
            header.height;

        for (std::uint32_t mip = 0u;
             mip < mip_count;
             ++mip) {
            const std::uint32_t blocks_w =
                std::max(
                    1u,
                    (width + 3u) / 4u);
            const std::uint32_t blocks_h =
                std::max(
                    1u,
                    (height + 3u) / 4u);

            const std::uint32_t row_pitch =
                blocks_w * block_bytes;
            const std::uint32_t slice_pitch =
                row_pitch * blocks_h;

            if (cursor > bytes.size() ||
                slice_pitch >
                    bytes.size() - cursor)
                return {
                    nullptr,
                    load_status::unsupported
                };

            D3D11_SUBRESOURCE_DATA sub{};
            sub.pSysMem =
                bytes.data() + cursor;
            sub.SysMemPitch =
                row_pitch;
            sub.SysMemSlicePitch =
                slice_pitch;
            initial.push_back(sub);

            cursor += slice_pitch;
            width =
                std::max(
                    1u,
                    width >> 1u);
            height =
                std::max(
                    1u,
                    height >> 1u);
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = header.width;
        desc.Height = header.height;
        desc.MipLevels = mip_count;
        desc.ArraySize = 1u;
        desc.Format = format;
        desc.SampleDesc.Count = 1u;
        desc.Usage =
            D3D11_USAGE_IMMUTABLE;
        desc.BindFlags =
            D3D11_BIND_SHADER_RESOURCE;

        ID3D11Texture2D *texture =
            nullptr;
        ID3D11ShaderResourceView *view =
            nullptr;

        g_internal_create = true;

        HRESULT create_texture = E_FAIL;
        {
            stutter_profile::scope gpu_time(stutter_profile::stage::d3d_texture_create);
            create_texture = device->CreateTexture2D(
                &desc,
                initial.data(),
                &texture);
        }

        HRESULT create_view = E_FAIL;
        if (SUCCEEDED(create_texture) &&
            texture != nullptr)
            create_view =
                device->CreateShaderResourceView(
                    texture,
                    nullptr,
                    &view);

        g_internal_create = false;

        if (texture != nullptr)
            texture->Release();

        if (FAILED(create_texture) ||
            FAILED(create_view) ||
            view == nullptr) {
            if (view != nullptr)
                view->Release();
            return {
                nullptr,
                load_status::create_failed
            };
        }

        return {
            view,
            load_status::ready
        };
    } catch (...) {
        g_internal_create = false;
        return {
            nullptr,
            load_status::create_failed
        };
    }
}

load_result safe_load_sidecar(
    ID3D11Device *device,
    asset_class cls,
    const std::wstring &logical_name,
    std::uint64_t hash) noexcept
{
    stutter_profile::scope dds_time(stutter_profile::stage::sidecar_load);
    // Path construction may allocate/throw even when the DDS loader itself
    // is noexcept. ReShade's resource-view callback must fail open instead
    // of propagating a C++ exception into the host D3D11 call.
    try {
        return load_dds(
            device,
            sidecar_path(cls, logical_name),
            cls,
            hash);
    } catch (...) {
        return {nullptr, load_status::unsupported};
    }
}

void account_load(
    load_status status) noexcept
{
    if (status == load_status::ready) {
        ++g_sidecar_ready;
    } else if (
        status == load_status::missing) {
        ++g_sidecar_missing;
    } else {
        ++g_sidecar_unsupported;
    }
}

bool snapshot_companion_cached(
    ID3D11ShaderResourceView *stock,
    asset_class cls,
    std::uint64_t &hash,
    ID3D11ShaderResourceView *&view,
    bool retain_view) noexcept
{
    hash = 0u;
    view = nullptr;

    if (stock == nullptr)
        return false;

    const auto key =
        static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(
                stock));
    const auto epoch = companion_epoch(key);
    auto &cached =
        g_companion_tls[
            companion_tls_slot(key)];

    auto publish_from_cache =
        [&]() noexcept {
            if (!cached.present)
                return false;

            hash = cached.logical_hash;

            ID3D11ShaderResourceView *selected =
                nullptr;
            switch (cls) {
            case asset_class::specular:
                selected = cached.specular;
                break;
            case asset_class::diffuse:
                selected = cached.diffuse;
                break;
            case asset_class::normal:
                selected = cached.normal;
                break;
            }

            if (retain_view &&
                selected != nullptr) {
                selected->AddRef();
                view = selected;
            }
            return true;
        };

    if (cached.key == key &&
        cached.epoch == epoch) {
#if defined(DSRRL_STUTTER_PROFILE)
        profile_companion_snapshot(true);
#endif
        return publish_from_cache();
    }

#if defined(DSRRL_STUTTER_PROFILE)
    profile_companion_snapshot(false);
#endif
    std::uint64_t logical_hash = 0u;
    ID3D11ShaderResourceView *spec = nullptr;
    ID3D11ShaderResourceView *diff = nullptr;
    ID3D11ShaderResourceView *norm = nullptr;
    bool present = false;
    std::uint64_t snapshot_epoch = epoch;

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        snapshot_epoch = companion_epoch(key);

        if (g_ambiguous_view_device.find(key) ==
                g_ambiguous_view_device.end()) {
            const auto found =
                g_cache.find(key);
            if (found != g_cache.end()) {
                present = true;
                logical_hash =
                    found->second.logical_hash;
                spec =
                    found->second.specular;
                diff =
                    found->second.diffuse;
                norm =
                    found->second.normal;

                // These references become TLS cache ownership after the mutex is
                // released. Holding them before unlock prevents a concurrent
                // resource-view destruction from invalidating the snapshot.
                if (spec != nullptr)
                    spec->AddRef();
                if (diff != nullptr)
                    diff->AddRef();
                if (norm != nullptr)
                    norm->AddRef();
            }
        }
    }

    // Release any previous TLS-owned sidecars outside g_mutex; a COM Release
    // can trigger a resource-view destruction callback and must not re-enter
    // the same mutex.
    cached.assign_owned(
        key,
        snapshot_epoch,
        logical_hash,
        spec,
        diff,
        norm,
        present);

    return publish_from_cache();
}

void logical_hashes_for(
    ID3D11ShaderResourceView *const *stocks,
    std::size_t count,
    std::uint64_t *hashes) noexcept
{
    if (hashes == nullptr)
        return;

    std::fill_n(hashes, count, 0u);

    if (stocks == nullptr ||
        count == 0u)
        return;

    for (std::size_t i = 0u;
         i < count;
         ++i) {
        ID3D11ShaderResourceView *ignored =
            nullptr;
        (void)snapshot_companion_cached(
            stocks[i],
            asset_class::specular,
            hashes[i],
            ignored,
            false);
    }
}

struct companion_lookup_request {
    ID3D11ShaderResourceView *stock = nullptr;
    asset_class cls = asset_class::specular;
};

void lookup_many(
    const companion_lookup_request *requests,
    std::size_t count,
    ID3D11ShaderResourceView **views) noexcept
{
    if (views == nullptr)
        return;

    std::fill_n(views, count, nullptr);

    if (requests == nullptr ||
        count == 0u)
        return;

    for (std::size_t i = 0u;
         i < count;
         ++i) {
        std::uint64_t ignored_hash = 0u;
        (void)snapshot_companion_cached(
            requests[i].stock,
            requests[i].cls,
            ignored_hash,
            views[i],
            true);
    }
}

void inspect_many(
    const companion_lookup_request *requests,
    std::size_t count,
    std::uint64_t *hashes,
    ID3D11ShaderResourceView **views) noexcept
{
    if (hashes == nullptr ||
        views == nullptr)
        return;

    std::fill_n(hashes, count, 0u);
    std::fill_n(views, count, nullptr);

    if (requests == nullptr ||
        count == 0u)
        return;

    for (std::size_t i = 0u;
         i < count;
         ++i) {
        (void)snapshot_companion_cached(
            requests[i].stock,
            requests[i].cls,
            hashes[i],
            views[i],
            true);
    }
}

#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
void on_init_resource(
    reshade::api::device *device,
    const reshade::api::resource_desc &desc,
    const reshade::api::subresource_data *initial_data,
    reshade::api::resource_usage,
    reshade::api::resource resource)
{
    if (g_internal_create || g_quarantined.load() ||
        device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11 ||
        resource.handle == 0u)
        return;
    // Independently collect native resource creation data; the CPU cache
    // logical name may already have left the loader's TLS scope here.
    spc25_physical::init(device,desc,initial_data,resource);
    const wchar_t *raw = nullptr;
    std::size_t size = 0u;
    if (!texture_identity_transport::snapshot_raw(raw, size) ||
        raw == nullptr || size == 0u)
        return;
    const auto hash = fnv_name(raw, size);
    if (!generated::spec_equipment_name_hash_allowed_v12(hash) &&
        !exact_subsurface_body_spec_hash(hash) &&
        !generated::diffuse_name_hash_allowed_v12(hash) &&
        !generated::normal_name_hash_allowed_v12(hash))
        return;

    g_name_exact_init_resource.fetch_add(
        1u, std::memory_order_relaxed);
    // Construct before locking. Mismatched identities for the same live
    // resource are quarantined until the actual destroy_resource callback.
    try {
        attested_resource_name current{};
        current.device = reinterpret_cast<ID3D11Device *>(device->get_native());
        current.logical_name.assign(raw, size);
        current.hash = hash;
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_ambiguous_named_resource_device.count(resource.handle))
            return;
        const auto it = g_attested_names_by_resource.find(resource.handle);
        if (it == g_attested_names_by_resource.end()) {
            g_attested_names_by_resource.emplace(resource.handle,
                std::move(current));
        } else if (it->second.device != current.device ||
                   it->second.hash != current.hash ||
                   it->second.logical_name != current.logical_name) {
            g_attested_names_by_resource.erase(it);
            g_ambiguous_named_resource_device.emplace(
                resource.handle, current.device);
        }
    } catch (...) {
        // Allocation must never unwind into the D3D11/ReShade callback.
    }
}

void on_destroy_resource(
    reshade::api::device *,
    reshade::api::resource resource)
{
    if (resource.handle == 0u) return;
    spc25_physical::drop_resource(resource.handle);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_attested_names_by_resource.erase(resource.handle);
    g_ambiguous_named_resource_device.erase(resource.handle);
    // This is a separate mutex: do not invert lock ordering.
}
#endif

#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
// Some D3D11 resources carry a native debug object name even after the
// retail loader's thread-local name has expired. Treat a name as authority
// only if it is an *exact* canonical basename already allowlisted by V12.
// No suffix stripping, path guessing or BC1/size-based matching.
bool snapshot_native_exact_debug_name(
    ID3D11DeviceChild *child,
    std::wstring &out,
    bool *any_label_present = nullptr) noexcept
{
    out.clear();
    if (any_label_present != nullptr)
        *any_label_present = false;
    if (child == nullptr)
        return false;

    // The native object may have either documented D3D11 debug-name
    // representation. Both are just candidate *producers*; only exact
    // V12 canonical logical texture identities become bridge authority.
    char ascii_bytes[260]{};
    UINT ascii_size = static_cast<UINT>(sizeof(ascii_bytes));
    const HRESULT ascii_result = child->GetPrivateData(
        WKPDID_D3DDebugObjectName, &ascii_size, ascii_bytes);
    const bool ascii_present =
        SUCCEEDED(ascii_result) && ascii_size > 0u;
    wchar_t wide_chars[260]{};
    UINT wide_bytes = static_cast<UINT>(sizeof(wide_chars));
    const HRESULT wide_result = child->GetPrivateData(
        WKPDID_D3DDebugObjectNameW, &wide_bytes, wide_chars);
    const bool wide_present =
        SUCCEEDED(wide_result) && wide_bytes > 0u;

    if (any_label_present != nullptr)
        *any_label_present = ascii_present || wide_present;
    if (!ascii_present && !wide_present)
        return false;

    auto exact_canonical = [](const wchar_t *chars,
                              std::size_t length) noexcept {
        if (chars == nullptr || length < 7u ||
            length >= 260u ||
            chars[length - 2u] != L'_' ||
            (chars[length - 1u] != L's' &&
             chars[length - 1u] != L'S'))
            return false;
        for (std::size_t i = 0u; i < length; ++i) {
            const wchar_t ch = chars[i];
            if (!((ch >= L'a' && ch <= L'z') ||
                  (ch >= L'A' && ch <= L'Z') ||
                  (ch >= L'0' && ch <= L'9') ||
                  ch == L'_'))
                return false;
        }
        const auto hash = fnv_name(chars, length);
        return generated::spec_equipment_name_hash_allowed_v12(hash) ||
            exact_subsurface_body_spec_hash(hash) ||
            generated::diffuse_name_hash_allowed_v12(hash) ||
            generated::normal_name_hash_allowed_v12(hash);
    };

    wchar_t ascii_name[260]{};
    std::size_t ascii_chars = 0u;
    if (ascii_present) {
        if (ascii_size >= sizeof(ascii_bytes))
            return false;
        ascii_chars = ascii_size;
        if (ascii_bytes[ascii_chars - 1u] == '\0')
            --ascii_chars;
        if (ascii_chars == 0u)
            return false;
        for (std::size_t i = 0u; i < ascii_chars; ++i) {
            const unsigned char ch =
                static_cast<unsigned char>(ascii_bytes[i]);
            if (ch > 0x7fu)
                return false;
            ascii_name[i] = static_cast<wchar_t>(ch);
        }
        if (!exact_canonical(ascii_name, ascii_chars))
            return false;
    }

    std::size_t wide_length = 0u;
    if (wide_present) {
        if ((wide_bytes % sizeof(wchar_t)) != 0u ||
            wide_bytes >= sizeof(wide_chars))
            return false;
        wide_length = wide_bytes / sizeof(wchar_t);
        if (wide_length != 0u &&
            wide_chars[wide_length - 1u] == L'\0')
            --wide_length;
        if (!exact_canonical(wide_chars, wide_length))
            return false;
    }

    if (ascii_present && wide_present &&
        (ascii_chars != wide_length ||
         std::char_traits<wchar_t>::compare(
             ascii_name, wide_chars, ascii_chars) != 0))
        return false; // two independently provided names disagree

    const wchar_t *chosen = ascii_present ? ascii_name : wide_chars;
    const auto count = ascii_present ? ascii_chars : wide_length;
    try {
        out.assign(chosen, count);
        return !out.empty();
    } catch (...) {
        out.clear();
        return false;
    }
}
bool on_create_resource_view(
    reshade::api::device *device,
    reshade::api::resource resource,
    reshade::api::resource_usage usage,
    reshade::api::resource_view_desc &)
{
    auto &pending = g_early_view_name;
    if (pending.pending) {
        // Nested pre-create callbacks have no guaranteed unambiguous
        // pre/post pairing. Drop both rather than borrowing a name.
        pending.ambiguous = true;
        return false;
    }
    if (g_internal_create || g_quarantined.load() ||
        device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11 ||
        resource.handle == 0u ||
        usage != reshade::api::resource_usage::shader_resource)
        return false;
    const wchar_t *raw = nullptr;
    std::size_t length = 0u;
    if (!texture_identity_transport::snapshot_raw(raw, length) ||
        raw == nullptr || length == 0u)
        return false;
    const auto hash = fnv_name(raw, length);
    if (!generated::spec_equipment_name_hash_allowed_v12(hash) &&
        !exact_subsurface_body_spec_hash(hash) &&
        !generated::diffuse_name_hash_allowed_v12(hash) &&
        !generated::normal_name_hash_allowed_v12(hash))
        return false;

    g_name_exact_create_view.fetch_add(
        1u, std::memory_order_relaxed);
    try {
        pending.device =
            reinterpret_cast<ID3D11Device *>(device->get_native());
        pending.resource = resource.handle;
        pending.usage = usage;
        pending.name.assign(raw, length);
        pending.pending = !pending.name.empty();
    } catch (...) {
        pending.clear();
    }
    // Strictly passive: never alter the original D3D resource descriptor.
    return false;
}
#endif

void on_init_resource_view(
    reshade::api::device *device,
    reshade::api::resource resource,
    reshade::api::resource_usage usage,
    const reshade::api::resource_view_desc &,
    reshade::api::resource_view view)
{
    // Stock/no-SPC builds do not need the resource-to-view name handoff.
    (void)resource;
    if (g_internal_create ||
        g_quarantined.load() ||
        device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11 ||
        usage !=
            reshade::api::resource_usage::shader_resource ||
        view.handle == 0u)
        return;

    stutter_profile::scope view_time(stutter_profile::stage::resource_view_init);
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    // Raw ReShade view/resource handles are authoritative D3D11 objects.
    // Track before the name lookup, which may be missing on the retail host.
    spc25_physical::link_view(view.handle,resource.handle);
#endif
    const wchar_t *logical_name_raw = nullptr;
    std::size_t logical_name_length = 0u;
    const bool direct_name =
        texture_identity_transport::snapshot_raw(
            logical_name_raw, logical_name_length);
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    std::wstring early_name{};
    bool pre_view_attested = false;
    auto &pre_view = g_early_view_name;
    if (pre_view.pending) {
        if (!pre_view.ambiguous &&
            pre_view.device ==
                reinterpret_cast<ID3D11Device *>(device->get_native()) &&
            pre_view.resource == resource.handle &&
            pre_view.usage == usage) {
            early_name = std::move(pre_view.name);
            pre_view_attested = !early_name.empty();
        }
        pre_view.clear();
    }
    std::wstring attested_name{};
    bool resource_handoff = false;
    if (resource.handle != 0u) {
        try {
            std::lock_guard<std::mutex> lock(g_mutex);
            const auto entry =
                g_attested_names_by_resource.find(resource.handle);
            if (entry != g_attested_names_by_resource.end() &&
                entry->second.device ==
                    reinterpret_cast<ID3D11Device *>(device->get_native())) {
                attested_name = entry->second.logical_name;
                resource_handoff = !attested_name.empty();
            }
        } catch (...) {
            return;
        }
    }
    // This query refers to the very D3D11 SRV/resource supplied by
    // ReShade in the current init_resource_view callback; no global-name
    // lookup or last-observed SRV can satisfy it.
    std::wstring native_debug_name{};
    bool debug_attested = false;
    if (resource.handle != 0u) {
        auto *native_resource = reinterpret_cast<ID3D11Resource *>(
            static_cast<std::uintptr_t>(resource.handle));
        debug_attested = snapshot_native_exact_debug_name(
            native_resource, native_debug_name);
    }
    if (view.handle != 0u) {
        std::wstring view_debug_name{};
        const bool view_debug = snapshot_native_exact_debug_name(
            reinterpret_cast<ID3D11ShaderResourceView *>(
                static_cast<std::uintptr_t>(view.handle)),
            view_debug_name);
        if (view_debug && debug_attested &&
            view_debug_name != native_debug_name)
            return;
        if (view_debug && !debug_attested) {
            native_debug_name = std::move(view_debug_name);
            debug_attested = true;
        }
    }
    auto matches_name = [](
        const std::wstring &name,
        const wchar_t *other,
        std::size_t length) noexcept {
        return other != nullptr && name.size() == length &&
            std::char_traits<wchar_t>::compare(
                name.c_str(), other, length) == 0;
    };
    // A discrepancy between any two attested producers invalidates the
    // association. Do not let an early name overwrite a direct or
    // resource-lifetime name.
    if (debug_attested && direct_name &&
        !matches_name(native_debug_name, logical_name_raw, logical_name_length))
        return;
    if (debug_attested && pre_view_attested &&
        native_debug_name != early_name)
        return;
    if (debug_attested && resource_handoff &&
        native_debug_name != attested_name)
        return;
    if (pre_view_attested && direct_name &&
        !matches_name(early_name, logical_name_raw, logical_name_length))
        return;
    if (pre_view_attested && resource_handoff &&
        early_name != attested_name)
        return;

    if (resource_handoff && direct_name) {
        // Two independently observed names disagree -> no resource alias.
        if (attested_name.size() != logical_name_length ||
            std::char_traits<wchar_t>::compare(
                attested_name.c_str(), logical_name_raw,
                logical_name_length) != 0)
            return;
    } else if (resource_handoff) {
        logical_name_raw = attested_name.c_str();
        logical_name_length = attested_name.size();
    } else if (pre_view_attested && !direct_name) {
        logical_name_raw = early_name.c_str();
        logical_name_length = early_name.size();
    } else if (debug_attested && !direct_name) {
        logical_name_raw = native_debug_name.c_str();
        logical_name_length = native_debug_name.size();
    } else if (!direct_name) {
        return;
    }
#else
    if (!direct_name)
        return;
#endif

    const auto logical_hash =
        fnv_name(
            logical_name_raw,
            logical_name_length);

    const bool subsurface_body_spec =
        exact_subsurface_body_spec_hash(
            logical_hash);
    const bool spec_member =
        generated::spec_equipment_name_hash_allowed_v12(
            logical_hash) ||
        subsurface_body_spec;
    const bool diffuse_member =
        generated::diffuse_name_hash_allowed_v12(
            logical_hash);
    const bool normal_member =
        generated::normal_name_hash_allowed_v12(
            logical_hash);

    if (!spec_member &&
        !diffuse_member &&
        !normal_member)
        return;
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    if (direct_name)
        g_name_exact_init_view.fetch_add(
            1u, std::memory_order_relaxed);
#endif

    std::wstring logical_name;
    try {
        logical_name.assign(
            logical_name_raw,
            logical_name_length);
    } catch (...) {
        return;
    }

    auto *native_device =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());
    if (native_device == nullptr)
        return;

    companion_set set{};
    set.device = native_device;
    set.device->AddRef();
    set.logical_hash =
        logical_hash;

    if (spec_member) {
        const auto loaded =
            safe_load_sidecar(
                native_device,
                asset_class::specular,
                logical_name,
                logical_hash);
        account_load(loaded.status);
        set.specular = loaded.view;
    }

    if (generated::
        diffuse_target_hash_allowed_v12(
            logical_hash)) {
        const auto loaded =
            safe_load_sidecar(
                native_device,
                asset_class::diffuse,
                logical_name,
                logical_hash);
        account_load(loaded.status);
        set.diffuse = loaded.view;
    }

    if (generated::
        normal_target_hash_allowed_v12(
            logical_hash)) {
        const auto loaded =
            safe_load_sidecar(
                native_device,
                asset_class::normal,
                logical_name,
                logical_hash);
        account_load(loaded.status);
        set.normal = loaded.view;
    }

    const auto key =
        static_cast<std::uint64_t>(
            view.handle);

    companion_set old{};
    bool had_old = false;
    bool accepted = false;

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        if (g_ambiguous_view_device.find(key) ==
            g_ambiguous_view_device.end()) {
            const auto found =
                g_cache.find(key);

            if (found != g_cache.end() &&
                found->second.logical_hash != logical_hash) {
                // The native handle is still live but now claims a different
                // exact logical identity. Neither association is safe. Remove
                // the previous record and quarantine this handle until its
                // destroy_resource_view callback.
                old = found->second;
                g_cache.erase(found);
                g_ambiguous_view_device.emplace(
                    key,
                    native_device);
                had_old = true;
            } else if (found != g_cache.end()) {
                old = found->second;
                found->second = set;
                had_old = true;
                accepted = true;
            } else {
                g_cache.emplace(
                    key,
                    set);
                accepted = true;
            }

            invalidate_companion_epoch(key);
        }
    }

    if (accepted)
        set = {};

    if (had_old)
        release_set(old);

    // If this handle was already quarantined, or this observation created the
    // conflict, the newly created companion set never becomes authoritative.
    if (!accepted)
        release_set(set);

#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    if (resource_handoff && !direct_name && accepted &&
        !g_attested_resource_view_join_logged.exchange(
            true, std::memory_order_relaxed))
        reshade::log::message(reshade::log::level::info,
            "[DSRRL SPC RESOURCE JOIN] source=init_resource_exact_name carrier=resource_to_view exact_allowlist=1 failopen_preserved=1 pixel=OPEN");
    if (pre_view_attested && !resource_handoff && !direct_name &&
        accepted && !g_early_view_join_logged.exchange(
            true, std::memory_order_relaxed))
        reshade::log::message(reshade::log::level::info,
            "[DSRRL SPC RESOURCE JOIN] source=create_resource_view_exact_name carrier=pre_post_view exact_allowlist=1 failopen_preserved=1 pixel=OPEN");
    if (debug_attested && !resource_handoff && !direct_name &&
        !pre_view_attested && accepted &&
        !g_native_debug_name_join_logged.exchange(
            true, std::memory_order_relaxed))
        reshade::log::message(reshade::log::level::info,
            "[DSRRL SPC RESOURCE JOIN] source=stock_d3d11_exact_debug_name carrier=native_resource_or_srv exact_allowlist=1 failopen_preserved=1 pixel=OPEN");
#endif
    ++g_named_views;
}

void on_destroy_resource_view(
    reshade::api::device *,
    reshade::api::resource_view view)
{
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    spc25_physical::drop_view(view.handle);
#endif
    companion_set dead{};
    bool found = false;
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(
            g_mutex);

        const auto key =
            static_cast<std::uint64_t>(
                view.handle);
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
        g_late_native_t1_attempts.erase(key);
#endif
        if (g_ambiguous_view_device.erase(key) != 0u)
            changed = true;

        const auto it =
            g_cache.find(key);

        if (it != g_cache.end()) {
            dead = it->second;
            g_cache.erase(it);
            found = true;
            changed = true;
        }

        if (changed)
            invalidate_companion_epoch(key);
    }

    if (found)
        release_set(dead);
}

void on_destroy_device(
    reshade::api::device *device)
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11)
        return;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());

#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    spc25_physical::drop_device(native);
#endif
    release_cache_for_device(
        native);
}

#if defined(DSRRL_STUTTER_PROFILE)
// Once per five seconds, emit one bounded aggregate line. The instrumented
// FLVER/MTD/sidecar hooks only update atomics; they never write to disk.
void on_stutter_present(
    reshade::api::command_queue *,
    reshade::api::swapchain *,
    const reshade::api::rect *,
    const reshade::api::rect *,
    std::uint32_t,
    const reshade::api::rect *)
{
    if (!stutter_profile::on_present_due())
        return;

    const auto frame = stutter_profile::take_frame_snapshot();
    const auto frequency =
        static_cast<double>(stutter_profile::frequency());
    char message[2048]{};
    const auto written = std::snprintf(
        message,
        sizeof(message),
        "[DSRRL STUTTER R43] window=5s present_frames=%llu present_max_ms=%.3f present_over100ms=%llu stages=name:calls:total_ms:max_ms:over100ms",
        static_cast<unsigned long long>(frame.frames),
        static_cast<double>(frame.max_ticks) * 1000.0 / frequency,
        static_cast<unsigned long long>(frame.over_100ms));
    if (written <= 0)
        return;
    std::size_t used =
        std::min(static_cast<std::size_t>(written), sizeof(message) - 1u);

    for (std::size_t index = 0u;
         index < stutter_profile::stage_count && used < sizeof(message) - 1u;
         ++index) {
        const auto stage =
            static_cast<stutter_profile::stage>(index);
        const auto sample =
            stutter_profile::take_stage_snapshot(stage);
        if (sample.calls == 0u)
            continue;

        const auto result = std::snprintf(
            message + used,
            sizeof(message) - used,
            " %s:%llu:%.3f:%.3f:%llu",
            stutter_profile::stage_names[index],
            static_cast<unsigned long long>(sample.calls),
            static_cast<double>(sample.total_ticks) * 1000.0 / frequency,
            static_cast<double>(sample.max_ticks) * 1000.0 / frequency,
            static_cast<unsigned long long>(sample.over_100ms));
        if (result <= 0)
            break;
        used += std::min(
            static_cast<std::size_t>(result),
            sizeof(message) - 1u - used);
    }

    reshade::log::message(reshade::log::level::info, message);
    char sync_line[256]{};
    const auto sync_written = std::snprintf(
        sync_line, sizeof(sync_line),
        "[DSRRL SYNC R43] sampled_companion_tls_hit=%llu sampled_companion_mutex_fallback=%llu sample_batch=128",
        static_cast<unsigned long long>(
            g_companion_tls_hit_samples.exchange(0u, std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_companion_tls_mutex_samples.exchange(0u, std::memory_order_relaxed)));
    if (sync_written > 0)
        reshade::log::message(reshade::log::level::info, sync_line);
#if defined(DSRRL_STUTTER_HITCH_TRACE)
    const auto hitches = stutter_profile::take_hitch_window();
    for (std::size_t i = 0u; i < hitches.count; ++i) {
        const auto &h = hitches.frames[i];
        const double to_ms = 1000.0 / frequency;
        const auto &sha = h.stages[0u];
        const auto &dds = h.stages[1u];
        const auto &mtd = h.stages[2u];
        const auto &registry = h.stages[3u];
        char detail[768]{};
        const auto count = std::snprintf(
            detail, sizeof(detail),
            "[DSRRL HITCH FRAME] qpc_begin=%llu qpc_end=%llu gap_ms=%.3f "
            "flver_sha=calls:%llu,all_ms:%.3f,present_ms:%.3f "
            "dds=calls:%llu,all_ms:%.3f,present_ms:%.3f "
            "mtd=calls:%llu,all_ms:%.3f,present_ms:%.3f "
            "flver_registry=calls:%llu,all_ms:%.3f,present_ms:%.3f",
            static_cast<unsigned long long>(h.begin_ticks),
            static_cast<unsigned long long>(h.end_ticks),
            static_cast<double>(h.end_ticks - h.begin_ticks) * to_ms,
            static_cast<unsigned long long>(sha.calls),
            static_cast<double>(sha.ticks_all) * to_ms,
            static_cast<double>(sha.ticks_present_thread) * to_ms,
            static_cast<unsigned long long>(dds.calls),
            static_cast<double>(dds.ticks_all) * to_ms,
            static_cast<double>(dds.ticks_present_thread) * to_ms,
            static_cast<unsigned long long>(mtd.calls),
            static_cast<double>(mtd.ticks_all) * to_ms,
            static_cast<double>(mtd.ticks_present_thread) * to_ms,
            static_cast<unsigned long long>(registry.calls),
            static_cast<double>(registry.ticks_all) * to_ms,
            static_cast<double>(registry.ticks_present_thread) * to_ms);
        if (count > 0)
            reshade::log::message(reshade::log::level::info, detail);
    }
#endif
}
#endif

bool append_retained_request(
    prepared_material_resource_draw &prepared,
    const island_draw_adapter_request &request,
    ID3D11ShaderResourceView *view) noexcept
{
    if (view == nullptr ||
        prepared.request_count >=
            prepared.requests.size() ||
        prepared.retained_count >=
            prepared.retained_views.size())
        return false;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            request,
            verify) !=
        island_draw_adapter_result::ready)
        return false;

    prepared.requests[
        prepared.request_count++] =
        request;
    prepared.retained_views[
        prepared.retained_count++] =
        view;
    return true;
}

} // namespace

material_resource_draw_runtime::
material_resource_draw_runtime(
    core::renderer_core &core) noexcept
    : core_(core)
{
}

material_resource_draw_runtime::
~material_resource_draw_runtime()
{
    unregister_events();
}

bool material_resource_draw_runtime::
register_events() noexcept
{
    if (g_core != nullptr &&
        g_core != &core_)
        return false;

    g_core = &core_;
    g_quarantined.store(false);
    g_hot_telemetry_enabled =
        runtime_hot_telemetry_requested();

#if defined(DSRRL_STUTTER_PROFILE)
    reshade::register_event<reshade::addon_event::present>(on_stutter_present);
    reshade::log::message(reshade::log::level::info,
        "[DSRRL STUTTER R43] bounded QPC profiling active; no feature gates changed");
#endif
#if defined(DSRRL_RESOURCE_EPOCH_SHARD_SYNC)
    reshade::log::message(reshade::log::level::info,
        "[DSRRL SYNC R43] companion SRV TLS epochs=KEY_BUCKET_256, fail-open and per-view lifetime preserved");
#if defined(DSRRL_COMPANION_TLS_4WAY)
    reshade::log::message(reshade::log::level::info,
        "[DSRRL TLS WAY] mode=4WAY_64SETS_256TOTAL_SYNC_ON");
#else
    reshade::log::message(reshade::log::level::info,
        "[DSRRL TLS WAY] mode=1WAY_256SETS_256TOTAL_SYNC_ON");
#endif
#else
    reshade::log::message(reshade::log::level::info,
        "[DSRRL SYNC R43] companion SRV TLS epochs=GLOBAL_BASELINE, fail-open and per-view lifetime preserved");
#endif
#if defined(DSRRL_STUTTER_HITCH_TRACE)
    reshade::log::message(reshade::log::level::info,
        "[DSRRL HITCH FRAME] exact R43 same-operator per-Present QPC buckets active");
#endif

#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    reshade::register_event<reshade::addon_event::init_resource>(
        on_init_resource);
    reshade::register_event<reshade::addon_event::destroy_resource>(
        on_destroy_resource);
    reshade::register_event<reshade::addon_event::create_resource_view>(
        on_create_resource_view);
#endif

    reshade::register_event<
        reshade::addon_event::init_resource_view>(
            on_init_resource_view);

    reshade::register_event<
        reshade::addon_event::destroy_resource_view>(
            on_destroy_resource_view);

    reshade::register_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);

    return true;
}

void material_resource_draw_runtime::
unregister_events() noexcept
{
    if (g_core != &core_)
        return;

#if defined(DSRRL_STUTTER_PROFILE)
    reshade::unregister_event<reshade::addon_event::present>(on_stutter_present);
#endif

    reshade::unregister_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);

    reshade::unregister_event<
        reshade::addon_event::destroy_resource_view>(
            on_destroy_resource_view);

    reshade::unregister_event<
        reshade::addon_event::init_resource_view>(
            on_init_resource_view);
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    reshade::unregister_event<reshade::addon_event::create_resource_view>(
        on_create_resource_view);
    reshade::unregister_event<reshade::addon_event::destroy_resource>(
        on_destroy_resource);
    reshade::unregister_event<reshade::addon_event::init_resource>(
        on_init_resource);
#endif

    release_cache();
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    spc25_physical::clear();
#endif
    g_core = nullptr;
}

specular_companion_probe material_resource_draw_runtime::
probe_exact_specular_companion(
    ID3D11DeviceContext *context) noexcept
{
    specular_companion_probe out{};
    out.context_valid = context != nullptr;
    out.quarantined = g_quarantined.load();

    if (!out.context_valid ||
        out.quarantined)
        return out;

    ID3D11ShaderResourceView *stock = nullptr;
    context->PSGetShaderResources(
        1u,
        1u,
        &stock);
    out.stock_bound = stock != nullptr;
    if (!out.stock_bound)
        return out;

    ID3D11ShaderResourceView *companion = nullptr;
    out.snapshot_resolved =
        snapshot_companion_cached(
            stock,
            asset_class::specular,
            out.logical_hash,
            companion,
            true);

    release_view(stock);

    out.logical_hash_allowed =
        out.logical_hash != 0u &&
        generated::spec_equipment_name_hash_allowed_v12(
            out.logical_hash);
    out.companion_ready =
        companion != nullptr;

    release_view(companion);
    return out;
}

bool material_resource_draw_runtime::
try_recover_exact_bound_spec_from_native_name(
    ID3D11DeviceContext *context) noexcept
{
#if !defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    (void)context;
    return false;
#else
    if (context == nullptr || g_quarantined.load())
        return false;

    ID3D11ShaderResourceView *stock = nullptr;
    context->PSGetShaderResources(1u, 1u, &stock);
    if (stock == nullptr) return false;
    const auto key = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(stock));

    // Limit the experiment to two exact lookups per native view lifetime,
    // even when hundreds of draws reuse a missing-name stock t1.
    bool denied = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_ambiguous_view_device.find(key) !=
                g_ambiguous_view_device.end() ||
            g_cache.find(key) != g_cache.end()) {
            denied = true;
        } else {
            try {
                auto &attempts = g_late_native_t1_attempts[key];
                if (attempts >= 2u)
                    denied = true;
                else
                    ++attempts;
            } catch (...) {
                denied = true;
            }
        }
    }
    if (denied) {
        // Releasing a D3D object under g_mutex can reenter a resource
        // destruction callback. Release only after leaving the lock.
        stock->Release();
        return false;
    }

    std::wstring name{};
    bool view_label = false;
    const bool view_name =
        snapshot_native_exact_debug_name(stock, name, &view_label);
    bool resource_label_seen = false;
    ID3D11Resource *resource = nullptr;
    stock->GetResource(&resource);
    if (resource != nullptr) {
        // CPU writer name->DLTexture2D->native COM address versus the
        // receiver's ACTUAL PSGetShaderResources(1) SRV and GetResource.
        // Pointer-value comparison only: no swaps, casts, or lifetime
        // authority. Original stock objects remain pinned in this scope.
        texture_identity_transport::diagnose_native_ps_t1(
            stock, resource);
        std::wstring resource_name{};
        const bool resource_named =
            snapshot_native_exact_debug_name(
                resource, resource_name, &resource_label_seen);
        if (view_name && resource_named &&
            name != resource_name)
            log_late_native_stage_once(
                3u, "conflicting_native_view_resource_names");
        if (!view_name && !resource_named &&
            (view_label || resource_label_seen))
            log_late_native_stage_once(
                1u, "native_label_present_not_canonical_allowlisted");
        resource->Release();
        if (resource_named) {
            if (view_name && name != resource_name) {
                stock->Release();
                return false; // contradictory exact identities
            }
            if (!view_name)
                name = std::move(resource_name);
        }
    }

    if (name.empty()) {
        // The actual SRV returned by PSGetShaderResources(1) is pinned.
        // GetResource gives native resource identity independent of MTD
        // guesses or missing retail debug object names. Never swap here.
        spc25_physical::inspect(stock);
        if (!g_name_producer_cut_logged.exchange(
                true, std::memory_order_relaxed)) {
            const auto names = texture_identity_transport::liveness();
            char msg[1152]{};
            std::snprintf(msg, sizeof(msg),
                "[DSRRL SPC25 NAME CUT] engine_lookup_calls=%llu utf16_names=%llu engine_scope_clear=%llu snapshot_hits=%llu gpu_init_resource_exact=%llu gpu_create_view_exact=%llu gpu_init_view_direct_exact=%llu joined_views=%llu cpu_808d_writer=%llu cpu_808d_named_scope=%llu cache_name_exact=%llu cache_name_different=%llu cache_name_unreadable=%llu cpu_rtti_exact=%llu cpu_rtti_unavailable=%llu tex2d_cpu=%llu tex2d_fields_readable=%llu tex2d_p28_texture_nonnull=%llu tex2d_p30_srv_nonnull=%llu tex2d_both_nonnull=%llu decode_calls=%llu decode_payload_readable=%llu decode_writer_ptr_seen=%llu source_to_srv=UNVERIFIED pixel=OPEN",
                static_cast<unsigned long long>(names.hook_calls),
                static_cast<unsigned long long>(names.names_captured),
                static_cast<unsigned long long>(names.names_cleared),
                static_cast<unsigned long long>(names.name_snapshots),
                static_cast<unsigned long long>(
                    g_name_exact_init_resource.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(
                    g_name_exact_create_view.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(
                    g_name_exact_init_view.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(
                    g_named_views.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(names.packet_writer_calls),
                static_cast<unsigned long long>(names.packet_named_in_scope),
                static_cast<unsigned long long>(names.cache_name_equal),
                static_cast<unsigned long long>(names.cache_name_different),
                static_cast<unsigned long long>(names.cache_name_unreadable),
                static_cast<unsigned long long>(names.cpu_rtti_exact),
                static_cast<unsigned long long>(names.cpu_rtti_unavailable),
                static_cast<unsigned long long>(names.cpu_tex2d_cpu_objects),
                static_cast<unsigned long long>(names.cpu_tex2d_fields_readable),
                static_cast<unsigned long long>(names.cpu_tex2d_p28_nonnull),
                static_cast<unsigned long long>(names.cpu_tex2d_p30_nonnull),
                static_cast<unsigned long long>(names.cpu_tex2d_both_nonnull),
                static_cast<unsigned long long>(names.decode_calls),
                static_cast<unsigned long long>(names.decode_payload_readable),
                static_cast<unsigned long long>(names.decode_writer_pointer_seen));
            reshade::log::message(reshade::log::level::info, msg);
        }
        if (!view_label && !resource_label_seen)
            log_late_native_stage_once(
                0u, "stock_t1_missing_exact_native_name");
        else
            log_late_native_stage_once(
                1u, "native_label_present_not_canonical_allowlisted");
        stock->Release();
        return false;
    }
    const auto hash = fnv_name(name);
    if (!generated::spec_equipment_name_hash_allowed_v12(hash)) {
        stock->Release();
        return false;
    }
    ID3D11Device *device = nullptr;
    stock->GetDevice(&device);
    if (device == nullptr) {
        stock->Release();
        return false;
    }
    // Retain the exact live stock SRV until after registration is complete.
    // Releasing it during DDS disk/GPU load could allow handle reuse before
    // this companion is installed, silently aliasing a different texture.
    auto loaded =
        safe_load_sidecar(device, asset_class::specular, name, hash);
    account_load(loaded.status);
    if (loaded.status != load_status::ready ||
        loaded.view == nullptr) {
        log_late_native_stage_once(
            2u, "canonical_name_but_exact_ptde_sidecar_unavailable");
        release_view(loaded.view);
        device->Release();
        stock->Release();
        return false; // no exact PTDE SpecRGB sidecar, stock fail-open
    }

    companion_set set{};
    set.device = device; // owns GetDevice reference
    set.logical_hash = hash;
    set.specular = loaded.view; // owns loader reference
    bool accepted = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        // Another thread or view destruction may have changed ownership
        // since the negative lookup. Never overwrite or borrow a record.
        if (g_ambiguous_view_device.find(key) ==
                g_ambiguous_view_device.end() &&
            g_cache.find(key) == g_cache.end()) {
            try {
                g_cache.emplace(key, set);
                accepted = true;
                invalidate_companion_epoch(key);
            } catch (...) {
                accepted = false;
            }
        }
    }
    if (accepted)
        set = {};
    else
        release_set(set);
    // The native handle was pinned through the whole transaction.
    stock->Release();

    if (accepted &&
        !g_late_native_t1_recovered_logged.exchange(
            true, std::memory_order_relaxed)) {
        char line[320]{};
        std::snprintf(line, sizeof(line),
            "[DSRRL SPC25 LATE T1] source=stock_bound_native_exact_debug_name hash=%016llx sidecar_ready=1 stock_identity=EXACT runtime=CONSUMER_RETRY pixel=OPEN",
            static_cast<unsigned long long>(hash));
        reshade::log::message(reshade::log::level::info,line);
    }
    return accepted;
#endif
}

bool material_resource_draw_runtime::
exact_specular_companion_ready(
    ID3D11DeviceContext *context) noexcept
{
    return probe_exact_specular_companion(
        context).ready();
}

bool material_resource_draw_runtime::
prepare_draw_requests(
    ID3D11DeviceContext *context,
    std::uint32_t receiver_id,
    const operators::material_response::
        mtd_semantic_query &query,
    bool full_material_response_ready,
    bool spec_rgb_consumer_ready,
    prepared_material_resource_draw &prepared) noexcept
{
    prepared = {};

    if (context == nullptr ||
        receiver_id == 0u ||
        g_quarantined.load())
        return false;

    const bool exact_material =
        query.material.valid &&
        query.material.owner_tuple_exact;

    if (!exact_material)
        return true;

    if (!spec_rgb_consumer_ready &&
        (receiver_id < 24u ||
         receiver_id > 35u))
        return true;

    ID3D11ShaderResourceView *views[3]{};
    context->PSGetShaderResources(
        0u,
        3u,
        views);

    const bool ready =
        prepare_draw_requests_bound(
            views,
            receiver_id,
            query,
            full_material_response_ready,
            spec_rgb_consumer_ready,
            prepared);

    for (auto *&view : views)
        release_view(view);

    return ready;
}

bool material_resource_draw_runtime::
prepare_draw_requests_bound(
    ID3D11ShaderResourceView *const (&views)[3],
    std::uint32_t receiver_id,
    const operators::material_response::
        mtd_semantic_query &query,
    bool full_material_response_ready,
    bool spec_rgb_consumer_ready,
    prepared_material_resource_draw &prepared) noexcept
{
    prepared = {};

    if (receiver_id == 0u ||
        g_quarantined.load())
        return false;

    const bool exact_material =
        query.material.valid &&
        query.material.owner_tuple_exact;

    if (!exact_material)
        return true;

    if (!spec_rgb_consumer_ready &&
        (receiver_id < 24u ||
         receiver_id > 35u))
        return true;

    const companion_lookup_request
        companion_requests[3]{
            {views[0], asset_class::diffuse},
            {views[1], asset_class::specular},
            {views[2], asset_class::normal}
        };
    std::uint64_t hashes[3]{};
    ID3D11ShaderResourceView *companions[3]{};
    inspect_many(
        companion_requests,
        3u,
        hashes,
        companions);

    const auto h0 = hashes[0];
    const auto h1 = hashes[1];
    const auto h2 = hashes[2];

    if (full_material_response_ready &&
        spec_rgb_consumer_ready &&
        core_.features().enabled(
            core::operator_id::spec_rgb) &&
        receiver_id >= 24u &&
        receiver_id <= 47u) {
        auto *replacement =
            companions[1];
        companions[1] = nullptr;

        const bool exact_companion =
            h1 != 0u &&
            generated::
                spec_equipment_name_hash_allowed_v12(
                    h1) &&
            replacement != nullptr;

        operators::resource_bridges::
            spec_rgb_context context_spec{};
        context_spec.receiver_id =
            receiver_id;
        context_spec.actual_material_verified =
            exact_material;
        context_spec.material_specular_consumer_verified =
            operators::material_response::
                classify_mtd_semantic(
                    query,
                    operators::material_response::
                        mtd_semantic_operator::spec_rgb)
                .state ==
            operators::material_response::
                mtd_semantic_state::use;
        context_spec.exact_name_ptde_companion_verified =
            exact_companion;
        context_spec.ptde_sidecar_ready =
            replacement != nullptr;
        context_spec.native_t10_transport_ready =
            spec_rgb_consumer_ready;
        context_spec.stock_t1_preserved =
            true;

        const auto decision =
            operators::resource_bridges::
                evaluate_spec_rgb_route(
                    context_spec,
                    query);

        if (decision.action ==
                operators::resource_bridges::
                    spec_rgb_action::
                        bind_ptde_t10_rgb &&
            replacement != nullptr) {
            island_draw_adapter_request request{};
            request.primary =
                core::operator_id::spec_rgb;
            request.receiver_verified = true;
            request.material_verified = true;
            request.srvs[0] = {
                decision.ptde_rgb_srv_slot,
                replacement
            };
            request.srv_count = 1u;

            if (append_retained_request(
                    prepared,
                    request,
                    replacement)) {
                prepared.spec_rgb = true;
                hot_count(g_spec_requests);
                replacement = nullptr;
            }
        }

        if (replacement != nullptr)
            replacement->Release();
    }

    const bool bmp_receiver =
        receiver_id >= 24u &&
        receiver_id <= 35u;

    const bool diffuse_pair =
        h0 != 0u &&
        h1 != 0u &&
        generated::diffuse_pair_allowed_v12(
            h1,
            h0);

    if (bmp_receiver &&
        exact_material &&
        diffuse_pair &&
        full_material_response_ready &&
        core_.features().enabled(
            core::operator_id::diffuse)) {
        auto *replacement =
            diffuse_pair
                ? companions[0]
                : nullptr;
        if (diffuse_pair)
            companions[0] = nullptr;

        const auto semantic =
            operators::material_response::
                classify_mtd_semantic(
                    query,
                    operators::material_response::
                        mtd_semantic_operator::diffuse);

        operators::resource_bridges::
            diffuse_bridge_context context_diff{};
        context_diff.receiver_id =
            receiver_id;
        context_diff.actual_material_verified =
            exact_material &&
            semantic.state ==
                operators::material_response::
                    mtd_semantic_state::use;
        context_diff.actual_bound_t0_verified =
            h0 != 0u;
        context_diff.exact_ptde_diffuse_companion_verified =
            diffuse_pair &&
            replacement != nullptr;
        context_diff.ptde_srv_ready =
            replacement != nullptr;
        context_diff.ptde_c100_donor_verified =
            full_material_response_ready;
        context_diff.diffuse_linear_receiver_ready =
            full_material_response_ready;
        context_diff.shared_material_route =
            true;
        context_diff.exact_texture_identity_conjunction =
            diffuse_pair;

        const auto decision =
            operators::resource_bridges::
                evaluate_diffuse_route(
                    context_diff);

        if (decision.action ==
                operators::resource_bridges::
                    diffuse_action::
                        bind_ptde_t0_and_full_material_response &&
            replacement != nullptr) {
            island_draw_adapter_request request{};
            request.primary =
                core::operator_id::diffuse;
            request.receiver_verified = true;
            request.material_verified = true;
            request.srvs[0] = {
                decision.srv_slot,
                replacement
            };
            request.srv_count = 1u;

            if (append_retained_request(
                    prepared,
                    request,
                    replacement)) {
                prepared.diffuse = true;
                hot_count(g_diffuse_requests);
                replacement = nullptr;
            }
        }

        if (replacement != nullptr)
            replacement->Release();
    }

    const bool normal_tuple =
        h0 != 0u &&
        h1 != 0u &&
        h2 != 0u &&
        generated::normal_tuple_allowed_v12(
            h0,
            h1,
            h2);

    if (bmp_receiver &&
        exact_material &&
        normal_tuple &&
        core_.features().enabled(
            core::operator_id::normal)) {
        auto *replacement =
            normal_tuple
                ? companions[2]
                : nullptr;
        if (normal_tuple)
            companions[2] = nullptr;

        const auto semantic =
            operators::material_response::
                classify_mtd_semantic(
                    query,
                    operators::material_response::
                        mtd_semantic_operator::normal_bump);

        operators::resource_bridges::
            normal_bridge_context context_norm{};
        context_norm.receiver_id =
            receiver_id;

        if (semantic.state ==
            operators::material_response::
                mtd_semantic_state::use) {
            context_norm.authority =
                operators::resource_bridges::
                    normal_route_authority::
                        homologous_material_route;
            context_norm.homologous_bmp_material_route =
                true;
        }

        context_norm.target_unambiguous =
            normal_tuple;
        context_norm.actual_bound_t2_verified =
            h2 != 0u;
        context_norm.exact_ptde_normal_sidecar_verified =
            normal_tuple &&
            replacement != nullptr;
        context_norm.ptde_srv_ready =
            replacement != nullptr;
        context_norm.preserve_stock_s2 =
            true;

        const auto decision =
            operators::resource_bridges::
                evaluate_normal_route(
                    context_norm);

        if (decision.action ==
                operators::resource_bridges::
                    normal_action::bind_ptde_t2 &&
            replacement != nullptr) {
            island_draw_adapter_request request{};
            request.primary =
                core::operator_id::normal;
            request.receiver_verified = true;
            request.material_verified =
                exact_material;
            request.srvs[0] = {
                decision.srv_slot,
                replacement
            };
            request.srv_count = 1u;

            if (append_retained_request(
                    prepared,
                    request,
                    replacement)) {
                prepared.normal = true;
                hot_count(g_normal_requests);
                replacement = nullptr;
            }
        }

        if (replacement != nullptr)
            replacement->Release();
    }

    for (auto *&companion : companions)
        release_view(companion);

    if (prepared.request_count == 0u)
        hot_count(g_fail_open);

    return true;
}

bool material_resource_draw_runtime::
prepare_fixed_pointlight_material_requests(
    ID3D11DeviceContext *context,
    const operators::material_response::
        mtd_semantic_query &query,
    bool exact_fixed_receiver_verified,
    bool direct_pointlight_material_authorized,
    bool blended_material,
    prepared_material_resource_draw &prepared) noexcept
{
    prepared = {};

    if (context == nullptr ||
        g_quarantined.load() ||
        !exact_fixed_receiver_verified ||
        !direct_pointlight_material_authorized ||
        !core_.features().enabled(
            core::operator_id::spec_rgb) ||
        !core_.features().enabled(
            core::operator_id::diffuse) ||
        !core_.features().enabled(
            core::operator_id::normal))
        return false;

    ID3D11ShaderResourceView *views[6]{};
    context->PSGetShaderResources(
        0u,
        6u,
        views);

    auto release_all = [&]() noexcept {
        for (auto *&view : views)
            release_view(view);
    };

    std::uint64_t hashes[6]{};
    logical_hashes_for(
        views,
        blended_material ? 6u : 3u,
        hashes);

    const auto h0 = hashes[0];
    const auto h1 = hashes[1];
    const auto h2 = hashes[2];
    const auto h3 =
        blended_material ? hashes[3] : 0u;
    const auto h4 =
        blended_material ? hashes[4] : 0u;
    const auto h5 =
        blended_material ? hashes[5] : 0u;

    const bool exact_material =
        query.material.valid &&
        query.material.owner_tuple_exact;

    const bool endpoint_a_pair =
        h0 != 0u &&
        h1 != 0u &&
        generated::spec_equipment_name_hash_allowed_v12(h1) &&
        generated::diffuse_pair_allowed_v12(h1,h0);

    const bool endpoint_b_pair =
        !blended_material ||
        (h3 != 0u &&
         h4 != 0u &&
         generated::spec_equipment_name_hash_allowed_v12(h4) &&
         generated::diffuse_pair_allowed_v12(h4,h3));

    const bool endpoint_a_normal =
        h2 != 0u &&
        generated::normal_tuple_allowed_v12(
            h0,h1,h2);
    const bool endpoint_b_normal =
        !blended_material ||
        (h5 != 0u &&
         generated::normal_tuple_allowed_v12(
            h3,h4,h5));

    if (!exact_material ||
        !endpoint_a_pair ||
        !endpoint_b_pair ||
        !endpoint_a_normal ||
        !endpoint_b_normal) {
        release_all();
        hot_count(g_fail_open);
        return true;
    }

    const companion_lookup_request
        companion_requests[6]{
            {views[1], asset_class::specular},
            {views[0], asset_class::diffuse},
            {views[2], asset_class::normal},
            {blended_material ? views[4] : nullptr,
             asset_class::specular},
            {blended_material ? views[3] : nullptr,
             asset_class::diffuse},
            {blended_material ? views[5] : nullptr,
             asset_class::normal}
        };
    ID3D11ShaderResourceView *companions[6]{};
    lookup_many(
        companion_requests,
        6u,
        companions);

    auto *spec_a = companions[0];
    auto *diff_a = companions[1];
    auto *normal_a = companions[2];
    auto *spec_b = companions[3];
    auto *diff_b = companions[4];
    auto *normal_b = companions[5];

    const bool endpoint_a_exact =
        spec_a != nullptr &&
        diff_a != nullptr &&
        normal_a != nullptr;
    const bool endpoint_b_exact =
        !blended_material ||
        (spec_b != nullptr &&
         diff_b != nullptr &&
         normal_b != nullptr);

    operators::resource_bridges::
        fixed_pointlight_spec_rgb_context bridge{};
    bridge.exact_fixed_receiver_verified = true;
    bridge.blended_material =
        blended_material;
    bridge.actual_material_verified = true;
    bridge.material_specular_consumer_verified = true;
    bridge.direct_pointlight_material_authorized = true;
    bridge.endpoint_a_identity_verified =
        endpoint_a_exact;
    bridge.endpoint_a_sidecar_ready =
        spec_a != nullptr;
    bridge.endpoint_b_identity_verified =
        endpoint_b_exact;
    bridge.endpoint_b_sidecar_ready =
        !blended_material || spec_b != nullptr;
    bridge.t10_t16_transport_ready = true;
    bridge.stock_t1_t4_preserved = true;

    const auto decision =
        operators::resource_bridges::
            evaluate_fixed_pointlight_spec_rgb_route(
                bridge,
                query);

    using action =
        operators::resource_bridges::
            fixed_pointlight_spec_rgb_action;

    if (decision.action ==
            action::preserve_host ||
        spec_a == nullptr ||
        diff_a == nullptr ||
        normal_a == nullptr ||
        (blended_material &&
         (spec_b == nullptr ||
          diff_b == nullptr ||
          normal_b == nullptr))) {
        release_view(spec_a);
        release_view(diff_a);
        release_view(normal_a);
        release_view(spec_b);
        release_view(diff_b);
        release_view(normal_b);
        release_all();
        hot_count(g_fail_open);
        return true;
    }

    island_draw_adapter_request spec_request{};
    spec_request.primary =
        core::operator_id::spec_rgb;
    spec_request.receiver_verified = true;
    spec_request.material_verified = true;
    spec_request.srvs[0] = {
        decision.endpoint_a_srv_slot,
        spec_a
    };
    spec_request.srv_count = 1u;

    if (decision.action ==
        action::bind_blended_t10_t16) {
        spec_request.srvs[1] = {
            decision.endpoint_b_srv_slot,
            spec_b
        };
        spec_request.srv_count = 2u;
    }

    island_draw_adapter_request diffuse_request{};
    diffuse_request.primary =
        core::operator_id::diffuse;
    diffuse_request.receiver_verified = true;
    diffuse_request.material_verified = true;
    diffuse_request.srvs[0] = {
        0u,
        diff_a
    };
    diffuse_request.srv_count = 1u;

    if (blended_material) {
        diffuse_request.srvs[1] = {
            3u,
            diff_b
        };
        diffuse_request.srv_count = 2u;
    }

    island_draw_adapter_request normal_request{};
    normal_request.primary =
        core::operator_id::normal;
    normal_request.receiver_verified = true;
    normal_request.material_verified = true;
    normal_request.srvs[0] = {
        2u,
        normal_a
    };
    normal_request.srv_count = 1u;
    if (blended_material) {
        normal_request.srvs[1] = {
            5u,
            normal_b
        };
        normal_request.srv_count = 2u;
    }

    draw_tx_mutation verify_spec{};
    draw_tx_mutation verify_diff{};
    draw_tx_mutation verify_norm{};
    const auto total_retained =
        spec_request.srv_count +
        diffuse_request.srv_count +
        normal_request.srv_count;

    if (build_island_draw_mutation(
            spec_request,
            verify_spec) !=
            island_draw_adapter_result::ready ||
        build_island_draw_mutation(
            diffuse_request,
            verify_diff) !=
            island_draw_adapter_result::ready ||
        build_island_draw_mutation(
            normal_request,
            verify_norm) !=
            island_draw_adapter_result::ready ||
        prepared.request_count + 3u >
            prepared.requests.size() ||
        prepared.retained_count +
            total_retained >
            prepared.retained_views.size()) {
        release_view(spec_a);
        release_view(diff_a);
        release_view(normal_a);
        release_view(spec_b);
        release_view(diff_b);
        release_view(normal_b);
        release_all();
        hot_count(g_fail_open);
        return true;
    }

    prepared.requests[
        prepared.request_count++] =
        spec_request;
    prepared.requests[
        prepared.request_count++] =
        diffuse_request;
    prepared.requests[
        prepared.request_count++] =
        normal_request;

    prepared.retained_views[
        prepared.retained_count++] =
        spec_a;
    spec_a = nullptr;
    if (spec_request.srv_count == 2u) {
        prepared.retained_views[
            prepared.retained_count++] =
            spec_b;
        spec_b = nullptr;
    }

    prepared.retained_views[
        prepared.retained_count++] =
        diff_a;
    diff_a = nullptr;
    if (diffuse_request.srv_count == 2u) {
        prepared.retained_views[
            prepared.retained_count++] =
            diff_b;
        diff_b = nullptr;
    }

    prepared.retained_views[
        prepared.retained_count++] =
        normal_a;
    normal_a = nullptr;
    if (normal_request.srv_count == 2u) {
        prepared.retained_views[
            prepared.retained_count++] =
            normal_b;
        normal_b = nullptr;
    }

    prepared.spec_rgb = true;
    prepared.diffuse = true;
    prepared.normal = true;
    hot_count(g_fixed_pointlight_spec_requests);
    hot_count(g_fixed_pointlight_diffuse_requests);
    hot_count(g_spec_requests);
    hot_count(g_diffuse_requests);
    hot_count(g_normal_requests);

    release_view(spec_a);
    release_view(diff_a);
    release_view(normal_a);
    release_view(spec_b);
    release_view(diff_b);
    release_view(normal_b);
    release_all();
    return true;
}

bool material_resource_draw_runtime::
prepare_subsurface_body_requests(
    ID3D11DeviceContext *context,
    std::uint32_t target_plain_receiver_id,
    prepared_material_resource_draw &prepared,
    operators::resource_bridges::subsurface_body_texture &body_texture) noexcept
{
    prepared = {};
    body_texture =
        operators::resource_bridges::
            subsurface_body_texture::unknown;

    if (context == nullptr ||
        g_quarantined.load() ||
        target_plain_receiver_id < 33u ||
        target_plain_receiver_id > 35u ||
        !core_.features().enabled(
            core::operator_id::spec_rgb) ||
        !core_.features().enabled(
            core::operator_id::diffuse) ||
        !core_.features().enabled(
            core::operator_id::normal))
        return false;

    ID3D11ShaderResourceView *views[3]{};
    context->PSGetShaderResources(
        0u,
        3u,
        views);

    std::uint64_t hashes[3]{};
    logical_hashes_for(
        views,
        3u,
        hashes);

    const auto h0 = hashes[0];
    const auto h1 = hashes[1];
    const auto h2 = hashes[2];
    const std::uint32_t native_012_mask =
        (views[0] != nullptr ? 1u << 0u : 0u) |
        (views[1] != nullptr ? 1u << 1u : 0u) |
        (views[2] != nullptr ? 1u << 2u : 0u);

    if (h1 == k_subsurface_body_f_spec_hash) {
        body_texture =
            operators::resource_bridges::
                subsurface_body_texture::bd_f_body_s;
    } else if (
        h1 == k_subsurface_body_m_spec_hash) {
        body_texture =
            operators::resource_bridges::
                subsurface_body_texture::bd_m_body_s;
    }

    // Ps_Body[DSBT] is a dedicated Subsurface bridge. Its two exact
    // stock body-spec resources are intentionally outside the generic
    // equipment SpecRGB name allowlist, but are already authenticated above
    // by exact logical hashes. Requiring the generic allowlist here makes the
    // dedicated route unreachable. Keep diffuse/normal tuple certification
    // intact and allow only these two exact body-spec identities.
    const bool exact_body_spec =
        exact_subsurface_body_spec_hash(h1);

    // Runtime dd6bb17 proved one exact DSBT male-body tuple that is
    // present in the live DSR binding and in the certified diffuse pair set,
    // but is absent from the generic normal_routes_v12 corpus:
    //   BD_M_body / BD_M_body_s / BD_M_body_n
    // Keep this as a dedicated Subsurface-only exception. Do not broaden the
    // generic equipment Normal route table from one receiver-local proof.
    constexpr std::uint64_t k_subsurface_body_m_diffuse_hash =
        0xcf6e2339c3593fb8ull;
    constexpr std::uint64_t k_subsurface_body_m_normal_hash =
        0x777ed32aecc3be51ull;
    const bool dedicated_body_m_normal_tuple =
        h0 == k_subsurface_body_m_diffuse_hash &&
        h1 == k_subsurface_body_m_spec_hash &&
        h2 == k_subsurface_body_m_normal_hash;

    const bool tuple_ready =
        body_texture !=
            operators::resource_bridges::
                subsurface_body_texture::unknown &&
        h0 != 0u &&
        h2 != 0u &&
        exact_body_spec &&
        generated::diffuse_pair_allowed_v12(
            h1,
            h0) &&
        (generated::normal_tuple_allowed_v12(
             h0,
             h1,
             h2) ||
         dedicated_body_m_normal_tuple);

    ID3D11ShaderResourceView *spec = nullptr;
    ID3D11ShaderResourceView *diff = nullptr;
    ID3D11ShaderResourceView *norm = nullptr;

    if (tuple_ready) {
        // Ps_Body[DSBT] is not an equipment texture replacement case for
        // diffuse/normal. The Common Body diffuse and normal payloads are
        // homologous between PTDE and DSR, so preserve the exact stock DSR
        // resources already bound at t0/t2. Only body SpecRGB is a dedicated
        // PTDE sidecar carrier. This keeps the Subsurface operator island
        // narrow and avoids inventing nonexistent Common Body Diffuse/Normal
        // sidecars.
        const companion_lookup_request spec_request{
            views[1],
            asset_class::specular
        };
        lookup_many(
            &spec_request,
            1u,
            &spec);

        diff = views[0];
        views[0] = nullptr;
        norm = views[2];
        views[2] = nullptr;
    }

    for (auto *&view : views)
        release_view(view);

    if (spec == nullptr ||
        diff == nullptr ||
        norm == nullptr) {
        if (dsrrl::runtime::telemetry::
                effect_enabled()) {
            std::atomic_bool *logged =
                &g_subsurface_body_unknown_diag_logged;
            const char *body_name = "UNKNOWN";
            if (body_texture ==
                operators::resource_bridges::
                    subsurface_body_texture::bd_f_body_s) {
                logged =
                    &g_subsurface_body_f_diag_logged;
                body_name = "BD_F_body_s";
            } else if (
                body_texture ==
                operators::resource_bridges::
                    subsurface_body_texture::bd_m_body_s) {
                logged =
                    &g_subsurface_body_m_diag_logged;
                body_name = "BD_M_body_s";
            }

            if (!logged->exchange(
                    true,
                    std::memory_order_relaxed)) {
                std::uint32_t native_0_15_mask = 0u;
                ID3D11ShaderResourceView *diag_views[16]{};
                context->PSGetShaderResources(
                    0u,
                    16u,
                    diag_views);
                for (std::uint32_t slot = 0u;
                     slot < 16u;
                     ++slot) {
                    if (diag_views[slot] != nullptr)
                        native_0_15_mask |=
                            1u << slot;
                    release_view(diag_views[slot]);
                }

                char line[896]{};
                std::snprintf(
                    line,
                    sizeof(line),
                    "[DSRRL SUBSURFACE RESOURCE] body=%s target_rx=%u tuple=%u ctx_type=%u native012=0x%02X native0_15=0x%04X h0=%016llx h1=%016llx h2=%016llx spec=%u diff=%u norm=%u",
                    body_name,
                    static_cast<unsigned>(
                        target_plain_receiver_id),
                    tuple_ready ? 1u : 0u,
                    static_cast<unsigned>(
                        context->GetType()),
                    static_cast<unsigned>(
                        native_012_mask),
                    static_cast<unsigned>(
                        native_0_15_mask),
                    static_cast<unsigned long long>(h0),
                    static_cast<unsigned long long>(h1),
                    static_cast<unsigned long long>(h2),
                    spec != nullptr ? 1u : 0u,
                    diff != nullptr ? 1u : 0u,
                    norm != nullptr ? 1u : 0u);
                reshade::log::message(
                    reshade::log::level::info,
                    line);
            }
        }

        release_view(spec);
        release_view(diff);
        release_view(norm);
        prepared = {};
        return false;
    }

    island_draw_adapter_request spec_request{};
    spec_request.primary =
        core::operator_id::spec_rgb;
    spec_request.receiver_verified = true;
    spec_request.material_verified = true;
    spec_request.srvs[0] = {
        10u,
        spec
    };
    spec_request.srv_count = 1u;

    island_draw_adapter_request diff_request{};
    diff_request.primary =
        core::operator_id::diffuse;
    diff_request.receiver_verified = true;
    diff_request.material_verified = true;
    diff_request.srvs[0] = {
        0u,
        diff
    };
    diff_request.srv_count = 1u;

    island_draw_adapter_request norm_request{};
    norm_request.primary =
        core::operator_id::normal;
    norm_request.receiver_verified = true;
    norm_request.material_verified = true;
    norm_request.srvs[0] = {
        2u,
        norm
    };
    norm_request.srv_count = 1u;

    if (!append_retained_request(
            prepared,
            spec_request,
            spec)) {
        release_view(spec);
        release_view(diff);
        release_view(norm);
        prepared = {};
        return false;
    }
    spec = nullptr;

    if (!append_retained_request(
            prepared,
            diff_request,
            diff)) {
        release_view(diff);
        release_view(norm);
        release_prepared_draw(prepared);
        return false;
    }
    diff = nullptr;

    if (!append_retained_request(
            prepared,
            norm_request,
            norm)) {
        release_view(norm);
        release_prepared_draw(prepared);
        return false;
    }
    norm = nullptr;

    prepared.spec_rgb = true;
    prepared.diffuse = true;
    prepared.normal = true;
    hot_count(g_spec_requests);
    hot_count(g_diffuse_requests);
    hot_count(g_normal_requests);
    return true;
}

bool material_resource_draw_runtime::
drop_spec_rgb_request(
    prepared_material_resource_draw &prepared) noexcept
{
    if (!prepared.spec_rgb ||
        prepared.request_count == 0u ||
        prepared.request_count != prepared.retained_count)
        return false;

    std::uint32_t index = prepared.request_count;
    for (std::uint32_t i = 0u;
         i < prepared.request_count;
         ++i) {
        if (prepared.requests[i].primary ==
            core::operator_id::spec_rgb) {
            index = i;
            break;
        }
    }

    if (index >= prepared.request_count)
        return false;

    if (prepared.retained_views[index] != nullptr)
        prepared.retained_views[index]->Release();

    for (std::uint32_t i = index + 1u;
         i < prepared.request_count;
         ++i) {
        prepared.requests[i - 1u] = prepared.requests[i];
        prepared.retained_views[i - 1u] = prepared.retained_views[i];
    }

    --prepared.request_count;
    --prepared.retained_count;
    prepared.requests[prepared.request_count] = {};
    prepared.retained_views[prepared.retained_count] = nullptr;
    prepared.spec_rgb = false;
    return true;
}


bool material_resource_draw_runtime::
keep_only_spec_rgb_request(
    prepared_material_resource_draw &prepared) noexcept
{
    if (!prepared.spec_rgb ||
        prepared.request_count == 0u ||
        prepared.request_count != prepared.retained_count)
        return false;

    std::uint32_t spec_index = prepared.request_count;
    for (std::uint32_t i = 0u;
         i < prepared.request_count;
         ++i) {
        if (prepared.requests[i].primary ==
            core::operator_id::spec_rgb) {
            if (spec_index != prepared.request_count)
                return false;
            spec_index = i;
        }
    }

    if (spec_index >= prepared.request_count)
        return false;

    auto spec_request = prepared.requests[spec_index];
    auto *spec_view = prepared.retained_views[spec_index];

    for (std::uint32_t i = 0u;
         i < prepared.retained_count;
         ++i) {
        if (i == spec_index)
            continue;
        if (prepared.retained_views[i] != nullptr)
            prepared.retained_views[i]->Release();
    }

    prepared.requests = {};
    prepared.retained_views = {};
    prepared.requests[0] = spec_request;
    prepared.retained_views[0] = spec_view;
    prepared.request_count = 1u;
    prepared.retained_count = 1u;
    prepared.spec_rgb = true;
    prepared.diffuse = false;
    prepared.normal = false;
    return true;
}

void material_resource_draw_runtime::
release_prepared_draw(
    prepared_material_resource_draw &prepared) noexcept
{
    for (std::uint32_t i = 0u;
         i < prepared.retained_count;
         ++i) {
        if (prepared.retained_views[i] != nullptr)
            prepared.retained_views[i]->Release();
    }

    prepared = {};
}

material_resource_telemetry
material_resource_draw_runtime::
telemetry() const noexcept
{
    return {
        g_named_views.load(),
        g_sidecar_ready.load(),
        g_sidecar_missing.load(),
        g_sidecar_unsupported.load(),
        g_spec_requests.load(),
        g_fixed_pointlight_spec_requests.load(),
        g_fixed_pointlight_diffuse_requests.load(),
        g_diffuse_requests.load(),
        g_normal_requests.load(),
        g_fail_open.load(),
        g_quarantined.load()
    };
}

void material_resource_draw_runtime::
reset() noexcept
{
    release_cache();
#if defined(DSRRL_EXPERIMENTAL_SPC_MATERIAL_BATCH)
    spc25_physical::clear();
#endif

    g_named_views.store(0u);
    g_sidecar_ready.store(0u);
    g_sidecar_missing.store(0u);
    g_sidecar_unsupported.store(0u);
    g_spec_requests.store(0u);
    g_fixed_pointlight_spec_requests.store(0u);
    g_fixed_pointlight_diffuse_requests.store(0u);
    g_diffuse_requests.store(0u);
    g_normal_requests.store(0u);
    g_fail_open.store(0u);
    g_subsurface_body_f_diag_logged.store(false);
    g_subsurface_body_m_diag_logged.store(false);
    g_subsurface_body_unknown_diag_logged.store(false);
    g_quarantined.store(false);
}

} // namespace dsrrl::runtime
