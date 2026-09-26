#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/upper_lower_draw_runtime.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/operators/lightbank/snapshot_freshness.hpp"
#include "dsrrl/operators/lightbank/hemdir3.hpp"
#include "dsrrl/runtime/generated_pmetal_env_source_authority.hpp"

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

    mutable std::mutex gpu_mutex;
    mutable ID3D11Device *device = nullptr;
    mutable ID3D11Buffer *ul_buffer = nullptr;
    mutable ID3D11Buffer *hemdir3_buffer = nullptr;

    ~snapshot()
    {
        if (ul_buffer != nullptr)
            ul_buffer->Release();
        if (hemdir3_buffer != nullptr)
            hemdir3_buffer->Release();
        if (device != nullptr)
            device->Release();
    }
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
};

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
using steady_packer_fn =
    void (__fastcall *)(void *,void *,std::int32_t);
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

constexpr std::uintptr_t k_rva_wrapper_type5 = 0x1C0BE0u;
constexpr std::uintptr_t k_rva_wrapper_type6 = 0x1C0C10u;
constexpr std::uintptr_t k_rva_steady_packer = 0x563B80u;
constexpr std::uintptr_t k_rva_blend_packer = 0x5637E0u;
constexpr std::uintptr_t k_rva_pmetal_env_blend = 0x563C30u;

constexpr std::uintptr_t k_ret_sel_1 = 0x20E019u;
constexpr std::uintptr_t k_ret_sel_2 = 0x20EB7Fu;
constexpr std::uintptr_t k_ret_sel_3 = 0x20FB9Eu;

