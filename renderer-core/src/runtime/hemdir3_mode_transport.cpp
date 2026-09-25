#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/hemdir3_mode_transport.hpp"
#include "dsrrl/runtime/flver_identity_transport.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" void dsrrl_hemdir3_mode_lt5_hook_entry();
extern "C" void dsrrl_hemdir3_selector_end_hook_entry();

extern "C" {
void *g_dsrrl_hemdir3_mode_lt5_trampoline = nullptr;
void *g_dsrrl_hemdir3_selector_end_trampoline = nullptr;
}

namespace dsrrl::runtime::hemdir3_mode_transport {
namespace {

constexpr std::uintptr_t k_lt5_rva = 0x295F9Fu;
constexpr std::uintptr_t k_selector_end_rva = 0x22BA79u;

constexpr std::array<std::uint8_t,17> k_lt5_bytes = {
    0x48,0x63,0xCA,
    0x41,0x8B,0xC3,
    0x48,0x8D,0x0C,0x48,
    0x49,0x63,0xC1,
    0x48,0x8D,0x14,0x49
};

constexpr std::array<std::uint8_t,18> k_selector_end_bytes = {
    0x44,0x0F,0xB6,0x84,0x24,0x88,0x00,0x00,0x00,
    0x8B,0xD0,
    0x48,0x8B,0xCB,
    0x48,0x83,0xC4,0x30
};

struct hook {
    void *target = nullptr;
    void *trampoline = nullptr;
    void *detour = nullptr;
    std::size_t stolen = 0;
    std::array<std::uint8_t,32> original{};
    bool patched = false;
};

struct tls_snapshot {
    std::uint64_t generation = 0;
    std::uint32_t incoming_mode = 0;
    std::uint32_t effective_mode = 0;
    bool capture_active = false;
    bool ready = false;
};

std::uintptr_t g_base = 0u;
hook g_lt5{};
hook g_selector_end{};
telemetry g_state{};
thread_local tls_snapshot g_tls{};

std::atomic<std::uint64_t> g_selector_begin{0};
std::atomic<std::uint64_t> g_incoming_mode2{0};
std::atomic<std::uint64_t> g_effective_observed{0};
std::atomic<std::uint64_t> g_mode2_observed{0};
std::atomic<std::uint64_t> g_snapshot_hits{0};
std::atomic<std::uint64_t> g_snapshot_misses{0};

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
    hook &out,
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

    out.target = target;
    out.trampoline = trampoline;
    out.detour = detour;
    out.stolen = N;

    std::copy(
        expected.begin(),
        expected.end(),
        out.original.begin());

    return true;
}

bool arm_hook(hook &h) noexcept
{
    if (h.target == nullptr ||
        h.trampoline == nullptr ||
        h.detour == nullptr ||
        h.stolen < 14u ||
        h.stolen > h.original.size())
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
            h.detour);
    std::memcpy(
        patch.data() + 6u,
        &detour,
        sizeof(detour));

    h.patched = true;
    return write_bytes(
        h.target,
        patch.data(),
        h.stolen);
}

bool restore_hook(hook &h) noexcept
{
    bool ok = true;

    if (h.patched) {
        ok =
            h.target != nullptr &&
            h.stolen != 0u &&
            write_bytes(
                h.target,
                h.original.data(),
                h.stolen);

        if (ok)
            ok =
                std::memcmp(
                    h.target,
                    h.original.data(),
                    h.stolen) == 0;

        if (ok)
            h.patched = false;
    }

    if (ok &&
        h.trampoline != nullptr) {
        ok =
            VirtualFree(
                h.trampoline,
                0,
                MEM_RELEASE) != FALSE;

        if (ok)
            h.trampoline = nullptr;
    }

    if (ok)
        h = {};

    return ok;
}

void observe_effective_mode(
    std::uint32_t effective_mode) noexcept
{
    if (!g_tls.capture_active)
        return;

    g_tls.effective_mode =
        effective_mode;
    g_tls.ready = true;
    g_tls.capture_active = false;

    ++g_effective_observed;

    if (effective_mode == 2u)
        ++g_mode2_observed;
}

void selector_end() noexcept
{
    // A mode2 incoming selector that did not pass the <5 dispatch branch was
    // overridden into the >=5 family. Do not let that unfinished capture leak
    // into any later selector call on this thread.
    g_tls.capture_active = false;
}

} // namespace

