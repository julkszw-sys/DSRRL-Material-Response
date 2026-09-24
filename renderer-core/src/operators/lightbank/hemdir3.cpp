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

} // namespace

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
