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
    bool probe_b_ready = false;
    std::uint16_t probe_a = 0;
    std::uint16_t probe_b = 0;

    bool ready() const noexcept
    {
        return exact_material && material_present && slot_valid &&
               probe_a_ready && probe_b_ready;
    }
};

bool register_runtime() noexcept;
void unregister_runtime() noexcept;

identity_snapshot observe_draw(
    reshade::api::command_list *cmd,
    const operators::material_response::mtd_envspec_semantics &material,
    bool exact_material) noexcept;

} // namespace dsrrl::runtime::envspec
