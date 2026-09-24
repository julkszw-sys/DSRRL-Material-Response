#pragma once

#include "dsrrl/core/island_policy.hpp"

#include <cstdint>

namespace dsrrl::operators::point_light {

enum class pointlight_receiver_stratum : std::uint8_t {
    unsupported=0,
    fixed_nospc_pntss,
    fixed_nospc_pntssss,
    fixed_spc_pntss,
    fixed_spc_pntssss,
    clustered_nospc_pnts,
    clustered_spc_pnts
};

enum class pointlight_source_scope : std::uint8_t {
    unsupported=0,
    ordinary_map_or_dynamic,
    player_lantern,
    sfx_attached,
    cutscene_special
};

enum class pointlight_runtime_reason : std::uint8_t {
    ready=0,
    core_gate_not_active,
    receiver_not_verified,
    unsupported_receiver,
    source_scope_not_verified,
    special_source_scope_not_supported,
    source_instance_state_not_ready,
    source_amplitude_category_not_ready,
    attenuation_not_ready,
    fixed_owner_context_not_verified,
    fixed_producer_serial_not_fresh,
    fixed_raw_q_sidecar_not_ready,
    fixed_light_count_mismatch,
    fixed_membership_not_verified,
    clustered_membership_descriptor_invalid,
    clustered_four_slot_shader_not_ready,
    stock_cluster_membership_not_bypassed,
    diffuse_material_path_not_ready,
    local_specular_path_not_ready,
    terminal_sat_not_ready,
    downstream_atmosphere_not_ready,
    draw_transaction_not_ready
};

struct clustered_membership_descriptor {
    bool immutable_draw_local=false;
    bool ordered_source_identity_ready=false;
    bool ordered_source_geometry_ready=false;
    bool ordered_raw_q_ready=false;
    std::uint8_t raw_selected_count=0;
    std::uint8_t material_max_pnt_lit_num=0;
    std::uint8_t effective_count=0;
};

inline bool clustered_membership_descriptor_ready(
    const clustered_membership_descriptor &descriptor) noexcept
{
    if(!descriptor.immutable_draw_local ||
       !descriptor.ordered_source_identity_ready ||
       !descriptor.ordered_source_geometry_ready ||
       !descriptor.ordered_raw_q_ready)
        return false;

    if(descriptor.raw_selected_count==0u ||
       descriptor.raw_selected_count>4u)
        return false;

    const auto expected=
        descriptor.raw_selected_count<descriptor.material_max_pnt_lit_num ?
        descriptor.raw_selected_count :
        descriptor.material_max_pnt_lit_num;

    return expected>0u &&
           descriptor.effective_count==expected;
}

struct pointlight_runtime_context {
    bool receiver_verified=false;
    pointlight_receiver_stratum receiver_stratum=
        pointlight_receiver_stratum::unsupported;

    bool source_scope_verified=false;
    pointlight_source_scope source_scope=
        pointlight_source_scope::unsupported;
    bool source_instance_state_ready=false;
    bool source_amplitude_category_ready=false;
    bool attenuation_ready=false;

    // Fixed PntSS/PntSSSS direct-PTDE island transport contract.
    // Raw source q is delivered through draw-scoped t19 rather than mutating
    // stock DSR c112..c119. Association is exact owner_context + producer
    // serial, and the receiver suffix fixes the legal selected count (2/4).
    bool fixed_owner_context_verified=false;
    bool fixed_producer_serial_fresh=false;
    bool fixed_raw_q_t19_sidecar_ready=false;
    std::uint8_t fixed_selected_light_count=0;
    bool fixed_membership_verified=false;

    // Clustered PntS cannot reuse stock DSR t16/t17 membership. The PTDE
    // carrier is the immutable first-four ordered overlap result plus the raw
    // selected count and material g_MaxPntLitNum/effective-count provenance.
    clustered_membership_descriptor clustered_membership{};
    bool clustered_four_slot_shader_ready=false;
    bool stock_cluster_membership_bypassed=false;

    bool diffuse_material_path_ready=false;
    bool local_specular_path_ready=false;
    bool terminal_sat_ready=false;
    bool downstream_atmosphere_ready=false;
    bool draw_transaction_ready=false;
};

struct pointlight_runtime_plan {
    bool ready=false;
    pointlight_runtime_reason reason=
        pointlight_runtime_reason::core_gate_not_active;

