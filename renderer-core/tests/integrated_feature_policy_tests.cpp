#include "dsrrl/runtime/integrated_feature_policy.hpp"

#include <iostream>

#define CHECK(x) do { if (!(x)) { \
    std::cerr << "CHECK failed: " #x "\n"; return 1; \
} } while (0)

int main()
{
    using namespace dsrrl;

    CHECK(runtime::integrated_feature_policy_is_ordered_complete());
    CHECK(runtime::integrated_boot_enabled_count() == 10u);

    CHECK(runtime::integrated_boot_enabled(core::operator_id::material_response));
    CHECK(runtime::integrated_boot_enabled(core::operator_id::spec_rgb));
    CHECK(runtime::integrated_boot_enabled(core::operator_id::diffuse));
    CHECK(runtime::integrated_boot_enabled(core::operator_id::normal));
    CHECK(runtime::integrated_boot_enabled(core::operator_id::subsurface));

    CHECK(!runtime::integrated_boot_enabled(core::operator_id::upper_lower));
    CHECK(!runtime::integrated_boot_enabled(core::operator_id::hemdir3));
    CHECK(!runtime::integrated_boot_enabled(core::operator_id::env_spec));
    CHECK(!runtime::integrated_boot_enabled(core::operator_id::env_diffuse));
    CHECK(!runtime::integrated_boot_enabled(core::operator_id::point_light));
    CHECK(!runtime::integrated_boot_enabled(core::operator_id::local_specular_legacy));
    CHECK(!runtime::integrated_boot_enabled(core::operator_id::faceeye_shadow_legacy));
    CHECK(!runtime::integrated_boot_enabled(core::operator_id::pmetal_black_safe_source));
    CHECK(!runtime::integrated_boot_enabled(core::operator_id::pmetal_black_safe_v10));

    CHECK(runtime::integrated_feature_entry_for(
        core::operator_id::post_bloom).stage == runtime::integrated_stage::blocked);
    CHECK(runtime::integrated_feature_entry_for(
        core::operator_id::post_hdr).stage == runtime::integrated_stage::blocked);
    CHECK(runtime::integrated_feature_entry_for(
        core::operator_id::terminal_sat_rgba).stage == runtime::integrated_stage::rejected);
    CHECK(runtime::integrated_feature_entry_for(
        core::operator_id::dsr_native_sfx).stage == runtime::integrated_stage::host_preserve);

    std::cout << "dsrrl_renderer_integrated_policy_tests: PASS\n";
    return 0;
}
