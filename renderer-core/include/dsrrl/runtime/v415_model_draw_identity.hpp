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
    bool valid=false;
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
        model_this!=0u && container!=0u &&
        context!=0u && class_kind!=0u
    };
}
inline model_draw_stamp current_for(
    std::uintptr_t context) noexcept
{
    const auto s=g_selector_model;
    return s.valid && s.context==context
        ? s : model_draw_stamp{};
}
} // namespace dsrrl::runtime::v415
