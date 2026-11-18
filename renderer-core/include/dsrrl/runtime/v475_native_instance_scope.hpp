#pragma once
// Retail DSR Source1: exact *live* default model-builder frame only.
// Reuse immutable V448 reverse-engineered Win64 unwind/type certificate.
// No new native detour, no disk EXE patch, no gameplay actor inference.
#include <Windows.h>
#include <cstdint>
#include "dsrrl/runtime/v448_live_builder_probe.hpp"
#include "dsrrl/runtime/v475_instance_source1_transport.hpp"

namespace dsrrl::runtime::v475 {
inline bool capture_exact_live_ticket(
    const material_identity &material,
    std::uint64_t selector_epoch,
    ticket &out) noexcept
{
    out = {};
    if (selector_epoch == 0u || !material.valid ||
        !material.owner_tuple_exact || !material.material_slot_valid ||
        !material.actual_material_exact || material.route_index != 345u)
        return false;

    const auto base =
        reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!base)
        return false;
    const auto probe = v448::probe_native_builder(base);
    if (probe.status != v448::probe_status::live_default_model ||
        !v448::target_native_row(probe.w))
        return false;
    const auto &w = probe.w;
    if (w.entity > UINTPTR_MAX - 0x108u ||
        w.dp != w.entity + 0x108u)
        return false;
    std::uint32_t full_handle = 0u;
    if (!v443::read(w.entity + 0x10u, full_handle))
        return false;

    out.entity = w.entity;
    out.draw_param = w.dp;
    out.packet = w.packet;
    out.context = w.context;
    out.engine_model = w.engine_model;
    out.native_frame = w.frame_rbp;
    out.registration_handle = full_handle;
    out.packed_a = w.a;
    out.packed_b = w.b;
    out.beta_bits = w.beta_bits;
    out.selector_epoch = selector_epoch;
    out.material = material;
    out.exact_type = true;      // verified V448 vtable + DP + engine model
    out.default_builder = true; // verified native return RVA 0x220CF0
    return well_formed(out);
}
} // namespace dsrrl::runtime::v475
