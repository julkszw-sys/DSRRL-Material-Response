#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/dof_plain_rate_runtime.hpp"

#include "dsrrl/operators/dof/dof_resource_contract.hpp"
#include "dsrrl/runtime/engine_hooks.hpp"

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace dsrrl::runtime::dof {
namespace {

using operators::dof::dsr_plain_dofrate_switch;

std::mutex g_mutex;
std::uintptr_t g_base = 0u;
std::uint8_t *g_target = nullptr;
bool g_registered = false;
plain_rate_telemetry g_telemetry{};

bool write_bytes(
    void *address,
    const std::uint8_t *bytes,
    std::size_t size) noexcept
{
    if (address == nullptr || bytes == nullptr || size == 0u)
        return false;

    DWORD old_protect = 0u;
    if (!VirtualProtect(
            address,
            size,
            PAGE_EXECUTE_READWRITE,
            &old_protect))
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
            old_protect,
            &ignored) != FALSE;

    return flushed && restored;
}

template<std::size_t N>
bool read_bytes(
    const std::uint8_t *address,
    std::array<std::uint8_t, N> &out) noexcept
{
    return address != nullptr &&
        engine::safe_read_bytes(
            address,
            out.data(),
            out.size());
}

template<std::size_t N>
bool bytes_equal(
    const std::array<std::uint8_t, N> &a,
    const std::array<std::uint8_t, N> &b) noexcept
{
    return a == b;
}

bool restore_locked() noexcept
{
    if (!g_telemetry.active)
        return true;

    std::array<std::uint8_t, 5> current{};
    if (!read_bytes(g_target, current) ||
        !bytes_equal(
            current,
            dsr_plain_dofrate_switch.replacement_plain_selector)) {
        ++g_telemetry.restore_fail;
        return false;
    }

    if (!write_bytes(
            g_target,
            dsr_plain_dofrate_switch.expected_cb_selector.data(),
            dsr_plain_dofrate_switch.expected_cb_selector.size())) {
        ++g_telemetry.restore_fail;
        return false;
    }

    std::array<std::uint8_t, 5> verify{};
    if (!read_bytes(g_target, verify) ||
        !bytes_equal(
            verify,
            dsr_plain_dofrate_switch.expected_cb_selector)) {
        ++g_telemetry.restore_fail;
        return false;
    }

    g_telemetry.active = false;
    ++g_telemetry.restore_ok;
    return true;
}

} // namespace

bool register_plain_rate_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_registered)
        return g_telemetry.exact_preimage_verified;

    g_base = engine::image_base();
    if (g_base == 0u) {
        ++g_telemetry.preflight_fail;
        return false;
    }

    g_target =
        reinterpret_cast<std::uint8_t *>(
            g_base + dsr_plain_dofrate_switch.instruction_rva);

    std::array<std::uint8_t, 5> current{};
    if (!read_bytes(g_target, current) ||
        !bytes_equal(
            current,
            dsr_plain_dofrate_switch.expected_cb_selector)) {
        g_target = nullptr;
        g_base = 0u;
        ++g_telemetry.preflight_fail;
        return false;
    }

    g_registered = true;
    g_telemetry.exact_preimage_verified = true;
    ++g_telemetry.preflight_ok;
    return true;
}

void unregister_plain_rate_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);

    (void)restore_locked();

    g_registered = false;
    g_telemetry.exact_preimage_verified = false;
    g_target = nullptr;
    g_base = 0u;
}

bool activate_plain_rate_runtime(
    const operators::dof::activation_context &context) noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_registered ||
        !g_telemetry.exact_preimage_verified ||
        g_target == nullptr ||
        !operators::dof::evaluate_activation(context).active) {
        ++g_telemetry.activate_fail;
        return false;
    }

    if (g_telemetry.active)
        return true;

    std::array<std::uint8_t, 5> current{};
    if (!read_bytes(g_target, current) ||
        !bytes_equal(
            current,
            dsr_plain_dofrate_switch.expected_cb_selector)) {
        ++g_telemetry.activate_fail;
        return false;
    }

    if (!write_bytes(
            g_target,
            dsr_plain_dofrate_switch.replacement_plain_selector.data(),
            dsr_plain_dofrate_switch.replacement_plain_selector.size())) {
        ++g_telemetry.activate_fail;
        return false;
    }

    std::array<std::uint8_t, 5> verify{};
    if (!read_bytes(g_target, verify) ||
        !bytes_equal(
            verify,
            dsr_plain_dofrate_switch.replacement_plain_selector)) {
        (void)write_bytes(
            g_target,
            dsr_plain_dofrate_switch.expected_cb_selector.data(),
            dsr_plain_dofrate_switch.expected_cb_selector.size());
        ++g_telemetry.activate_fail;
        return false;
    }

    g_telemetry.active = true;
    ++g_telemetry.activate_ok;
    return true;
}

void deactivate_plain_rate_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    (void)restore_locked();
}

bool plain_rate_preflight_ready() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return
        g_registered &&
        g_telemetry.exact_preimage_verified &&
        g_target != nullptr;
}

plain_rate_telemetry plain_rate_status() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_telemetry;
}

} // namespace dsrrl::runtime::dof
