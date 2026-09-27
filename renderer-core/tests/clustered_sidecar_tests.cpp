#include "dsrrl/operators/point_light/clustered_sidecar.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>

#define CHECK(x) do { if(!(x)){ std::cerr << "CHECK failed: " #x "\n"; return 1; } } while(false)

int main()
{
    using namespace dsrrl::operators;

    std::array<point_light::clustered_source_raw_v1,4> sources{};
    for (std::uint32_t i=0u;i<4u;++i) {
        sources[i].source_id = 100u+i;
        sources[i].source_category =
            static_cast<std::uint8_t>(i);
        sources[i].position_inv_range = {
            10.0f+static_cast<float>(i),
            20.0f,
            30.0f,
            0.125f
        };
        sources[i].raw_q_end = {
            0.1f*static_cast<float>(i+1u),
            0.2f,
            0.3f,
            12.0f+static_cast<float>(i)
        };
    }

    material_response::decision material{};
    material.active = true;
    material.c100 = {{2.0f,3.0f,4.0f}};
    material.c101_f0q = {{0.25f,0.5f,0.75f}};
    material.c101 = 2.5f;
    material.ptde_specular_power = 8.5f;
    material.ptde_specular_power_verified = true;

    const auto built =
        point_light::build_clustered_sidecar_v1(
            sources,
            4u,
            3u,
            material);

    CHECK(built.result ==
          point_light::clustered_sidecar_result_v1::ready);
    CHECK(built.payload.ready);
    CHECK(built.payload.raw_selected_count == 4u);
    CHECK(built.payload.material_max_pnt_lit_num == 3u);
    CHECK(built.payload.effective_count == 3u);

    // Draw-local loop count lives outside the ordinary cached MR state.
    CHECK(built.payload.b12[3][0] == 3.0f);

    // Current MR ABI is preserved for the PTDE material operands.
    CHECK(built.payload.b12[0][3] == 8.5f);
    CHECK(built.payload.b12[1][0] == 2.0f);
    CHECK(built.payload.b12[1][1] == 3.0f);
    CHECK(built.payload.b12[1][2] == 4.0f);
    CHECK(built.payload.b12[2][0] == 2.5f);

    for (std::uint32_t i=0u;i<3u;++i) {
        CHECK(built.payload.ordered_source_ids[i] == 100u+i);
        CHECK(built.payload.t18[i].position_inv_range ==
              sources[i].position_inv_range);
        CHECK(built.payload.t18[i].color_end[0] == 0.0f);
        CHECK(built.payload.t18[i].color_end[1] == 0.0f);
        CHECK(built.payload.t18[i].color_end[2] == 0.0f);
        CHECK(built.payload.t18[i].color_end[3] ==
              sources[i].raw_q_end[3]);
        CHECK(built.payload.t18[i].metadata[0] == 0u);
        CHECK(built.payload.t18[i].metadata[1] == 0u);
        CHECK(built.payload.t18[i].metadata[2] == 0u);
        CHECK(built.payload.t18[i].metadata[3] == 0u);
        CHECK(built.payload.t19[i] == sources[i].raw_q_end);
    }

    const auto bad_count =
        point_light::build_clustered_sidecar_v1(
            sources,0u,4u,material);
    CHECK(bad_count.result ==
          point_light::clustered_sidecar_result_v1::fail_invalid_count);

    auto bad_sources=sources;
    bad_sources[0].position_inv_range[3]=0.0f;
    const auto bad_range =
        point_light::build_clustered_sidecar_v1(
            bad_sources,1u,4u,material);
    CHECK(bad_range.result ==
          point_light::clustered_sidecar_result_v1::fail_invalid_inv_range);

    return 0;
}