    bool use_fixed_native_membership=false;
    bool use_cpu_selected_four_sidecar=false;
    bool bypass_stock_cluster_membership=false;
    bool require_local_specular=false;
    std::uint8_t selected_light_count=0;
};

inline bool pointlight_receiver_is_fixed(
    pointlight_receiver_stratum stratum) noexcept
{
    return stratum==pointlight_receiver_stratum::fixed_nospc_pntss ||
           stratum==pointlight_receiver_stratum::fixed_nospc_pntssss ||
           stratum==pointlight_receiver_stratum::fixed_spc_pntss ||
           stratum==pointlight_receiver_stratum::fixed_spc_pntssss;
}

inline std::uint8_t pointlight_fixed_expected_light_count(
    pointlight_receiver_stratum stratum) noexcept
{
    switch(stratum){
    case pointlight_receiver_stratum::fixed_nospc_pntss:
    case pointlight_receiver_stratum::fixed_spc_pntss:
        return 2u;
    case pointlight_receiver_stratum::fixed_nospc_pntssss:
    case pointlight_receiver_stratum::fixed_spc_pntssss:
        return 4u;
    default:
        return 0u;
    }
}

inline bool pointlight_receiver_is_clustered(
    pointlight_receiver_stratum stratum) noexcept
{
    return stratum==pointlight_receiver_stratum::clustered_nospc_pnts ||
           stratum==pointlight_receiver_stratum::clustered_spc_pnts;
}

inline bool pointlight_receiver_has_specular(
    pointlight_receiver_stratum stratum) noexcept
{
    return stratum==pointlight_receiver_stratum::fixed_spc_pntss ||
           stratum==pointlight_receiver_stratum::fixed_spc_pntssss ||
           stratum==pointlight_receiver_stratum::clustered_spc_pnts;
}

inline pointlight_runtime_plan evaluate_pointlight_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const pointlight_runtime_context &context) noexcept
{
    pointlight_runtime_plan out;
    const auto gate=core::evaluate_operator_activation(
        features,core::operator_id::point_light,activation);

    if(gate.state!=core::island_state::active){
        out.reason=pointlight_runtime_reason::core_gate_not_active;
        return out;
    }
    if(!context.receiver_verified){
        out.reason=pointlight_runtime_reason::receiver_not_verified;
        return out;
    }
    if(context.receiver_stratum==pointlight_receiver_stratum::unsupported){
        out.reason=pointlight_runtime_reason::unsupported_receiver;
        return out;
    }
    if(!context.source_scope_verified){
        out.reason=pointlight_runtime_reason::source_scope_not_verified;
        return out;
    }
    if(context.source_scope!=pointlight_source_scope::ordinary_map_or_dynamic){
        out.reason=
            pointlight_runtime_reason::special_source_scope_not_supported;
        return out;
    }
    if(!context.source_instance_state_ready){
        out.reason=pointlight_runtime_reason::source_instance_state_not_ready;
        return out;
    }
    if(!context.source_amplitude_category_ready){
        out.reason=
            pointlight_runtime_reason::source_amplitude_category_not_ready;
        return out;
    }
    if(!context.attenuation_ready){
        out.reason=pointlight_runtime_reason::attenuation_not_ready;
        return out;
    }

    if(pointlight_receiver_is_fixed(context.receiver_stratum)){
        out.use_fixed_native_membership=true;
        if(!context.fixed_owner_context_verified){
            out.reason=
                pointlight_runtime_reason::fixed_owner_context_not_verified;
            return out;
        }
        if(!context.fixed_producer_serial_fresh){
            out.reason=
                pointlight_runtime_reason::fixed_producer_serial_not_fresh;
            return out;
        }
        if(!context.fixed_raw_q_t19_sidecar_ready){
            out.reason=
                pointlight_runtime_reason::fixed_raw_q_sidecar_not_ready;
            return out;
        }
        if(context.fixed_selected_light_count !=
           pointlight_fixed_expected_light_count(context.receiver_stratum)){
            out.reason=
                pointlight_runtime_reason::fixed_light_count_mismatch;
            return out;
        }
        if(!context.fixed_membership_verified){
            out.reason=
                pointlight_runtime_reason::fixed_membership_not_verified;
            return out;
        }
        out.selected_light_count=
            context.fixed_selected_light_count;
    }else if(pointlight_receiver_is_clustered(context.receiver_stratum)){
        out.use_cpu_selected_four_sidecar=true;
        out.bypass_stock_cluster_membership=true;
        if(!clustered_membership_descriptor_ready(
               context.clustered_membership)){
            out.reason=
                pointlight_runtime_reason::
                    clustered_membership_descriptor_invalid;
            return out;
        }
        out.selected_light_count=
            context.clustered_membership.effective_count;
        if(!context.clustered_four_slot_shader_ready){
            out.reason=
                pointlight_runtime_reason::
                    clustered_four_slot_shader_not_ready;
            return out;
        }
        if(!context.stock_cluster_membership_bypassed){
            out.reason=
                pointlight_runtime_reason::
                    stock_cluster_membership_not_bypassed;
            return out;
        }
    }else{
        out.reason=pointlight_runtime_reason::unsupported_receiver;
        return out;
    }

    if(!context.diffuse_material_path_ready){
        out.reason=
            pointlight_runtime_reason::diffuse_material_path_not_ready;
        return out;
    }

    out.require_local_specular=
        pointlight_receiver_has_specular(context.receiver_stratum);
    if(out.require_local_specular && !context.local_specular_path_ready){
        out.reason=
            pointlight_runtime_reason::local_specular_path_not_ready;
        return out;
    }

    if(!context.terminal_sat_ready){
        out.reason=pointlight_runtime_reason::terminal_sat_not_ready;
        return out;
    }
    if(!context.downstream_atmosphere_ready){
        out.reason=
            pointlight_runtime_reason::downstream_atmosphere_not_ready;
        return out;
    }
    if(!context.draw_transaction_ready){
        out.reason=pointlight_runtime_reason::draw_transaction_not_ready;
        return out;
    }

    out.ready=true;
    out.reason=pointlight_runtime_reason::ready;
    return out;
}

} // namespace dsrrl::operators::point_light
