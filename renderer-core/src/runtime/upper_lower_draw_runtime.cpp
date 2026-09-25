#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/upper_lower_draw_runtime.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"
#include "dsrrl/operators/lightbank/snapshot_freshness.hpp"

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
    alignas(16) std::array<f4,8> payload{};
    mutable std::mutex gpu_mutex;
    mutable ID3D11Device *device = nullptr;
    mutable ID3D11Buffer *buffer = nullptr;

    ~snapshot()
    {
        if (buffer != nullptr)
            buffer->Release();
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
    f4 upper{};
    f4 lower{};
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
using blend_fn =
    void *(__fastcall *)(void *,const void *,const void *,float);
using steady_packer_fn =
    void (__fastcall *)(void *,void *,std::int32_t);

constexpr std::uintptr_t k_rva_wrapper_type5 = 0x1C0BE0u;
constexpr std::uintptr_t k_rva_wrapper_type6 = 0x1C0C10u;
constexpr std::uintptr_t k_rva_blend_helper = 0x5642F0u;
constexpr std::uintptr_t k_rva_steady_packer = 0x563B80u;

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

constexpr std::size_t k_record_stride = 0x110u;
constexpr std::size_t k_q_upper_offset = 0x60u;
constexpr std::size_t k_q_lower_offset = 0x70u;
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
std::array<inline_hook,4> g_hooks{};

wrapper_fn g_wrapper5_orig = nullptr;
wrapper_fn g_wrapper6_orig = nullptr;
blend_fn g_blend_orig = nullptr;
steady_packer_fn g_steady_packer_orig = nullptr;

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
std::atomic<std::uint64_t> g_snapshot_publish{0};
std::atomic<std::uint64_t> g_selector_seen{0};
std::atomic<std::uint64_t> g_selector_match{0};
std::atomic<std::uint64_t> g_selector_miss{0};
std::atomic<std::uint64_t> g_tuple_mismatch{0};
std::atomic<std::uint64_t> g_b13_create{0};
std::atomic<std::uint64_t> g_b13_hit{0};
std::atomic<std::uint64_t> g_requests{0};

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

        fresh->payload[6] =
            producer.upper;
        fresh->payload[7] =
            producer.lower;

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
    ++counter;

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

    if (!completed.have_upper ||
        !completed.have_lower) {
        std::lock_guard<std::mutex> lock(
            g_snapshot_mutex);
        g_snapshots.erase(
            completed.owner);
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

void __fastcall hook_steady_packer(
    void *source,
    void *dst,
    std::int32_t selector) noexcept
{
    ++g_steady_seen;

    if (g_steady_packer_orig != nullptr)
        g_steady_packer_orig(
            source,
            dst,
            selector);

    if (!g_producer.active)
        return;

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
    ++g_steady_pass;
}

void *__fastcall hook_blend(
    void *dst,
    const void *a,
    const void *b,
    float beta) noexcept
{
    ++g_blend_seen;

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
                    ++g_blend_upper;
                } else {
                    g_producer.lower = value;
                    g_producer.have_lower = true;
                    ++g_blend_lower;
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
                &hook_steady_packer))) {
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

    for (auto &hook : g_hooks)
        if (!arm_hook(hook))
            return false;

    return true;
}

bool restore_producer_hooks() noexcept
{
    bool ok = true;

    for (auto it = g_hooks.rbegin();
         it != g_hooks.rend();
         ++it)
        ok = restore_hook(*it) && ok;

    if (ok) {
        g_wrapper5_orig = nullptr;
        g_wrapper6_orig = nullptr;
        g_blend_orig = nullptr;
        g_steady_packer_orig = nullptr;
    }

    return ok;
}

ID3D11Buffer *realize_b13(
    const std::shared_ptr<const snapshot> &selected,
    ID3D11Device *device) noexcept
{
    if (!selected ||
        device == nullptr)
        return nullptr;

    std::lock_guard<std::mutex> lock(
        selected->gpu_mutex);

    if (selected->buffer != nullptr) {
        if (selected->device != device)
            return nullptr;

        selected->buffer->AddRef();
        ++g_b13_hit;
        return selected->buffer;
    }

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = 128u;
    desc.Usage =
        D3D11_USAGE_IMMUTABLE;
    desc.BindFlags =
        D3D11_BIND_CONSTANT_BUFFER;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem =
        selected->payload.data();

    ID3D11Buffer *buffer = nullptr;
    if (FAILED(device->CreateBuffer(
            &desc,
            &init,
            &buffer)) ||
        buffer == nullptr)
        return nullptr;

    selected->device = device;
    device->AddRef();

    selected->buffer = buffer;
    buffer->AddRef();
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
    if (g_runtime != nullptr)
        g_runtime->selector_event(
            owner,
            return_address,
            r14,
            r15);
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

bool upper_lower_draw_runtime::prepare_draw_request(
    ID3D11DeviceContext *context,
    std::uint32_t receiver_id,
    prepared_upper_lower_draw &prepared) noexcept
{
    prepared = {};

    if (!g_enabled.load() ||
        g_quarantined.load() ||
        !core_.features().enabled(
            core::operator_id::upper_lower) ||
        context == nullptr ||
        receiver_id < 24u ||
        receiver_id > 47u ||
        !g_draw_snapshot)
        return false;

    ID3D11Device *device = nullptr;
    context->GetDevice(&device);

    if (device == nullptr)
        return false;

    auto *b13 =
        realize_b13(
            g_draw_snapshot,
            device);

    device->Release();

    if (b13 == nullptr)
        return false;

    prepared.b13 = b13;
    prepared.request.primary =
        core::operator_id::upper_lower;
    prepared.request.receiver_verified = true;
    prepared.request.material_verified = false;
    prepared.request.constant_buffers[0] = {
        13u,
        b13
    };
    prepared.request.constant_buffer_count = 1u;

    draw_tx_mutation verify{};
    if (build_island_draw_mutation(
            prepared.request,
            verify) !=
        island_draw_adapter_result::ready) {
        release_prepared_draw(prepared);
        return false;
    }

    prepared.ready = true;
    ++g_requests;
    return true;
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
        g_snapshot_publish.load(),
        g_selector_seen.load(),
        g_selector_match.load(),
        g_selector_miss.load(),
        g_tuple_mismatch.load(),
        g_b13_create.load(),
        g_b13_hit.load(),
        g_requests.load(),
        g_enabled.load(),
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
    g_snapshot_publish.store(0);
    g_selector_seen.store(0);
    g_selector_match.store(0);
    g_selector_miss.store(0);
    g_tuple_mismatch.store(0);
    g_b13_create.store(0);
    g_b13_hit.store(0);
    g_requests.store(0);
}

} // namespace dsrrl::runtime
