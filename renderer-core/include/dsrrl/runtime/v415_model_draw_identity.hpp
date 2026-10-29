#pragma once
// V415: observation-only native FlverModel identity at a verified callsite.
// NOT a gameplay actor ID or a bridge admission/selection authority.
#include <cstdint>

namespace dsrrl::runtime::v415 {
struct model_draw_stamp {
    std::uintptr_t model_this=0u;
    std::uintptr_t container=0u;
    std::uintptr_t context=0u;
    std::uintptr_t vtable=0u;
    std::int32_t material_index=-1;
    std::uint32_t class_kind=0u; // 1=FgFlverModel, 2=FlverModel
    // V416: distinct read-only FgModel parent provenance; not an actor ID.
    std::uintptr_t fg_parent=0u;
    std::uintptr_t fg_parent_vtable=0u;
    std::uintptr_t fg_parent_upstream=0u; // raw FgModel+0x30; semantics OPEN
    bool fg_parent_reciprocal=false;
    // V417: diagnostic fields from native FgModel. A registry slot is not
    // an authenticated gameplay actor ID nor a memory-lifetime generation.
    std::uint32_t fg_registry_id=0u;
    bool fg_registry_readable=false;
    std::uint32_t fg_refcount=0u;
    std::uintptr_t upstream_word0=0u; // opaque first 8 bytes if readable
    bool upstream_word0_readable=false;
    bool valid=false;
#if defined(DSRRL_V420_NATIVE_DRAW_PACKET_RE)
    // V420: RDX of native FgFlverModel draw, NOT gameplay actor identity.
    // Draw packet may be reused; only this selector invocation certifies it.
    std::uintptr_t draw_packet=0u;
    std::uint16_t packet_raw_a=0u,packet_raw_b=0u;
    std::uint32_t packet_beta_bits=0u,packet_draw_flags=0u;
    std::uint32_t packet_material_flags=0u;
    std::uint8_t packet_character=0u;
    bool packet_verified=false;
#endif
};

// Only the selector's current thread may consume this diagnostic stamp.
// Every selector opportunity clears it before resolving its own caller.
inline thread_local model_draw_stamp g_selector_model{};
inline void reset_selector() noexcept { g_selector_model={}; }
inline void record_selector(
    std::uintptr_t model_this,
    std::uintptr_t container,
    std::uintptr_t context,
    std::uintptr_t vtable,
    std::int32_t material_index,
    std::uint32_t class_kind) noexcept
{
    g_selector_model={
        model_this,container,context,vtable,
        material_index,class_kind,
        0u,0u,0u,false,
        0u,false,0u,0u,false,
        model_this!=0u && container!=0u &&
        context!=0u && class_kind!=0u
    };
}
// Called only after two-sided native FgModel/child identity verification.
inline void record_verified_fgparent(
    std::uintptr_t model_this,
    std::uintptr_t parent,
    std::uintptr_t parent_vtable,
    std::uintptr_t upstream) noexcept
{
    if (!g_selector_model.valid ||
        g_selector_model.class_kind != 1u ||
        g_selector_model.model_this != model_this ||
        parent == 0u)
        return;
    g_selector_model.fg_parent = parent;
    g_selector_model.fg_parent_vtable = parent_vtable;
    g_selector_model.fg_parent_upstream = upstream;
    g_selector_model.fg_parent_reciprocal = true;
}
// Only optional V417 diagnostics. Does not change the validity of V416's
// reciprocal pointer witness and must not be used as a selection carrier.
inline void record_registry_observation(
    std::uintptr_t model_this, std::uint32_t registry_id,
    bool registry_readable,
    std::uint32_t refcount, std::uintptr_t word0,
    bool word0_readable) noexcept
{
    if (!g_selector_model.valid ||
        !g_selector_model.fg_parent_reciprocal ||
        g_selector_model.model_this != model_this)
        return;
    g_selector_model.fg_registry_id=registry_id;
    g_selector_model.fg_registry_readable=registry_readable;
    g_selector_model.fg_refcount=refcount;
    g_selector_model.upstream_word0=word0;
    g_selector_model.upstream_word0_readable=word0_readable;
}
#if defined(DSRRL_V420_NATIVE_DRAW_PACKET_RE)
// Called only at original FlverModel::draw selector return 0x20E019,
// after exact packet[0]==FrpgDrawContext and finite-beta verification.
inline void record_packet(std::uintptr_t model_this,
    std::uintptr_t packet,std::uint16_t raw_a,std::uint16_t raw_b,
    std::uint32_t beta_bits,std::uint32_t draw_flags,
    std::uint32_t material_flags,std::uint8_t character) noexcept
{
    if(!g_selector_model.valid ||
       g_selector_model.model_this!=model_this || !packet ||
       character>1u || (beta_bits&0x7F800000u)==0x7F800000u)
        return;
    auto &s=g_selector_model;
    s.draw_packet=packet;
    s.packet_raw_a=raw_a;s.packet_raw_b=raw_b;
    s.packet_beta_bits=beta_bits;
    s.packet_draw_flags=draw_flags;
    s.packet_material_flags=material_flags;
    s.packet_character=character;
    s.packet_verified=true;
}
#endif
inline model_draw_stamp current_for(
    std::uintptr_t context) noexcept
{
    const auto s=g_selector_model;
    return s.valid && s.context==context
        ? s : model_draw_stamp{};
}
} // namespace dsrrl::runtime::v415
