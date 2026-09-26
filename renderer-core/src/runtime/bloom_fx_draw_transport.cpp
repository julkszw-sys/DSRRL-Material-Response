#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/bloom_fx_draw_transport.hpp"

#include "dsrrl/runtime/flver_identity_transport.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl::runtime::bloom_fx_draw_transport {
namespace {

constexpr std::uintptr_t k_particle_draw_rva = 0x00FFCF30u;
constexpr std::uintptr_t k_cluster_draw_rva = 0x00FFDCE0u;

constexpr std::uintptr_t k_particle_vtable_rva = 0x0151C930u;
constexpr std::uintptr_t k_cluster_vtable_rva = 0x0151CBF8u;
constexpr std::uintptr_t k_particle_state_vtable_rva = 0x015F8B18u;
constexpr std::uintptr_t k_cluster_state_vtable_rva = 0x015F8B98u;

// Relocation-free prefix shared by both index-10 draw callbacks:
//   sub rsp,58h
//   mov qword ptr [rsp+20h],-2
//   mov rax,[rcx+30h]
constexpr std::array<std::uint8_t,17> k_draw_prefix = {
    0x48,0x83,0xEC,0x58,
    0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,
    0x48,0x8B,0x41,0x30
};

struct hook {
    void *target = nullptr;
    void *trampoline = nullptr;
    void *detour = nullptr;
    std::size_t stolen = 0;
    std::array<std::uint8_t,32> original{};
    bool patched = false;
};

using draw_fn =
    void (__fastcall *)(void *, void *, std::uint64_t);

struct tls_state {
    fx_draw_snapshot snapshot{};
};

std::uintptr_t g_base = 0u;
hook g_particle{};
hook g_cluster{};
draw_fn g_particle_original = nullptr;
draw_fn g_cluster_original = nullptr;
telemetry g_state{};
thread_local tls_state g_tls{};

std::atomic<std::uint64_t> g_particle_events{0};
std::atomic<std::uint64_t> g_cluster_events{0};
std::atomic<std::uint64_t> g_exact_entity_hits{0};
std::atomic<std::uint64_t> g_entity_rejects{0};
std::atomic<std::uint64_t> g_state_ready_hits{0};
std::atomic<std::uint64_t> g_state_missing{0};
std::atomic<std::uint64_t> g_exact_state_hits{0};
std::atomic<std::uint64_t> g_state_vtable_rejects{0};
std::atomic<std::uint64_t> g_source_links_ready{0};
std::atomic<std::uint64_t> g_source_links_missing{0};
std::atomic<std::uint64_t> g_snapshot_hits{0};
std::atomic<std::uint64_t> g_snapshot_misses{0};
std::atomic<std::uint64_t> g_generation{0};

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

    if (!readable_range(target,N) ||
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
            0u,
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
                0u,
                MEM_RELEASE) != FALSE;

        if (ok)
            h.trampoline = nullptr;
    }

    if (ok)
        h = {};

    return ok;
}

void observe(
    fx_draw_entity_kind kind,
    void *entity,
    void *draw_context,
    std::uint64_t mode_token) noexcept
{
    if (g_state.quarantined)
        return;

    if (kind ==
        fx_draw_entity_kind::particle)
        ++g_particle_events;
    else if (kind ==
             fx_draw_entity_kind::cluster)
        ++g_cluster_events;

    fx_draw_snapshot snap{};
    snap.generation =
        g_generation.fetch_add(1u) + 1u;
    snap.kind = kind;
    snap.entity = entity;
    snap.draw_context = draw_context;
    snap.mode_token =
        static_cast<std::uint32_t>(
            mode_token);

    if (!readable_range(entity,0x38u)) {
        ++g_entity_rejects;
        g_tls.snapshot = {};
        return;
    }

    std::uintptr_t vtable = 0u;
    void *appearance_state = nullptr;
    std::memcpy(
        &vtable,
        entity,
        sizeof(vtable));
    std::memcpy(
        &appearance_state,
        static_cast<const std::uint8_t *>(
            entity) + 0x30u,
        sizeof(appearance_state));

    const std::uintptr_t expected_vtable =
        g_base +
        (kind == fx_draw_entity_kind::particle
            ? k_particle_vtable_rva
            : k_cluster_vtable_rva);

    snap.exact_entity_vtable =
        vtable == expected_vtable;

    if (!snap.exact_entity_vtable) {
        ++g_entity_rejects;
        g_tls.snapshot = {};
        return;
    }

    ++g_exact_entity_hits;

    snap.appearance_state =
        appearance_state;

    const std::size_t state_probe_size =
        kind == fx_draw_entity_kind::particle
            ? 0x40u
            : 0x68u;

    snap.appearance_state_ready =
        appearance_state != nullptr &&
        readable_range(
            appearance_state,
            state_probe_size);

    if (!snap.appearance_state_ready) {
        ++g_state_missing;
        g_tls.snapshot = snap;
        return;
    }

    ++g_state_ready_hits;

    std::uintptr_t state_vtable = 0u;
    std::memcpy(
        &state_vtable,
        appearance_state,
        sizeof(state_vtable));

    const std::uintptr_t expected_state_vtable =
        g_base +
        (kind == fx_draw_entity_kind::particle
            ? k_particle_state_vtable_rva
            : k_cluster_state_vtable_rva);

    snap.exact_appearance_vtable =
        state_vtable == expected_state_vtable;

    if (!snap.exact_appearance_vtable) {
        ++g_state_vtable_rejects;
        g_tls.snapshot = snap;
        return;
    }

    ++g_exact_state_hits;

    const auto *state_bytes =
        static_cast<const std::uint8_t *>(
            appearance_state);

    const std::size_t primary_offset =
        kind == fx_draw_entity_kind::particle
            ? 0x30u
            : 0x50u;
    const std::size_t secondary_offset =
        kind == fx_draw_entity_kind::particle
            ? 0x38u
            : 0x58u;

    std::memcpy(
        &snap.appearance_source_primary,
        state_bytes + primary_offset,
        sizeof(snap.appearance_source_primary));
    std::memcpy(
        &snap.appearance_source_secondary,
        state_bytes + secondary_offset,
        sizeof(snap.appearance_source_secondary));

    if (kind == fx_draw_entity_kind::cluster) {
        std::memcpy(
            &snap.appearance_semantic_word,
            state_bytes + 0x60u,
            sizeof(snap.appearance_semantic_word));
    }

    snap.source_links_ready =
        snap.appearance_source_primary != nullptr &&
        snap.appearance_source_secondary != nullptr;

    if (snap.source_links_ready)
        ++g_source_links_ready;
    else
        ++g_source_links_missing;

    snap.ready =
        snap.exact_entity_vtable &&
        snap.exact_appearance_vtable &&
        snap.appearance_state_ready &&
        snap.source_links_ready &&
        draw_context != nullptr;

    g_tls.snapshot = snap;
}

