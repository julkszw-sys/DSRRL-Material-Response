#include "dsrrl/operators/resource_bridges/subsurface_route.hpp"

#include <cstddef>
#include <iostream>

using namespace dsrrl::operators::resource_bridges;

namespace {

bool check(bool condition, const char *expression, int line)
{
    if (condition)
        return true;

    std::cerr << "CHECK FAILED line " << line << ": "
              << expression << '
';
    return false;
}

#define CHECK(expr) do { if (!check(static_cast<bool>(expr), #expr, __LINE__)) return 1; } while (false)

subsurface_route_context valid_context(
    std::size_t receiver_index,
    subsurface_body_texture body_texture)
{
    subsurface_route_context context;
    context.actual_material_verified = true;
    context.actual_material_name = k_dsr_body_subsurf_material;
    context.actual_material_sha256 =
        k_dsr_body_subsurf_material_sha256;

    context.actual_receiver_verified = true;
    context.actual_receiver_name =
        k_subsurface_receiver_routes[receiver_index].dsr_receiver_name;
    context.actual_receiver_sha256 =
        k_subsurface_receiver_routes[receiver_index].dsr_receiver_sha256;

    context.actual_body_texture_verified = true;
    context.body_spec_texture = body_texture;
    context.stable_hemenv_no_pointlight_draw_verified = true;

    context.ptde_slot_mapping_verified = true;
    context.ptde_donor_verified = true;
    context.ptde_material_name = k_ptde_body_plain_material;
    context.ptde_material_sha256 =
        k_ptde_body_plain_material_sha256;
    context.ptde_subsurface_usage_verified = true;
    context.ptde_uses_subsurface = false;
    context.ptde_plain_surface_target_verified = true;

    context.target_plain_receiver_ready = true;
    context.spec_rgb_route_ready = true;
    context.diffuse_route_ready = true;
    context.normal_route_ready = true;
    context.material_response_route_ready = true;
    context.dsr_subsurf_bypass_carrier_ready = true;
    return context;
}

} // namespace

int main()
{
    CHECK(k_subsurface_receiver_routes[0].target_plain_receiver_id == 33u);
    CHECK(k_subsurface_receiver_routes[1].target_plain_receiver_id == 34u);
    CHECK(k_subsurface_receiver_routes[2].target_plain_receiver_id == 35u);

    for (std::size_t i = 0; i < 3u; ++i) {
        auto context = valid_context(
            i,
            i == 0u
                ? subsurface_body_texture::bd_f_body_s
                : subsurface_body_texture::bd_m_body_s);

        const auto decision = evaluate_subsurface_route(context);
        CHECK(decision.action ==
              subsurface_route_action::route_to_ptde_plain_difspcbmp_surface);
        CHECK(decision.reason == subsurface_route_reason::active);
        CHECK(decision.target_plain_receiver_id ==
              33u + static_cast<std::uint32_t>(i));
        CHECK(decision.ptde_c101 == 1.0f);
        CHECK(decision.bypass_dsr_subsurf);
        CHECK(!decision.preserve_dsr_sss);
    }

    auto context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);

    context.actual_material_sha256 =
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    auto decision = evaluate_subsurface_route(context);
    CHECK(decision.action == subsurface_route_action::preserve_host);
    CHECK(decision.reason ==
          subsurface_route_reason::wrong_dsr_material_identity);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.actual_receiver_name =
        "FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo";
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::unsupported_receiver_identity);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.actual_receiver_sha256 =
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::unsupported_receiver_identity);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.actual_body_texture_verified = false;
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::body_texture_not_verified);

    context = valid_context(
        0u,
        subsurface_body_texture::unknown);
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::unsupported_body_texture);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.stable_hemenv_no_pointlight_draw_verified = false;
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::draw_path_not_verified);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.ptde_slot_mapping_verified = false;
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::ptde_slot_mapping_not_verified);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.ptde_material_sha256 =
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::wrong_ptde_donor_identity);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.ptde_plain_surface_target_verified = false;
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::ptde_plain_surface_target_not_verified);
    CHECK(decision.preserve_dsr_sss);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.target_plain_receiver_ready = false;
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::target_plain_receiver_not_ready);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.spec_rgb_route_ready = false;
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::spec_rgb_route_not_ready);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.diffuse_route_ready = false;
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::diffuse_route_not_ready);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.normal_route_ready = false;
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::normal_route_not_ready);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.material_response_route_ready = false;
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::material_response_route_not_ready);

    context = valid_context(
        0u,
        subsurface_body_texture::bd_f_body_s);
    context.dsr_subsurf_bypass_carrier_ready = false;
    decision = evaluate_subsurface_route(context);
    CHECK(decision.reason ==
          subsurface_route_reason::dsr_subsurf_bypass_carrier_not_ready);
    CHECK(decision.action == subsurface_route_action::preserve_host);

    std::cout << "subsurface_route_tests: PASS
";
    return 0;
}
