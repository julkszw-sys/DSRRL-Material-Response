#include "dsrrl/operators/lightbank/hemdir3.hpp"

#include <algorithm>
#include <cmath>

namespace dsrrl::operators::lightbank {
namespace {

bool finite(const hemdir3_vec3 &v) noexcept
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

float dot(const hemdir3_vec3 &a, const hemdir3_vec3 &b) noexcept
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

hemdir3_vec3 madd(
    const hemdir3_vec3 &a,
    const hemdir3_vec3 &b,
    float s) noexcept
{
    return {a.x+b.x*s,a.y+b.y*s,a.z+b.z*s};
}

constexpr float k_pi = 3.14159265358979323846f;
constexpr float k_two_pi = 2.0f * k_pi;

float wrap_pi(float x) noexcept
{
    x = std::fmod(x + k_pi, k_two_pi);
    if (x < 0.0f)
        x += k_two_pi;
    return x - k_pi;
}

float blend_angle_degrees(float a, float b, float beta) noexcept
{
    const float ar = a * (k_pi / 180.0f);
    const float br = b * (k_pi / 180.0f);
    return ar + beta * wrap_pi(br - ar);
}

hemdir3_vec3 decode_color(const hemdir3_raw_color &c) noexcept
{
    const float scale = (1.0f / 255.0f) * (c.multiplier_percent / 100.0f);
    return {c.rgb_255.x * scale, c.rgb_255.y * scale, c.rgb_255.z * scale};
}

hemdir3_vec3 blend_vec3(
    const hemdir3_vec3 &a,
    const hemdir3_vec3 &b,
    float beta) noexcept
{
    return {
        a.x + beta * (b.x - a.x),
        a.y + beta * (b.y - a.y),
        a.z + beta * (b.z - a.z)
    };
}

hemdir3_vec3 direction_from_angles(float x, float y) noexcept
{
    return {
        std::sin(y) * std::cos(x),
        -std::sin(x),
        std::cos(y) * std::cos(x)
    };
}

bool finite_endpoint(const hemdir3_raw_lobe_endpoint &e) noexcept
{
    return std::isfinite(e.direction.x_degrees) &&
           std::isfinite(e.direction.y_degrees) &&
           finite(e.color.rgb_255) &&
           std::isfinite(e.color.multiplier_percent);
}

} // namespace

hemdir3_profile_sample evaluate_hemdir3_profile(
    const std::array<hemdir3_raw_lobe_endpoint, 3> &endpoint_a,
    const std::array<hemdir3_raw_lobe_endpoint, 3> &endpoint_b,
    float beta) noexcept
{
    if (!std::isfinite(beta))
        return {};
    for (std::size_t i = 0; i < endpoint_a.size(); ++i)
        if (!finite_endpoint(endpoint_a[i]) || !finite_endpoint(endpoint_b[i]))
            return {};

    hemdir3_profile_sample out;
    for (std::size_t i = 0; i < out.lobes.size(); ++i) {
        const auto ca = decode_color(endpoint_a[i].color);
        const auto cb = decode_color(endpoint_b[i].color);

        if (beta <= 0.0f) {
            const float x = endpoint_a[i].direction.x_degrees * (k_pi / 180.0f);
            const float y = endpoint_a[i].direction.y_degrees * (k_pi / 180.0f);
            out.lobes[i] = {direction_from_angles(x, y), ca};
        } else if (beta >= 1.0f) {
            const float x = endpoint_b[i].direction.x_degrees * (k_pi / 180.0f);
            const float y = endpoint_b[i].direction.y_degrees * (k_pi / 180.0f);
            out.lobes[i] = {direction_from_angles(x, y), cb};
        } else {
            const float x = blend_angle_degrees(
                endpoint_a[i].direction.x_degrees,
                endpoint_b[i].direction.x_degrees,
                beta);
            const float y = blend_angle_degrees(
                endpoint_a[i].direction.y_degrees,
                endpoint_b[i].direction.y_degrees,
                beta);
            out.lobes[i] = {
                direction_from_angles(x, y),
                blend_vec3(ca, cb, beta)
            };
        }
    }
    out.result = hemdir3_profile_result::exact;
    return out;
}

hemdir3_sample evaluate_hemdir3_source_join(
    const hemdir3_vec3 &hemisphere,
    const hemdir3_vec3 &n_final,
    const std::array<hemdir3_lobe, 3> &lobes) noexcept
{
    hemdir3_sample out;
    if (!finite(hemisphere) || !finite(n_final))
        return out;
    for (const auto &lobe : lobes)
        if (!finite(lobe.direction) || !finite(lobe.color))
            return out;

    out.hemisphere = hemisphere;
    out.joined_source = hemisphere;

    for (std::size_t i = 0; i < lobes.size(); ++i) {
        const float w = std::max(-dot(n_final, lobes[i].direction), 0.0f);
        if (!std::isfinite(w))
            return hemdir3_sample{};
        out.weights[i] = w;
        out.joined_source = madd(out.joined_source, lobes[i].color, w);
    }

    out.result = hemdir3_math_result::exact;
    return out;
}

hemdir3_runtime_plan evaluate_hemdir3_runtime_readiness(
    const core::feature_registry &features,
    const core::activation_context &activation,
    const hemdir3_runtime_context &context) noexcept
{
    hemdir3_runtime_plan out;
    const auto core_gate =
        core::evaluate_operator_activation(
            features,
            core::operator_id::hemdir3,
            activation);

    if (core_gate.state != core::island_state::active) {
        out.reason = hemdir3_runtime_reason::core_gate_not_active;
        return out;
    }
    if (context.semantic_mode != 2u) {
        out.reason = hemdir3_runtime_reason::semantic_mode_not_hemdir3;
        return out;
    }
    // Mode 2 is not legal to infer from ordinary FrpgModel/MapModel state:
    // audited direct producers establish only {0,1}. Arm HemDir3 only when
    // runtime supplies an independently exact effective-mode2 observation.
    if (context.semantic_mode_provenance !=
        hemdir3_semantic_mode_provenance::exact_effective_mode2) {
        out.reason =
            hemdir3_runtime_reason::semantic_mode_provenance_not_verified;
        return out;
    }
    if (context.producer_snapshot.owner == 0u ||
        context.draw_snapshot.owner == 0u ||
        context.producer_snapshot.owner != context.draw_snapshot.owner) {
        out.reason = hemdir3_runtime_reason::snapshot_owner_not_verified;
        return out;
    }
    if (!lightbank_assignment_tuple_matches(
            context.producer_snapshot,
            context.draw_snapshot)) {
        out.reason =
            hemdir3_runtime_reason::snapshot_assignment_tuple_not_fresh;
        return out;
    }
    if (!context.upper_lower_source_ready) {
        out.reason = hemdir3_runtime_reason::upper_lower_source_not_ready;
        return out;
    }
    if (!context.d123_source_ready) {
        out.reason = hemdir3_runtime_reason::d123_source_not_ready;
        return out;
    }
    if (!context.b13_carrier_ready) {
        out.reason = hemdir3_runtime_reason::b13_carrier_not_ready;
        return out;
    }
    if (!context.receiver_verified) {
        out.reason = hemdir3_runtime_reason::receiver_not_verified;
        return out;
    }
    if (context.receiver_stratum == hemdir3_receiver_stratum::unsupported) {
        out.reason = hemdir3_runtime_reason::unsupported_receiver;
        return out;
    }

    if (context.receiver_stratum == hemdir3_receiver_stratum::spc) {
        out.require_b12_material_donor = true;
        out.require_directional_legacy_specular = true;

        if (!context.spc_b12_material_donor_ready) {
            out.reason =
                hemdir3_runtime_reason::spc_b12_material_donor_not_ready;
            return out;
        }
        if (!context.directional_specular_continuation_ready) {
            out.reason =
                hemdir3_runtime_reason::
                    directional_specular_continuation_not_ready;
            return out;
        }
    }

    if (!context.host_envdiffuse_source_suppressed) {
        out.reason = hemdir3_runtime_reason::host_envdiffuse_not_suppressed;
        return out;
    }
    if (!context.material_continuation_ready) {
        out.reason = hemdir3_runtime_reason::material_continuation_not_ready;
        return out;
    }
    if (!context.downstream_material_domain_ready) {
        out.reason =
            hemdir3_runtime_reason::downstream_material_domain_not_ready;
        return out;
    }
    if (!context.downstream_postfog_ready) {
        out.reason = hemdir3_runtime_reason::downstream_postfog_not_ready;
        return out;
    }
    if (!context.atmosphere_route_verified) {
        out.reason = hemdir3_runtime_reason::atmosphere_route_not_verified;
        return out;
    }
    if (!context.draw_transaction_ready) {
        out.reason = hemdir3_runtime_reason::draw_transaction_not_ready;
        return out;
    }

    out.ready = true;
    out.reason = hemdir3_runtime_reason::ready;
    return out;
}

} // namespace dsrrl::operators::lightbank