constexpr std::array<std::uint8_t,17> k_wrapper_bytes = {
    0x48,0x83,0xEC,0x38,0x4D,0x8B,0xC8,0xF3,0x0F,0x11,0x5C,0x24,0x20,0x4C,0x8B,0x41,0x40
};
constexpr std::array<std::uint8_t,14> k_steady_packer_bytes = {
    0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x40,0x48,0x8B,0x41,0x18
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
constexpr std::size_t k_raw_upper_rgbm_offset = 0x24u;
constexpr std::size_t k_raw_lower_rgbm_offset = 0x2Cu;
constexpr float k_inv_pow = 1.0f / 2.2f;

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
std::array<inline_hook,2> g_hooks{};
inline_hook g_pmetal_env_hook{};

wrapper_fn g_wrapper5_orig = nullptr;
wrapper_fn g_wrapper6_orig = nullptr;
steady_packer_fn g_steady_packer_orig = nullptr;
lightbank_blend_packer_fn g_blend_packer_orig = nullptr;
pmetal_env_blend_fn g_pmetal_env_blend_orig = nullptr;

std::mutex g_snapshot_mutex;
std::unordered_map<
    std::uintptr_t,
    std::shared_ptr<const snapshot>> g_snapshots;

thread_local producer_tls g_producer{};
thread_local std::shared_ptr<const snapshot> g_draw_snapshot{};

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

std::uint64_t pmetal_fnv_byte(
    std::uint64_t hash,
    std::uint8_t value) noexcept
{
    hash ^= value;
    return hash * 0x100000001b3ULL;
}

bool pmetal_bank_signature(
    const std::uint8_t *base,
    std::uint64_t &signature) noexcept
{
    signature = 0u;
    if (base == nullptr)
        return false;

    std::uint16_t version = 0u;
    std::uint16_t count = 0u;
    if (!safe_read(base + 8u, version) ||
        !safe_read(base + 10u, count) ||
        version != 4u ||
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
            !safe_read(entry + 8u, name_offset) ||
            name_offset < minimum_name ||
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
            std::uint8_t ch = 0u;
            if (!safe_read(
                    base + name_offset + j,
                    ch))
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

    const std::uint8_t *base = nullptr;
    if (!safe_read(
            static_cast<const std::uint8_t *>(
                source) + 0x18u,
            base) ||
        base == nullptr)
        return false;

    std::uint16_t version = 0u;
    std::uint16_t count = 0u;
    if (!safe_read(base + 8u, version) ||
        !safe_read(base + 10u, count) ||
        version != 4u ||
        count == 0u ||
        count > 256u)
        return false;

    const auto index =
        static_cast<std::uint8_t>(
            selector);
    if (static_cast<std::uint32_t>(index) >=
        count)
        return false;

    const auto *entry =
        base + 0x30u +
        static_cast<std::size_t>(index) * 12u;

    if (!safe_read(entry, row_id) ||
        !pmetal_bank_signature(
            base,
            bank_signature))
        return false;

    const auto *bank =
        pmetal_env_source_authority::find_bank(
            bank_signature);
    if (bank == nullptr)
        return false;

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

    return readable_range(record, 0x50u)
        ? record
        : nullptr;
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

bool evaluate_raw_d123(
    const std::uint8_t *a,
    const std::uint8_t *b,
    float beta,
    std::array<
        operators::lightbank::hemdir3_lobe,
        3> &out) noexcept
{
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

bool read_selected_ptde(
    void *source,
    std::int32_t selector,
    f4 &upper,
    f4 &lower) noexcept
{
    if (source == nullptr ||
        selector < 0)
        return false;

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

    const auto *record =
        records +
        static_cast<std::size_t>(
            selector) *
        k_record_stride;

    f4 q_upper{};
    f4 q_lower{};

    if (!safe_read(
            record + k_q_upper_offset,
            q_upper) ||
        !safe_read(
            record + k_q_lower_offset,
            q_lower))
        return false;

    f4 u{};
    f4 l{};

    if (!inverse_q(q_upper.x, u.x) ||
        !inverse_q(q_upper.y, u.y) ||
        !inverse_q(q_upper.z, u.z) ||
        !inverse_q(q_lower.x, l.x) ||
        !inverse_q(q_lower.y, l.y) ||
        !inverse_q(q_lower.z, l.z))
        return false;

    upper = {u.x,u.y,u.z,0.0f};
    lower = {l.x,l.y,l.z,0.0f};
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

bool evaluate_raw_upper_lower(
    const std::uint8_t *a,
    const std::uint8_t *b,
    float beta,
    f4 &upper,
    f4 &lower) noexcept
{
    if (a == nullptr ||
        b == nullptr ||
        !std::isfinite(beta))
        return false;

    raw_rgbm upper_a{};
    raw_rgbm upper_b{};
    raw_rgbm lower_a{};
    raw_rgbm lower_b{};

    if (!safe_read(
            a + k_raw_upper_rgbm_offset,
            upper_a) ||
        !safe_read(
            b + k_raw_upper_rgbm_offset,
            upper_b) ||
        !safe_read(
            a + k_raw_lower_rgbm_offset,
            lower_a) ||
        !safe_read(
            b + k_raw_lower_rgbm_offset,
            lower_b))
        return false;

    upper =
        lerp4(
            decode_rgbm(upper_a),
            decode_rgbm(upper_b),
            beta);
    lower =
        lerp4(
            decode_rgbm(lower_a),
            decode_rgbm(lower_b),
            beta);

    return
        std::isfinite(upper.x) &&
        std::isfinite(upper.y) &&
        std::isfinite(upper.z) &&
        std::isfinite(lower.x) &&
        std::isfinite(lower.y) &&
        std::isfinite(lower.z);
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

    if (!safe_read(
            producer.assignment + 8u,
            selector_a) ||
        !safe_read(
            producer.assignment + 10u,
            selector_b) ||
        !safe_read(
            producer.assignment + 12u,
            beta_bits))
        return;

    try {
        auto fresh =
            std::make_shared<snapshot>();

        fresh->fingerprint = {
            producer.owner,
            selector_a,
            selector_b,
            beta_bits
        };

        fresh->ul_payload[6] =
            producer.upper;
        fresh->ul_payload[7] =
            producer.lower;

        fresh->hemdir3_payload[6] =
            producer.upper;
        fresh->hemdir3_payload[7] =
            producer.lower;
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

        if (producer.have_d123) {
            for (std::size_t i = 0u;
                 i < producer.d123.size();
                 ++i) {
                const auto &lobe =
                    producer.d123[i];

                fresh->hemdir3_payload[i] = {
                    lobe.direction.x,
                    lobe.direction.y,
                    lobe.direction.z,
                    0.0f
                };

                fresh->hemdir3_payload[3u + i] = {
                    lobe.color.x,
                    lobe.color.y,
                    lobe.color.z,
                    0.0f
                };
            }

            ++g_d123_snapshot_publish;
        }

        {
            std::lock_guard<std::mutex> lock(
                g_snapshot_mutex);
            g_snapshots[producer.owner] =
                std::move(fresh);
        }

        ++g_snapshot_publish;
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
    // PERF DIAG N: preserve the exact wrapper detour/trampoline cost while
    // disabling producer TLS, snapshot publication and all semantic capture.
    ++counter;
    return original != nullptr
        ? original(
            rcx,
            owner,
            assignment,
            x)
        : nullptr;
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

void __fastcall hook_steady_packer(
    void *source,
    void *dst,
    std::int32_t selector) noexcept
{
    // PERF DIAG N: detour/trampoline only.
    ++g_steady_seen;
    if (g_steady_packer_orig != nullptr)
        g_steady_packer_orig(
            source,
            dst,
            selector);
}

void *__fastcall hook_blend_packer(
    void *dst,
    void *source_a,
    std::int32_t selector_a,
    void *source_b,
    std::int32_t selector_b,
    float beta) noexcept
{
    // PERF DIAG N: detour/trampoline only.
    ++g_blend_seen;
    return g_blend_packer_orig != nullptr
        ? g_blend_packer_orig(
            dst,
            source_a,
            selector_a,
            source_b,
            selector_b,
            beta)
        : nullptr;
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
        ++g_pmetal_env_blend;
    } else {
        g_producer.have_pmetal_env = false;
        ++g_pmetal_env_miss;
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
            k_rva_steady_packer,
            k_steady_packer_bytes,
            reinterpret_cast<void *>(
                &hook_steady_packer)) ||
        !prepare_hook(
            g_hooks[1],
            k_rva_blend_packer,
            k_blend_packer_bytes,
            reinterpret_cast<void *>(
                &hook_blend_packer))) {
        return false;
    }

    g_wrapper5_orig = nullptr;
    g_wrapper6_orig = nullptr;
    g_steady_packer_orig =
        reinterpret_cast<steady_packer_fn>(
            g_hooks[0].trampoline);
    g_blend_packer_orig =
        reinterpret_cast<lightbank_blend_packer_fn>(
            g_hooks[1].trampoline);

    for (auto &hook : g_hooks)
        if (!arm_hook(hook))
            return false;

    g_pmetal_env_hook_armed.store(false);
    return true;
}

bool restore_producer_hooks() noexcept
{
    bool ok =
        restore_hook(
            g_pmetal_env_hook);

    g_pmetal_env_hook_armed.store(false);

    if (ok)
        g_pmetal_env_blend_orig = nullptr;

    for (auto it = g_hooks.rbegin();
         it != g_hooks.rend();
         ++it)
        ok = restore_hook(*it) && ok;

    if (ok) {
        g_wrapper5_orig = nullptr;
        g_wrapper6_orig = nullptr;
        g_steady_packer_orig = nullptr;
        g_blend_packer_orig = nullptr;
    }

    return ok;
}

ID3D11Buffer *realize_b13(
    const std::shared_ptr<const snapshot> &selected,
    ID3D11Device *device,
    bool hemdir3_combined) noexcept
{
    if (!selected ||
        device == nullptr ||
        (hemdir3_combined &&
         !selected->d123_ready))
        return nullptr;

    std::lock_guard<std::mutex> lock(
        selected->gpu_mutex);

    if (selected->device != nullptr &&
        selected->device != device)
        return nullptr;

    auto *&cached =
        hemdir3_combined
            ? selected->hemdir3_buffer
            : selected->ul_buffer;

    if (cached != nullptr) {
        cached->AddRef();

        if (hemdir3_combined)
            ++g_hemdir3_b13_hit;
        else
            ++g_b13_hit;

        return cached;
    }

    const auto &payload =
        hemdir3_combined
            ? selected->hemdir3_payload
            : selected->ul_payload;

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth =
        static_cast<UINT>(
            sizeof(payload));
    desc.Usage =
        D3D11_USAGE_IMMUTABLE;
    desc.BindFlags =
        D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem =
        payload.data();

    ID3D11Buffer *buffer = nullptr;
    if (FAILED(device->CreateBuffer(
            &desc,
            &init,
            &buffer)) ||
        buffer == nullptr)
        return nullptr;

    if (selected->device == nullptr) {
        selected->device = device;
        device->AddRef();
    }

    cached = buffer;
    buffer->AddRef();

    if (hemdir3_combined)
        ++g_hemdir3_b13_create;
    else
        ++g_b13_create;

    return buffer;
}

void clear_snapshots() noexcept
{
    g_draw_snapshot.reset();

    std::lock_guard<std::mutex> lock(
        g_snapshot_mutex);
    g_snapshots.clear();
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
    // PERF DIAG N: selector join is already runtime-exonerated by K and is
    // disabled here so this build measures only raw native detour overhead.
    (void)owner;
    (void)return_address;
    (void)r14;
    (void)r15;
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
    ++g_selector_seen;
    g_draw_snapshot.reset();

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
        ++g_selector_miss;
        return;
    }

    std::uint16_t selector_a = 0u;
    std::uint16_t selector_b = 0u;
    std::uint32_t beta_bits = 0u;

    if (!safe_read(
            descriptor + 0x4Cu,
            selector_a) ||
        !safe_read(
            descriptor + 0x4Eu,
            selector_b) ||
        !safe_read(
            descriptor + 0x50u,
            beta_bits)) {
        ++g_selector_miss;
        return;
    }

    std::shared_ptr<const snapshot> selected{};

    {
        std::lock_guard<std::mutex> lock(
            g_snapshot_mutex);

        const auto found =
            g_snapshots.find(
                reinterpret_cast<std::uintptr_t>(
                    owner));

        if (found != g_snapshots.end())
            selected = found->second;
    }

    if (!selected) {
        ++g_selector_miss;
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
        ++g_tuple_mismatch;
        return;
    }

    g_draw_snapshot =
        std::move(selected);
    ++g_selector_match;
}

bool upper_lower_draw_runtime::prepare_upper_lower_carrier(
    ID3D11DeviceContext *context,
    prepared_upper_lower_draw &prepared) noexcept
{
    prepared = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        !core_.features().enabled(
            core::operator_id::upper_lower) ||
        context == nullptr ||
        !g_draw_snapshot)
        return false;

    ID3D11Device *device = nullptr;
    context->GetDevice(&device);

    if (device == nullptr)
        return false;

    auto *b13 =
        realize_b13(
            g_draw_snapshot,
            device,
            false);

    device->Release();

    if (b13 == nullptr)
        return false;

    // Producer-only contract: realize and retain the exact fresh b13
    // payload for consumer constant-buffer slot 13u. Receiver/shader ownership
    // is established by the consumer
    // adapter (ordinary HemEnv, MR+U/L, or another explicitly certified
    // consumer), never here.
    prepared.b13 = b13;
    prepared.ready = true;
    ++g_requests;
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
        context == nullptr ||
        !g_draw_snapshot ||
        !g_draw_snapshot->d123_ready)
        return false;

    ID3D11Device *device = nullptr;
    context->GetDevice(&device);
    if (device == nullptr)
        return false;

    auto *b13 =
        realize_b13(
            g_draw_snapshot,
            device,
            true);

    device->Release();

    if (b13 == nullptr)
        return false;

    prepared.b13 = b13;
    prepared.fingerprint =
        g_draw_snapshot->fingerprint;
    prepared.d123_ready = true;
    prepared.upper_lower_ready = true;
    prepared.ready = true;
    ++g_hemdir3_carrier_requests;
    return true;
}

void upper_lower_draw_runtime::release_hemdir3_carrier(
    prepared_hemdir3_carrier &prepared) noexcept
{
    if (prepared.b13 != nullptr)
        prepared.b13->Release();

    prepared = {};
}

bool upper_lower_draw_runtime::selected_pmetal_env_source(
    pmetal_env_source &out) const noexcept
{
    out = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        !g_pmetal_env_hook_armed.load() ||
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
    if (prepared.b13 != nullptr)
        prepared.b13->Release();

    prepared = {};
}

void upper_lower_draw_runtime::consume_draw_selection() noexcept
{
    g_draw_snapshot.reset();
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
        g_enabled.load(),
        g_pmetal_env_hook_armed.load(),
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
}

} // namespace dsrrl::runtime
