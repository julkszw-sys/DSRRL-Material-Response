#include "dsrrl/operators/point_light/clustered_sidecar.hpp"

#include <algorithm>
#include <cmath>

namespace dsrrl::operators::point_light {
namespace {

bool finite4(const std::array<float,4> &v) noexcept
{
    return std::isfinite(v[0]) &&
           std::isfinite(v[1]) &&
           std::isfinite(v[2]) &&
           std::isfinite(v[3]);
}

} // namespace

clustered_sidecar_build_v1 build_clustered_sidecar_v1(
    const std::array<clustered_source_raw_v1,4> &sources,
    std::uint8_t raw_selected_count,
    std::uint8_t material_max_pnt_lit_num,
    const operators::material_response::decision &material) noexcept
{
    clustered_sidecar_build_v1 out{};

    if (raw_selected_count == 0u ||
        raw_selected_count > 4u) {
        out.result = clustered_sidecar_result_v1::fail_invalid_count;
        return out;
    }

    if (material_max_pnt_lit_num == 0u ||
        material_max_pnt_lit_num > 4u) {
        out.result =
            clustered_sidecar_result_v1::fail_invalid_material_limit;
        return out;
    }

    const auto effective_count =
        static_cast<std::uint8_t>(
            std::min(
                static_cast<unsigned>(raw_selected_count),
                static_cast<unsigned>(material_max_pnt_lit_num)));

    if (effective_count == 0u) {
        out.result = clustered_sidecar_result_v1::fail_invalid_count;
        return out;
    }

    out.payload.b12 =
        operators::material_response::make_material_response_b12_payload(
            material);

    // Draw-local count carrier. The ordinary MR b12 remains immutable/cached;
    // clustered PointLight realizes its own b12 copy so the per-draw selector
    // result never mutates shared material state.
    out.payload.b12[3][0] =
        static_cast<float>(effective_count);

    for (std::uint8_t i = 0u;
         i < effective_count;
         ++i) {
        const auto &source = sources[i];

        if (!finite4(source.position_inv_range) ||
            !finite4(source.raw_q_end)) {
            out.result =
                clustered_sidecar_result_v1::fail_nonfinite_source;
            return out;
        }

        const float inv_range =
            source.position_inv_range[3];
        const float end =
            source.raw_q_end[3];

        if (!(inv_range > 0.0f)) {
            out.result =
                clustered_sidecar_result_v1::fail_invalid_inv_range;
            return out;
        }

        if (!(end > 0.0f)) {
            out.result =
                clustered_sidecar_result_v1::fail_invalid_end;
            return out;
        }

        out.payload.ordered_source_ids[i] =
            source.source_id;

        // t18 is deliberately geometry-only. Preserve the homologous
        // position/invRange lane and raw End, while zeroing DSR-only
        // transformed source RGB/category/falloff metadata. The PTDE local
        // light contribution must therefore obtain source RGB only from t19.
        out.payload.t18[i].position_inv_range =
            source.position_inv_range;
        out.payload.t18[i].color_end = {
            0.0f,
            0.0f,
            0.0f,
            end
        };
        out.payload.t18[i].metadata = {
            0u,0u,0u,0u
        };

        // t19 owns the untransformed source signal. Keep all four lanes for
        // provenance even though the current direct-PTDE PntS consumer reads
        // RGB and obtains End from the geometry record.
        out.payload.t19[i] =
            source.raw_q_end;
    }

    out.payload.raw_selected_count =
        raw_selected_count;
    out.payload.material_max_pnt_lit_num =
        material_max_pnt_lit_num;
    out.payload.effective_count =
        effective_count;
    out.payload.ready = true;
    out.result = clustered_sidecar_result_v1::ready;
    return out;
}

} // namespace dsrrl::operators::point_light
