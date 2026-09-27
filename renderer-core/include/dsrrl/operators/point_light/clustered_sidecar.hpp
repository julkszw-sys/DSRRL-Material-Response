#pragma once

#include "dsrrl/operators/material_response/material_response_b12_payload.hpp"

#include <array>
#include <cstdint>

namespace dsrrl::operators::point_light {

struct clustered_source_raw_v1 {
    std::uint32_t source_id = 0u;
    std::uint8_t source_category = 0u;
    std::array<float,4> position_inv_range{};
    std::array<float,4> raw_q_end{};
};

struct clustered_t18_record_v1 {
    std::array<float,4> position_inv_range{};
    std::array<float,4> color_end{};
    std::array<std::uint32_t,4> metadata{};
};

static_assert(sizeof(clustered_t18_record_v1) == 48u,
    "DSR clustered t18 record ABI must remain 48 bytes.");

struct clustered_sidecar_payload_v1 {
    std::array<clustered_t18_record_v1,4> t18{};
    std::array<std::array<float,4>,4> t19{};
    operators::material_response::material_response_b12_payload b12{};
    std::array<std::uint32_t,4> ordered_source_ids{};
    std::uint8_t raw_selected_count = 0u;
    std::uint32_t material_max_pnt_lit_num = 0u;
    std::uint8_t effective_count = 0u;
    bool ready = false;
};

enum class clustered_sidecar_result_v1 : std::uint8_t {
    ready = 0,
    fail_invalid_count,
    fail_invalid_material_limit,
    fail_nonfinite_source,
    fail_invalid_inv_range,
    fail_invalid_end
};

struct clustered_sidecar_build_v1 {
    clustered_sidecar_result_v1 result =
        clustered_sidecar_result_v1::fail_invalid_count;
    clustered_sidecar_payload_v1 payload{};
};

clustered_sidecar_build_v1 build_clustered_sidecar_v1(
    const std::array<clustered_source_raw_v1,4> &sources,
    std::uint8_t raw_selected_count,
    std::uint32_t material_max_pnt_lit_num,
    const operators::material_response::decision &material) noexcept;

} // namespace dsrrl::operators::point_light
