#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/dof_tonemap_handoff_runtime.hpp"

#include "dsrrl/operators/dof/dof_island.hpp"\n#include "dsrrl/runtime/dof_process_memory.hpp"

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace dsrrl::runtime::dof {
namespace {

constexpr std::uintptr_t k_rva_tonemap_pass13 =
    operators::dof::dsr_active_output_cut.tonemap_pass13_executor_rva;
constexpr std::uintptr_t k_rva_image_state_global = 0x01C6D598u;
// Retail pass 0x13 dispatches to 0x1404572A0. The first 14 bytes end
// exactly on an instruction boundary and are sufficient for the absolute
// jump patch; resume at +0x0E before the stack-frame allocation.
constexpr std::size_t k_stolen = 14u;

constexpr std::array<std::uint8_t, k_stolen> k_expected = {{
    0x48,0x8B,0xC4,
    0x48,0x89,0x58,0x20,
    0x55,
    0x56,
    0x57,
    0x41,0x56,
    0x41,0x57
}};

using pass13_fn = void(__fastcall *)(
    void *,
    void *,
    void *);

std::mutex g_mutex;
std::uintptr_t g_base = 0u;
void *g_target = nullptr;
void *g_trampoline = nullptr;
pass13_fn g_original = nullptr;
std::array<std::uint8_t, k_stolen> g_original_bytes{};
bool g_patched = false;

thread_local void *g_pass_desc = nullptr;
thread_local unsigned g_depth = 0u;

std::atomic<std::uint64_t> g_calls{0u};
std::atomic<std::uint64_t> g_hits{0u};
std::atomic<std::uint64_t> g_misses{0u};

bool write_bytes(
    void *address,
    const void *bytes,
    std::size_t size) noexcept
{
    DWORD old = 0u;
    if (address == nullptr ||
        bytes == nullptr ||
        size == 0u ||
        !VirtualProtect(
            address,
            size,
            PAGE_EXECUTE_READWRITE,
            &old))
        return false;

    std::memcpy(address, bytes, size);
    const bool flushed =
        FlushInstructionCache(
            GetCurrentProcess(),
            address,
            size) != FALSE;

    DWORD ignored = 0u;
    const bool restored =
        VirtualProtect(
            address,
            size,
            old,
            &ignored) != FALSE;

    return flushed && restored;
}

void __fastcall hook_pass13(
    void *node,
    void *render_context,
    void *pass_desc) noexcept
{
    ++g_calls;

    void *const previous_desc = g_pass_desc;
    const unsigned previous_depth = g_depth;

    g_pass_desc = pass_desc;
    g_depth = previous_depth + 1u;

    if (g_original != nullptr)
        g_original(
            node,
            render_context,
            pass_desc);

    g_depth = previous_depth;
    g_pass_desc = previous_desc;
}

bool build_trampoline() noexcept
{
    auto *mem = static_cast<std::uint8_t *>(
        VirtualAlloc(
            nullptr,
            64u,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE));
    if (mem == nullptr)
        return false;

    std::size_t cursor = 0u;
    std::memcpy(
        mem + cursor,
        g_original_bytes.data(),
        g_original_bytes.size());
    cursor += g_original_bytes.size();

    mem[cursor++] = 0xFFu;
    mem[cursor++] = 0x25u;

    const std::uint32_t zero = 0u;
    std::memcpy(
        mem + cursor,
        &zero,
        sizeof(zero));
    cursor += sizeof(zero);

    const std::uint64_t resume =
        static_cast<std::uint64_t>(
            g_base + k_rva_tonemap_pass13 + k_stolen);
    std::memcpy(
        mem + cursor,
        &resume,
        sizeof(resume));
    cursor += sizeof(resume);

    if (FlushInstructionCache(
            GetCurrentProcess(),
            mem,
            cursor) == FALSE) {
        VirtualFree(mem, 0u, MEM_RELEASE);
        return false;
    }

    g_trampoline = mem;
    g_original =
        reinterpret_cast<pass13_fn>(
            g_trampoline);
    return true;
}

bool patch_entry() noexcept
{
    auto *target =
        reinterpret_cast<std::uint8_t *>(
            g_base + k_rva_tonemap_pass13);

    if (!process_memory::safe_read_bytes(
            target,
            g_original_bytes.data(),
            g_original_bytes.size()) ||
        g_original_bytes != k_expected)
        return false;

    if (!build_trampoline())
        return false;

    std::array<std::uint8_t, k_stolen> patch{};
    patch.fill(0x90u);
    patch[0] = 0xFFu;
    patch[1] = 0x25u;

    const std::uint32_t zero = 0u;
    std::memcpy(
        patch.data() + 2u,
        &zero,
        sizeof(zero));

    const std::uint64_t hook =
        reinterpret_cast<std::uint64_t>(
            &hook_pass13);
    std::memcpy(
        patch.data() + 6u,
        &hook,
        sizeof(hook));

    g_target = target;
    if (!write_bytes(
            g_target,
            patch.data(),
            patch.size())) {
        g_target = nullptr;
        return false;
    }

    g_patched = true;
    return true;
}

void restore_locked() noexcept
{
    if (g_patched &&
        g_target != nullptr)
        (void)write_bytes(
            g_target,
            g_original_bytes.data(),
            g_original_bytes.size());

    g_patched = false;
    g_target = nullptr;
    g_original = nullptr;

    if (g_trampoline != nullptr) {
        VirtualFree(
            g_trampoline,
            0u,
            MEM_RELEASE);
        g_trampoline = nullptr;
    }
}

bool descriptor_is_dof_output() noexcept
{
    if (g_depth == 0u ||
        g_pass_desc == nullptr ||
        g_base == 0u)
        return false;

    std::uint32_t descriptor_source = 0u;
    if (!process_memory::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                g_pass_desc) + 0x0Cu,
            &descriptor_source,
            sizeof(descriptor_source)) ||
        descriptor_source == 0u)
        return false;

    std::uintptr_t image_state = 0u;
    if (!process_memory::safe_read_bytes(
            reinterpret_cast<const void *>(
                g_base + k_rva_image_state_global),
            &image_state,
            sizeof(image_state)) ||
        image_state == 0u)
        return false;

    std::uint32_t dof_srv_alias = 0u;
    if (!process_memory::safe_read_bytes(
            reinterpret_cast<const void *>(
                image_state + 0x104u),
            &dof_srv_alias,
            sizeof(dof_srv_alias)) ||
        dof_srv_alias == 0u)
        return false;

    return descriptor_source == dof_srv_alias;
}

} // namespace

bool register_tonemap_handoff_scope_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_patched)
        return true;

    g_base = process_memory::image_base();
    if (g_base == 0u ||
        !patch_entry()) {
        restore_locked();
        g_base = 0u;
        return false;
    }

    g_calls.store(0u);
    g_hits.store(0u);
    g_misses.store(0u);
    return true;
}

void unregister_tonemap_handoff_scope_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    restore_locked();
    g_base = 0u;
    g_pass_desc = nullptr;
    g_depth = 0u;
}

bool inside_exact_tonemap_dof_handoff() noexcept
{
    const bool match =
        g_patched &&
        descriptor_is_dof_output();

    if (g_depth != 0u) {
        if (match)
            ++g_hits;
        else
            ++g_misses;
    }

    return match;
}

tonemap_handoff_telemetry tonemap_handoff_status() noexcept
{
    return {
        g_calls.load(),
        g_hits.load(),
        g_misses.load(),
        g_patched && g_original != nullptr
    };
}

} // namespace dsrrl::runtime::dof
