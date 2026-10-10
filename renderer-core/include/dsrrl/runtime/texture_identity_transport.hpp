#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace dsrrl::runtime::texture_identity_transport {

struct hook_status {
    bool provenance_ok = false;
    bool name_hook_armed = false;
    bool clear_hook_armed = false;
    bool packet_hook_armed = false;
    bool decoder_hook_armed = false;
    bool restore_failed = false;
};

bool install() noexcept;
void uninstall() noexcept;
hook_status status() noexcept;

// Snapshot the exact logical texture name currently in the certified loader
// scope. Returns false when no exact name is active.
bool snapshot(std::wstring &logical_name) noexcept;

// Zero-allocation snapshot for resource lifecycle routing. The returned view
// points at thread-local transport storage and is valid only until the next
// texture-name hook event on the same thread.
bool snapshot_raw(
    const wchar_t *&logical_name,
    std::size_t &length) noexcept;

// Opt-in SPC25 producer/transport census. No name guesses and no pointer
// lifetimes are exported; counts alone diagnose the semantic handoff cut.
struct texture_name_liveness {
    std::uint64_t hook_calls = 0u;
    std::uint64_t names_captured = 0u;
    std::uint64_t names_cleared = 0u;
    std::uint64_t name_snapshots = 0u;
    std::uint64_t packet_writer_calls = 0u;
    std::uint64_t packet_named_in_scope = 0u;
    std::uint64_t cache_name_equal = 0u;
    std::uint64_t cache_name_different = 0u;
    std::uint64_t cache_name_unreadable = 0u;
    std::uint64_t cpu_rtti_exact = 0u;
    std::uint64_t cpu_rtti_unavailable = 0u;
    std::uint64_t cpu_tex2d_cpu_objects = 0u;
    std::uint64_t cpu_tex2d_fields_readable = 0u;
    std::uint64_t cpu_tex2d_p28_nonnull = 0u;
    std::uint64_t cpu_tex2d_p30_nonnull = 0u;
    std::uint64_t cpu_tex2d_both_nonnull = 0u;
    std::uint64_t decode_calls = 0u;
    std::uint64_t decode_payload_readable = 0u;
    std::uint64_t decode_writer_pointer_seen = 0u;
};
texture_name_liveness liveness() noexcept;

// Diagnostic-only pointer identity census. Caller already owns/pins the live
// native D3D11 view and resource; this only compares pointer *values* against
// previously sampled named DLTexture2D fields, without dereferencing either.
// A pointer match is NOT a certified lifetime/asset/PS t1 bridge authority.
// Sample each distinct live native view address at most once until the
// ReShade destruction callback tombstones it and rearms that address.
// Unobserved callback/lifetime gaps remain possible: cross-epoch OPEN.
// This suppresses repeated COM GetResource and 1024-slot scans.
bool should_sample_native_ps_t1(const void *native_view) noexcept;

// Targeted diagnostic only: observe the known c5330_s native SRV on
// eligible exact-material receivers even when the view's first-sample
// budget has already been consumed. Logs bounded coverage and requests an
// occasional exact pinned resource/epoch comparison. Never authorizes binds.
bool should_probe_c5330_ps_t1(
    const void *native_view, std::uint32_t receiver_id) noexcept;

// Source->bound PS-t1 bridge for equipment only. Returns one exact ASCII
// logical name from the live named DLTexture2D writer, provided the pinned
// view, resource and current generation all match ONE writer snapshot.
// Asset authorization and exact FLVER owner+slot must be rechecked by caller.
// Failure, ambiguity or stale generation returns false and stock DSR wins.
bool resolve_exact_equipment_native_ps_t1(
    const void *native_view, const void *native_resource,
    char *ascii_name, std::size_t capacity) noexcept;

// Exact source/transport witness only. Requires one named c5330_s CPU
// writer snapshot, identical pinned native view + resource + current
// generation; ambiguous or stale snapshots fail open. No SRV ownership.
bool match_exact_c5330_native_ps_t1(
    const void *native_view, const void *native_resource) noexcept;

void diagnose_native_ps_t1(
    const void *native_view,
    const void *native_resource) noexcept;

// ReShade destroy_resource_view event, native D3D11 view value only.
// Tombstones any recorded writer ownership for that view and rearms
// one-shot sampling if a later lifetime reuses its address. Never
// claims epoch equivalence until runtime destroy/recreate is attested.
void retire_native_ps_t1(const void *native_view) noexcept;

} // namespace dsrrl::runtime::texture_identity_transport