extern "C" void
dsrrl_hemdir3_effective_mode_observer(
    std::uint32_t effective_mode) noexcept
{
    observe_effective_mode(
        effective_mode);
}

extern "C" void
dsrrl_hemdir3_selector_end_observer() noexcept
{
    selector_end();
}

bool install() noexcept
{
    if (g_lt5.patched ||
        g_selector_end.patched)
        return false;

    g_state = {};

    const auto selector =
        flver_identity_transport::status();

    if (!selector.provenance_ok ||
        !selector.selector_armed)
        return false;

    g_base =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));

    if (g_base == 0u)
        return false;

    g_state.provenance_ok = true;

    if (!prepare_hook(
            g_lt5,
            k_lt5_rva,
            k_lt5_bytes,
            reinterpret_cast<void *>(
                &dsrrl_hemdir3_mode_lt5_hook_entry)) ||
        !prepare_hook(
            g_selector_end,
            k_selector_end_rva,
            k_selector_end_bytes,
            reinterpret_cast<void *>(
                &dsrrl_hemdir3_selector_end_hook_entry)))
        goto fail;

    g_dsrrl_hemdir3_mode_lt5_trampoline =
        g_lt5.trampoline;
    g_dsrrl_hemdir3_selector_end_trampoline =
        g_selector_end.trampoline;

    if (!arm_hook(g_selector_end) ||
        !arm_hook(g_lt5))
        goto fail;

    g_state.lt5_hook_armed = true;
    g_state.selector_end_hook_armed = true;
    return true;

fail:
    uninstall();
    return false;
}

void uninstall() noexcept
{
    const bool lt5_ok =
        restore_hook(g_lt5);
    const bool end_ok =
        restore_hook(g_selector_end);

    if (!lt5_ok || !end_ok) {
        g_state.restore_failed = true;
        g_state.quarantined = true;
        g_state.lt5_hook_armed =
            g_lt5.patched;
        g_state.selector_end_hook_armed =
            g_selector_end.patched;
        return;
    }

    g_dsrrl_hemdir3_mode_lt5_trampoline =
        nullptr;
    g_dsrrl_hemdir3_selector_end_trampoline =
        nullptr;

    g_base = 0u;
    g_tls = {};
    g_state = {};
}

void selector_begin(
    std::uint32_t incoming_mode) noexcept
{
    ++g_selector_begin;

    ++g_tls.generation;
    g_tls.incoming_mode =
        incoming_mode;
    g_tls.effective_mode = 0u;
    g_tls.ready = false;

    const bool candidate =
        incoming_mode == 2u;

    if (candidate)
        ++g_incoming_mode2;

    g_tls.capture_active =
        candidate &&
        g_state.lt5_hook_armed &&
        g_state.selector_end_hook_armed &&
        !g_state.quarantined;
}

bool snapshot(
    std::uint32_t &effective_mode) noexcept
{
    effective_mode = 0u;

    if (!g_tls.ready ||
        g_state.quarantined) {
        ++g_snapshot_misses;
        return false;
    }

    effective_mode =
        g_tls.effective_mode;
    ++g_snapshot_hits;
    return true;
}

void consume_draw_selection() noexcept
{
    g_tls.capture_active = false;
    g_tls.ready = false;
    g_tls.incoming_mode = 0u;
    g_tls.effective_mode = 0u;
}

telemetry status() noexcept
{
    auto out = g_state;

    out.selector_begin =
        g_selector_begin.load();
    out.incoming_mode2 =
        g_incoming_mode2.load();
    out.effective_observed =
        g_effective_observed.load();
    out.mode2_observed =
        g_mode2_observed.load();
    out.snapshot_hits =
        g_snapshot_hits.load();
    out.snapshot_misses =
        g_snapshot_misses.load();

    return out;
}

void reset_stats() noexcept
{
    g_selector_begin.store(0u);
    g_incoming_mode2.store(0u);
    g_effective_observed.store(0u);
    g_mode2_observed.store(0u);
    g_snapshot_hits.store(0u);
    g_snapshot_misses.store(0u);
    g_tls = {};
}

} // namespace dsrrl::runtime::hemdir3_mode_transport