void __fastcall particle_entry(
    void *entity,
    void *draw_context,
    std::uint64_t mode_token) noexcept
{
    observe(
        fx_draw_entity_kind::particle,
        entity,
        draw_context,
        mode_token);

    if (g_particle_original != nullptr)
        g_particle_original(
            entity,
            draw_context,
            mode_token);
}

void __fastcall cluster_entry(
    void *entity,
    void *draw_context,
    std::uint64_t mode_token) noexcept
{
    observe(
        fx_draw_entity_kind::cluster,
        entity,
        draw_context,
        mode_token);

    if (g_cluster_original != nullptr)
        g_cluster_original(
            entity,
            draw_context,
            mode_token);
}

} // namespace

bool install() noexcept
{
    if (g_particle.patched ||
        g_cluster.patched)
        return false;

    g_state = {};

    const auto provenance =
        flver_identity_transport::status();

    // Reuse the full-file retail SHA provenance already established by the
    // source-complete FLVER hook layer. FX then adds its own exact byte and
    // vtable attestation at the homologous draw-entity semantic cut.
    if (!provenance.provenance_ok)
        return false;

    g_base =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));

    if (g_base == 0u)
        return false;

    g_state.provenance_ok = true;

    if (!prepare_hook(
            g_particle,
            k_particle_draw_rva,
            k_draw_prefix,
            reinterpret_cast<void *>(
                &particle_entry)) ||
        !prepare_hook(
            g_cluster,
            k_cluster_draw_rva,
            k_draw_prefix,
            reinterpret_cast<void *>(
                &cluster_entry)))
        goto fail;

    g_particle_original =
        reinterpret_cast<draw_fn>(
            g_particle.trampoline);
    g_cluster_original =
        reinterpret_cast<draw_fn>(
            g_cluster.trampoline);

    if (!arm_hook(g_cluster) ||
        !arm_hook(g_particle))
        goto fail;

    g_state.particle_hook_armed = true;
    g_state.cluster_hook_armed = true;
    return true;

fail:
    uninstall();
    return false;
}

void uninstall() noexcept
{
    const bool particle_ok =
        restore_hook(g_particle);
    const bool cluster_ok =
        restore_hook(g_cluster);

    if (!particle_ok ||
        !cluster_ok) {
        g_state.restore_failed = true;
        g_state.quarantined = true;
        g_state.particle_hook_armed =
            g_particle.patched;
        g_state.cluster_hook_armed =
            g_cluster.patched;
        return;
    }

    g_particle_original = nullptr;
    g_cluster_original = nullptr;
    g_base = 0u;
    g_tls = {};
    g_state = {};
}

bool snapshot(
    fx_draw_snapshot &out) noexcept
{
    out = {};

    if (!g_tls.snapshot.ready ||
        g_state.quarantined) {
        ++g_snapshot_misses;
        return false;
    }

    out = g_tls.snapshot;
    ++g_snapshot_hits;
    return true;
}

void consume() noexcept
{
    g_tls.snapshot = {};
}

telemetry status() noexcept
{
    auto out = g_state;
    out.particle_events =
        g_particle_events.load();
    out.cluster_events =
        g_cluster_events.load();
    out.exact_entity_hits =
        g_exact_entity_hits.load();
    out.entity_rejects =
        g_entity_rejects.load();
    out.state_ready_hits =
        g_state_ready_hits.load();
    out.state_missing =
        g_state_missing.load();
    out.exact_state_hits =
        g_exact_state_hits.load();
    out.state_vtable_rejects =
        g_state_vtable_rejects.load();
    out.source_links_ready =
        g_source_links_ready.load();
    out.source_links_missing =
        g_source_links_missing.load();
    out.snapshot_hits =
        g_snapshot_hits.load();
    out.snapshot_misses =
        g_snapshot_misses.load();
    return out;
}

void reset_stats() noexcept
{
    g_particle_events.store(0u);
    g_cluster_events.store(0u);
    g_exact_entity_hits.store(0u);
    g_entity_rejects.store(0u);
    g_state_ready_hits.store(0u);
    g_state_missing.store(0u);
    g_exact_state_hits.store(0u);
    g_state_vtable_rejects.store(0u);
    g_source_links_ready.store(0u);
    g_source_links_missing.store(0u);
    g_snapshot_hits.store(0u);
    g_snapshot_misses.store(0u);
    g_generation.store(0u);
    g_tls = {};
}

} // namespace dsrrl::runtime::bloom_fx_draw_transport
