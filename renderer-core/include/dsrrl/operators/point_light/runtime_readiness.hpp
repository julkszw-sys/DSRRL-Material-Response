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
    clustered_pnts
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
    fixed_receiver_payload_not_ready,
    fixed_membership_not_verified,
    clustered_cpu_membership_sidecar_not_ready,
    clustered_membership_order_not_verified,
    clustered_count_provenance_not_ready,
    clustered_effective_count_invalid,
    clustered_four_slot_shader_not_ready,
    stock_cluster_membership_not_bypassed,
    diffuse_material_path_not_ready,
    local_specular_path_not_ready,
    terminal_sat_not_ready,
    downstream_atmosphere_not_ready,
    draw_transaction_not_ready
};

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

    bool fixed_receiver_payload_ready=false;
    bool fixed_membership_verified=false;

    // Ordinary PTDE FrpgModel selection is not a DSR-style cluster lookup.
    // The sidecar must preserve the ordered first-four bucket/list-overlap set,
    // then preserve the material clamp N=min(N_raw,g_MaxPntLitNum).  Do not
    // authorize this island from stock DSR t16/t17 membership or count64.
    bool clustered_cpu_membership_sidecar_ready=false;
    bool clustered_membership_order_verified=false;
    bool clustered_count_provenance_ready=false;
    std::uint8_t clustered_raw_selected_count=0;
    std::uint8_t clustered_material_max_count=0;
    std::uint8_t clustered_effective_count=0;
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
    std::uint8_t selected_light_count=0;
    bool require_local_specular=false;
};

inline bool pointlight_receiver_is_fixed(
    pointlight_receiver_stratum stratum) noexcept
{
    return stratum==pointlight_receiver_stratum::fixed_nospc_pntss ||
           stratum==pointlight_receiver_stratum::fixed_nospc_pntssss ||
           stratum==pointlight_receiver_stratum::fixed_spc_pntss ||
           stratum==pointlight_receiver_stratum::fixed_spc_pntssss;
}

inline bool pointlight_receiver_has_specular(
    pointlight_receiver_stratum stratum) noexcept
{
    return stratum==pointlight_receiver_stratum::fixed_spc_pntss ||
           stratum==pointlight_receiver_stratum::fixed_spc_pntssss ||
           stratum==pointlight_receiver_stratum::clustered_pnts;
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
        if(!context.fixed_receiver_payload_ready){
            out.reason=
                pointlight_runtime_reason::fixed_receiver_payload_not_ready;
            return out;
        }
        if(!context.fixed_membership_verified){
            out.reason=
                pointlight_runtime_reason::fixed_membership_not_verified;
            return out;
        }
    }else{
        out.use_cpu_selected_four_sidecar=true;
        out.bypass_stock_cluster_membership=true;
        if(!context.clustered_cpu_membership_sidecar_ready){
            out.reason=
                pointlight_runtime_reason::
                    clustered_cpu_membership_sidecar_not_ready;
            return out;
        }
        if(!context.clustered_membership_order_verified){
            out.reason=
                pointlight_runtime_reason::
                    clustered_membership_order_not_verified;
            return out;
        }
        if(!context.clustered_count_provenance_ready){
            out.reason=
                pointlight_runtime_reason::
                    clustered_count_provenance_not_ready;
            return out;
        }
        const auto raw=context.clustered_raw_selected_count;
        const auto material_max=context.clustered_material_max_count;
        const auto expected=
            static_cast<std::uint8_t>(raw<material_max?raw:material_max);
        if(raw>4u || material_max>4u ||
           context.clustered_effective_count!=expected){
            out.reason=
                pointlight_runtime_reason::clustered_effective_count_invalid;
            return out;
        }
        out.selected_light_count=context.clustered_effective_count;
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
