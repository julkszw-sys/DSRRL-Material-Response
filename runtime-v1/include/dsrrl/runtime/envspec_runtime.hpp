#pragma once

#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"

#include <cstdint>
#include <reshade.hpp>

namespace dsrrl::runtime::envspec {

struct identity_snapshot {
    bool exact_material = false;
    bool material_present = false;
    bool slot_valid = false;
    std::uint8_t slot = 0;
    bool probe_a_ready = false;
    bool probe_b_required = false;
    bool probe_b_ready = false;
    std::uint16_t probe_a = 0;
    std::uint16_t probe_b = 0;

    // Future RESOURCE carrier. These are created from the exact canonical
    // 32x32x6 one-mip PTDE RGBA8 pack without CPU RGB/alpha predecode.
    bool packed_gi_admitted = false;
    bool stored_alpha_preserved = false;
    bool ptde_sampler_ready = false;
    bool ptde_a_ready = false;
    bool ptde_b_ready = false;
    reshade::api::resource_view ptde_a_view{};
    reshade::api::resource_view ptde_b_view{};
    reshade::api::sampler ptde_sampler{};

    bool identity_ready() const noexcept
    {
        return exact_material && material_present && slot_valid &&
               probe_a_ready && (!probe_b_required || probe_b_ready);
    }

    bool carrier_ready() const noexcept
    {
        return identity_ready() && packed_gi_admitted &&
               stored_alpha_preserved && ptde_sampler_ready &&
               ptde_a_ready && (!probe_b_required || ptde_b_ready);
    }

    bool ready() const noexcept
    {
        return identity_ready();
    }
};

bool register_runtime() noexcept;
void unregister_runtime() noexcept;

identity_snapshot observe_draw(
    reshade::api::command_list *cmd,
    const operators::material_response::mtd_envspec_semantics &material,
    bool exact_material,
    bool probe_b_required) noexcept;

} // namespace dsrrl::runtime::envspec
