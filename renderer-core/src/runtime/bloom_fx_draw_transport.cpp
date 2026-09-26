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
#include <mutex>
#include <unordered_map>

namespace dsrrl::runtime::bloom_fx_draw_transport {
namespace {

namespace pp = dsrrl::operators::postprocess;

constexpr std::uintptr_t k_particle_draw_rva = 0x00FFCF30u;
constexpr std::uintptr_t k_cluster_draw_rva = 0x00FFDCE0u;

constexpr std::uintptr_t k_particle_vtable_rva = 0x0151C930u;
constexpr std::uintptr_t k_cluster_vtable_rva = 0x0151CBF8u;
constexpr std::uintptr_t k_particle_state_vtable_rva = 0x015F8B18u;
constexpr std::uintptr_t k_cluster_state_vtable_rva = 0x015F8B98u;
constexpr std::uintptr_t k_particle_model_ctor_rva = 0x004FE7B0u;
constexpr std::uintptr_t k_particle_model_dtor_rva = 0x004FED90u;
constexpr std::uintptr_t k_particle_state_update_rva = 0x0118CCF0u;
constexpr std::uintptr_t k_semantic_index_getter_rva = 0x001BBC00u;

// Relocation-free prefix shared by both index-10 draw callbacks:
//   sub rsp,58h
//   mov qword ptr [rsp+20h],-2
//   mov rax,[rcx+30h]
constexpr std::array<std::uint8_t,17> k_draw_prefix = {
    0x48,0x83,0xEC,0x58,
    0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF,
    0x48,0x8B,0x41,0x30
};

constexpr std::array<std::uint8_t,22> k_particle_model_ctor_prefix = {
    0x48,0x89,0x4C,0x24,0x08,
    0x56,
    0x57,
    0x41,0x56,
    0x48,0x83,0xEC,0x30,
    0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF
};

constexpr std::array<std::uint8_t,15> k_particle_model_dtor_prefix = {
    0x48,0x89,0x5C,0x24,0x08,
    0x57,
    0x48,0x83,0xEC,0x20,
    0x8B,0xDA,
    0x48,0x8B,0xF9
};

constexpr std::array<std::uint8_t,16> k_particle_state_update_prefix = {
    0x48,0x8B,0xC4,
    0x55,
    0x57,
    0x48,0x81,0xEC,0xD8,0x00,0x00,0x00,
    0x48,0x89,0x70,0x18
};

constexpr std::array<std::uint8_t,18> k_semantic_index_getter_bytes = {
    0x48,0x8B,0x05,0x69,0xF3,0xAA,0x01,
    0x48,0x63,0xD1,
    0x8B,0x84,0x90,0x98,0x38,0x00,0x00,
    0xC3
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
using particle_model_ctor_fn =
    void *(__fastcall *)(void *, void *, void *, void *, void *);
using particle_model_dtor_fn =
    void *(__fastcall *)(void *, std::uint32_t);
using particle_state_update_fn =
    bool (__fastcall *)(void *, void *, void *);
using semantic_index_getter_fn =
    std::uint32_t (__fastcall *)(std::uint32_t);

struct particle_model_record {
    void *arg2 = nullptr;
    void *arg3 = nullptr;
    void *arg4 = nullptr;
    void *arg5 = nullptr;
    std::uint64_t generation = 0;
    pp::waterwave_authored_identity waterwave_identity{};
    bool waterwave_identity_exact = false;
};

struct backend_semantic_record {
    std::uint32_t backend_key = 0u;
    std::uint32_t waterwave_runtime_index = 0u;
    std::uint64_t generation = 0u;
    bool key_observed = false;
    bool matches_waterwave = false;
};

struct tls_state {
    fx_draw_snapshot snapshot{};
};

std::uintptr_t g_base = 0u;
hook g_particle{};
hook g_cluster{};
hook g_particle_model_ctor{};
hook g_particle_model_dtor{};
hook g_particle_state_update{};
draw_fn g_particle_original = nullptr;
draw_fn g_cluster_original = nullptr;
particle_model_ctor_fn g_particle_model_ctor_original = nullptr;
particle_model_dtor_fn g_particle_model_dtor_original = nullptr;
particle_state_update_fn g_particle_state_update_original = nullptr;
semantic_index_getter_fn g_semantic_index_getter = nullptr;
telemetry g_state{};
thread_local tls_state g_tls{};
std::mutex g_model_mutex;
std::unordered_map<std::uintptr_t,particle_model_record> g_particle_models;
std::unordered_map<std::uintptr_t,backend_semantic_record>
    g_backend_semantics;
constexpr std::size_t k_particle_model_registry_cap = 4096u;
constexpr std::size_t k_backend_semantic_registry_cap = 4096u;

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
std::atomic<std::uint64_t> g_particle_model_ctor_events{0};
std::atomic<std::uint64_t> g_particle_model_dtor_events{0};
std::atomic<std::uint64_t> g_particle_model_join_hits{0};
std::atomic<std::uint64_t> g_particle_model_join_misses{0};
std::atomic<std::uint64_t> g_particle_model_owner_join_hits{0};
std::atomic<std::uint64_t> g_particle_model_source_primary_join_hits{0};
std::atomic<std::uint64_t> g_particle_model_source_secondary_join_hits{0};
std::atomic<std::uint64_t> g_particle_state_update_events{0};
std::atomic<std::uint64_t> g_backend_key_reads{0};
std::atomic<std::uint64_t> g_backend_key_read_failures{0};
std::atomic<std::uint64_t> g_waterwave_runtime_index_reads{0};
std::atomic<std::uint64_t> g_backend_key_waterwave_matches{0};
std::atomic<std::uint64_t> g_waterwave_semantic_model_candidate_hits{0};
std::atomic<std::uint64_t> g_backend_semantic_snapshot_hits{0};
std::atomic<std::uint64_t> g_backend_semantic_snapshot_misses{0};
std::atomic<std::uint64_t> g_waterwave_publish_ok{0};
std::atomic<std::uint64_t> g_waterwave_publish_fail{0};
std::atomic<std::uint64_t> g_waterwave_same_instance_hits{0};
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

void register_particle_model(
    void *instance,
    void *arg2,
    void *arg3,
    void *arg4,
    void *arg5) noexcept
{
    if (instance == nullptr)
        return;

    ++g_particle_model_ctor_events;

    particle_model_record record{};
    record.arg2 = arg2;
    record.arg3 = arg3;
    record.arg4 = arg4;
    record.arg5 = arg5;
    record.generation =
        g_generation.fetch_add(1u) + 1u;

    std::lock_guard<std::mutex> lock(g_model_mutex);

    if (g_particle_models.size() >=
            k_particle_model_registry_cap &&
        g_particle_models.find(
            reinterpret_cast<std::uintptr_t>(
                instance)) ==
            g_particle_models.end()) {
        g_particle_models.clear();
    }

    g_particle_models[
        reinterpret_cast<std::uintptr_t>(
            instance)] = record;
}

bool join_particle_model(
    fx_draw_snapshot &snap) noexcept
{
    if (snap.kind !=
            fx_draw_entity_kind::particle ||
        !snap.appearance_state_ready)
        return false;

    struct candidate {
        void *ptr = nullptr;
        fx_particle_model_join_channel channel =
            fx_particle_model_join_channel::none;
    };

    const candidate candidates[] = {
        {
            snap.appearance_owner,
            fx_particle_model_join_channel::appearance_owner
        },
        {
            snap.appearance_source_primary,
            fx_particle_model_join_channel::appearance_source_primary
        },
        {
            snap.appearance_source_secondary,
            fx_particle_model_join_channel::appearance_source_secondary
        }
    };

    std::lock_guard<std::mutex> lock(g_model_mutex);

    for (const auto &candidate_value : candidates) {
        const auto candidate_key =
            reinterpret_cast<std::uintptr_t>(
                candidate_value.ptr);
        if (candidate_key == 0u)
            continue;

        const auto found =
            g_particle_models.find(candidate_key);
        if (found ==
            g_particle_models.end())
            continue;

        snap.particle_model_instance =
            candidate_value.ptr;
        snap.particle_model_join_channel =
            candidate_value.channel;
        snap.particle_model_arg2 =
            found->second.arg2;
        snap.particle_model_arg3 =
            found->second.arg3;
        snap.particle_model_arg4 =
            found->second.arg4;
        snap.particle_model_arg5 =
            found->second.arg5;
        snap.particle_model_generation =
            found->second.generation;
        snap.particle_model_instance_join = true;
        snap.waterwave_authored_identity_exact =
            found->second.waterwave_identity_exact;
        snap.waterwave_same_model_instance =
            found->second.waterwave_identity_exact &&
            found->second.generation != 0u;

        switch (candidate_value.channel) {
        case fx_particle_model_join_channel::appearance_owner:
            ++g_particle_model_owner_join_hits;
            break;
        case fx_particle_model_join_channel::appearance_source_primary:
            ++g_particle_model_source_primary_join_hits;
            break;
        case fx_particle_model_join_channel::appearance_source_secondary:
            ++g_particle_model_source_secondary_join_hits;
            break;
        default:
            break;
        }

        if (snap.waterwave_same_model_instance)
            ++g_waterwave_same_instance_hits;
        ++g_particle_model_join_hits;
        return true;
    }

    ++g_particle_model_join_misses;
    return false;
}

void * __fastcall particle_model_ctor_entry(
    void *self,
    void *arg2,
    void *arg3,
    void *arg4,
    void *arg5) noexcept
{
    void *result = nullptr;

    if (g_particle_model_ctor_original != nullptr) {
        result =
            g_particle_model_ctor_original(
                self,
                arg2,
                arg3,
                arg4,
                arg5);
    }

    if (result != nullptr)
        register_particle_model(
            result,
            arg2,
            arg3,
            arg4,
            arg5);

    return result;
}

void * __fastcall particle_model_dtor_entry(
    void *self,
    std::uint32_t flags) noexcept
{
    if (self != nullptr) {
        {
            std::lock_guard<std::mutex> lock(g_model_mutex);
            g_particle_models.erase(
                reinterpret_cast<std::uintptr_t>(
                    self));
        }
        ++g_particle_model_dtor_events;
    }

    if (g_particle_model_dtor_original != nullptr) {
        return g_particle_model_dtor_original(
            self,
            flags);
    }

    return self;
}

bool read_semantic_index_getter() noexcept
{
    if (g_base == 0u)
        return false;

    const auto *bytes =
        reinterpret_cast<const std::uint8_t *>(
            g_base + k_semantic_index_getter_rva);

    if (!readable_range(
            bytes,
            k_semantic_index_getter_bytes.size()) ||
        std::memcmp(
            bytes,
            k_semantic_index_getter_bytes.data(),
            k_semantic_index_getter_bytes.size()) != 0)
        return false;

    g_semantic_index_getter =
        reinterpret_cast<semantic_index_getter_fn>(
            const_cast<std::uint8_t *>(bytes));
    return true;
}

bool __fastcall particle_state_update_entry(
    void *appearance,
    void *key_object,
    void *context) noexcept
{
    ++g_particle_state_update_events;

    backend_semantic_record record{};
    record.generation =
        g_generation.fetch_add(1u) + 1u;

    if (appearance != nullptr &&
        readable_range(key_object,sizeof(std::uint32_t))) {
        std::memcpy(
            &record.backend_key,
            key_object,
            sizeof(record.backend_key));
        record.key_observed = true;
        ++g_backend_key_reads;

        if (g_semantic_index_getter != nullptr) {
            record.waterwave_runtime_index =
                g_semantic_index_getter(
                    pp::k_waterwave_spx_semantic_id);
            ++g_waterwave_runtime_index_reads;

            record.matches_waterwave =
                record.waterwave_runtime_index != 0u &&
                record.backend_key ==
                    record.waterwave_runtime_index;

            if (record.matches_waterwave)
                ++g_backend_key_waterwave_matches;
        }

        std::lock_guard<std::mutex> lock(g_model_mutex);

        if (g_backend_semantics.size() >=
                k_backend_semantic_registry_cap &&
            g_backend_semantics.find(
                reinterpret_cast<std::uintptr_t>(
                    appearance)) ==
                g_backend_semantics.end()) {
            g_backend_semantics.clear();
        }

        g_backend_semantics[
            reinterpret_cast<std::uintptr_t>(
                appearance)] = record;
    } else {
        ++g_backend_key_read_failures;
    }

    if (g_particle_state_update_original != nullptr) {
        return g_particle_state_update_original(
            appearance,
            key_object,
            context);
    }

    return false;
}

void attach_backend_semantic(
    fx_draw_snapshot &snap) noexcept
{
    if (snap.kind != fx_draw_entity_kind::particle ||
        snap.appearance_state == nullptr)
        return;

    std::lock_guard<std::mutex> lock(g_model_mutex);

    const auto found =
        g_backend_semantics.find(
            reinterpret_cast<std::uintptr_t>(
                snap.appearance_state));

    if (found == g_backend_semantics.end()) {
        ++g_backend_semantic_snapshot_misses;
        return;
    }

    snap.backend_semantic_generation =
        found->second.generation;
    snap.appearance_backend_key =
        found->second.backend_key;
    snap.waterwave_runtime_semantic_index =
        found->second.waterwave_runtime_index;
    snap.appearance_backend_key_observed =
        found->second.key_observed;
    snap.backend_key_matches_waterwave_runtime_index =
        found->second.matches_waterwave;
    ++g_backend_semantic_snapshot_hits;
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

    if (kind == fx_draw_entity_kind::particle) {
        std::memcpy(
            &snap.appearance_owner,
            state_bytes + 0x10u,
            sizeof(snap.appearance_owner));
    }

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

    (void)join_particle_model(snap);
    attach_backend_semantic(snap);

    snap.waterwave_semantic_model_candidate =
        is_waterwave_semantic_model_diagnostic_candidate(snap);
    if (snap.waterwave_semantic_model_candidate)
        ++g_waterwave_semantic_model_candidate_hits;

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

    // ReShade D3D draw callbacks execute synchronously inside the original
    // entity draw. Retire the TLS identity immediately afterwards so an
    // unrelated later API draw cannot inherit stale FX authority.
    g_tls.snapshot = {};
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

    g_tls.snapshot = {};
}

} // namespace

bool install() noexcept
{
    if (g_particle.patched ||
        g_cluster.patched ||
        g_particle_model_ctor.patched ||
        g_particle_model_dtor.patched ||
        g_particle_state_update.patched)
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

    if (!read_semantic_index_getter())
        goto fail;

    g_state.semantic_index_getter_attested = true;

    if (!prepare_hook(
            g_particle_model_ctor,
            k_particle_model_ctor_rva,
            k_particle_model_ctor_prefix,
            reinterpret_cast<void *>(
                &particle_model_ctor_entry)) ||
        !prepare_hook(
            g_particle_model_dtor,
            k_particle_model_dtor_rva,
            k_particle_model_dtor_prefix,
            reinterpret_cast<void *>(
                &particle_model_dtor_entry)) ||
        !prepare_hook(
            g_particle_state_update,
            k_particle_state_update_rva,
            k_particle_state_update_prefix,
            reinterpret_cast<void *>(
                &particle_state_update_entry)) ||
        !prepare_hook(
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

    g_particle_model_ctor_original =
        reinterpret_cast<particle_model_ctor_fn>(
            g_particle_model_ctor.trampoline);
    g_particle_model_dtor_original =
        reinterpret_cast<particle_model_dtor_fn>(
            g_particle_model_dtor.trampoline);
    g_particle_state_update_original =
        reinterpret_cast<particle_state_update_fn>(
            g_particle_state_update.trampoline);
    g_particle_original =
        reinterpret_cast<draw_fn>(
            g_particle.trampoline);
    g_cluster_original =
        reinterpret_cast<draw_fn>(
            g_cluster.trampoline);

    if (!arm_hook(g_particle_model_dtor) ||
        !arm_hook(g_particle_model_ctor) ||
        !arm_hook(g_particle_state_update) ||
        !arm_hook(g_cluster) ||
        !arm_hook(g_particle))
        goto fail;

    g_state.particle_hook_armed = true;
    g_state.cluster_hook_armed = true;
    g_state.particle_model_ctor_hook_armed = true;
    g_state.particle_model_dtor_hook_armed = true;
    g_state.particle_state_update_hook_armed = true;
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
    const bool model_ctor_ok =
        restore_hook(g_particle_model_ctor);
    const bool model_dtor_ok =
        restore_hook(g_particle_model_dtor);
    const bool state_update_ok =
        restore_hook(g_particle_state_update);

    if (!particle_ok ||
        !cluster_ok ||
        !model_ctor_ok ||
        !model_dtor_ok ||
        !state_update_ok) {
        g_state.restore_failed = true;
        g_state.quarantined = true;
        g_state.particle_hook_armed =
            g_particle.patched;
        g_state.cluster_hook_armed =
            g_cluster.patched;
        g_state.particle_model_ctor_hook_armed =
            g_particle_model_ctor.patched;
        g_state.particle_model_dtor_hook_armed =
            g_particle_model_dtor.patched;
        g_state.particle_state_update_hook_armed =
            g_particle_state_update.patched;
        return;
    }

    g_particle_original = nullptr;
    g_cluster_original = nullptr;
    g_particle_model_ctor_original = nullptr;
    g_particle_model_dtor_original = nullptr;
    g_particle_state_update_original = nullptr;
    g_semantic_index_getter = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_model_mutex);
        g_particle_models.clear();
        g_backend_semantics.clear();
    }
    g_base = 0u;
    g_tls = {};
    g_state = {};
}

bool publish_waterwave_model_identity(
    void *particle_model_instance,
    const pp::waterwave_authored_identity &identity) noexcept
{
    if (particle_model_instance == nullptr ||
        pp::validate_waterwave_authored_identity(identity) !=
            pp::waterwave_authored_identity_result::
                exact_authored_identity) {
        ++g_waterwave_publish_fail;
        return false;
    }

    std::lock_guard<std::mutex> lock(g_model_mutex);

    const auto found =
        g_particle_models.find(
            reinterpret_cast<std::uintptr_t>(
                particle_model_instance));

    if (found == g_particle_models.end() ||
        found->second.generation == 0u) {
        ++g_waterwave_publish_fail;
        return false;
    }

    found->second.waterwave_identity =
        identity;
    found->second.waterwave_identity_exact = true;
    ++g_waterwave_publish_ok;
    return true;
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
    out.particle_model_ctor_events =
        g_particle_model_ctor_events.load();
    out.particle_model_dtor_events =
        g_particle_model_dtor_events.load();
    out.particle_model_join_hits =
        g_particle_model_join_hits.load();
    out.particle_model_join_misses =
        g_particle_model_join_misses.load();
    out.particle_model_owner_join_hits =
        g_particle_model_owner_join_hits.load();
    out.particle_model_source_primary_join_hits =
        g_particle_model_source_primary_join_hits.load();
    out.particle_model_source_secondary_join_hits =
        g_particle_model_source_secondary_join_hits.load();
    out.particle_state_update_events =
        g_particle_state_update_events.load();
    out.backend_key_reads =
        g_backend_key_reads.load();
    out.backend_key_read_failures =
        g_backend_key_read_failures.load();
    out.waterwave_runtime_index_reads =
        g_waterwave_runtime_index_reads.load();
    out.backend_key_waterwave_matches =
        g_backend_key_waterwave_matches.load();
    out.waterwave_semantic_model_candidate_hits =
        g_waterwave_semantic_model_candidate_hits.load();
    out.backend_semantic_snapshot_hits =
        g_backend_semantic_snapshot_hits.load();
    out.backend_semantic_snapshot_misses =
        g_backend_semantic_snapshot_misses.load();
    out.waterwave_publish_ok =
        g_waterwave_publish_ok.load();
    out.waterwave_publish_fail =
        g_waterwave_publish_fail.load();
    out.waterwave_same_instance_hits =
        g_waterwave_same_instance_hits.load();
    {
        std::lock_guard<std::mutex> lock(g_model_mutex);
        out.particle_model_registry_size =
            static_cast<std::uint64_t>(
                g_particle_models.size());
    }
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
    g_particle_model_ctor_events.store(0u);
    g_particle_model_dtor_events.store(0u);
    g_particle_model_join_hits.store(0u);
    g_particle_model_join_misses.store(0u);
    g_particle_model_owner_join_hits.store(0u);
    g_particle_model_source_primary_join_hits.store(0u);
    g_particle_model_source_secondary_join_hits.store(0u);
    g_particle_state_update_events.store(0u);
    g_backend_key_reads.store(0u);
    g_backend_key_read_failures.store(0u);
    g_waterwave_runtime_index_reads.store(0u);
    g_backend_key_waterwave_matches.store(0u);
    g_waterwave_semantic_model_candidate_hits.store(0u);
    g_backend_semantic_snapshot_hits.store(0u);
    g_backend_semantic_snapshot_misses.store(0u);
    g_waterwave_publish_ok.store(0u);
    g_waterwave_publish_fail.store(0u);
    g_waterwave_same_instance_hits.store(0u);
    g_snapshot_hits.store(0u);
    g_snapshot_misses.store(0u);
    g_generation.store(0u);
    {
        std::lock_guard<std::mutex> lock(g_model_mutex);
        g_backend_semantics.clear();
    }
    g_tls = {};
}

} // namespace dsrrl::runtime::bloom_fx_draw_transport
