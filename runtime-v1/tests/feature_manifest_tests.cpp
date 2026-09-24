#include "dsrrl/runtime/feature_manifest.hpp"

#include <cstddef>
#include <iostream>

using namespace dsrrl;

namespace {

bool check(bool condition,const char *expr,int line)
{
    if(condition) return true;
    std::cerr<<"CHECK FAILED line "<<line<<": "<<expr<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)

} // namespace

int main()
{
    using namespace runtime;

    CHECK(runtime_feature_manifest_is_ordered_complete());
    CHECK(k_runtime_feature_manifest.size()==core::operator_count);
    CHECK(runtime_boot_enabled_count()==11u);
    CHECK(runtime_boot_preflight_count()==2u);

    std::size_t current=0;
    std::size_t future_candidate=0;
    std::size_t future_partial=0;
    std::size_t blocked=0;
    std::size_t diagnostic=0;
    std::size_t host=0;
    std::size_t rejected=0;

    for(const auto &entry:k_runtime_feature_manifest){
        switch(entry.stage){
        case runtime_feature_stage::current_wired: ++current; break;
        case runtime_feature_stage::future_candidate: ++future_candidate; break;
        case runtime_feature_stage::future_partial: ++future_partial; break;
        case runtime_feature_stage::blocked_preflight: ++blocked; break;
        case runtime_feature_stage::diagnostic_only: ++diagnostic; break;
        case runtime_feature_stage::host_preserve: ++host; break;
        case runtime_feature_stage::rejected: ++rejected; break;
        }

        if(entry.stage!=runtime_feature_stage::current_wired)
            CHECK(entry.boot_policy==runtime_boot_policy::hold_off);
    }

    CHECK(current==13u);
    CHECK(future_candidate==0u);
    CHECK(future_partial==6u);
    CHECK(blocked==2u);
    CHECK(diagnostic==1u);
    CHECK(host==2u);
    CHECK(rejected==1u);

    CHECK(runtime_feature_boot_enabled(core::operator_id::material_response));
    CHECK(runtime_feature_boot_enabled(core::operator_id::spec_rgb));
    CHECK(runtime_feature_boot_enabled(core::operator_id::diffuse));
    CHECK(runtime_feature_boot_enabled(core::operator_id::normal));

    CHECK(runtime_feature_needs_boot_preflight(core::operator_id::upper_lower));
    CHECK(!runtime_feature_boot_enabled(core::operator_id::upper_lower));
    CHECK(runtime_feature_needs_boot_preflight(core::operator_id::pmetal_black_safe_source));
    CHECK(!runtime_feature_boot_enabled(core::operator_id::pmetal_black_safe_source));

    CHECK(runtime_feature_boot_enabled(core::operator_id::pmetal_black_safe_v10));
    CHECK(runtime_feature_entry_for(core::operator_id::pmetal_black_safe_v10).stage==
          runtime_feature_stage::current_wired);

    CHECK(runtime_feature_boot_enabled(core::operator_id::subsurface));
    CHECK(runtime_feature_entry_for(core::operator_id::subsurface).stage==
          runtime_feature_stage::current_wired);

    CHECK(runtime_feature_requires_detailed_readiness(core::operator_id::hemdir3));
    CHECK(runtime_feature_requires_detailed_readiness(core::operator_id::env_spec));
    CHECK(runtime_feature_requires_detailed_readiness(core::operator_id::env_diffuse));
    CHECK(runtime_feature_requires_detailed_readiness(core::operator_id::point_light));
    CHECK(runtime_feature_requires_detailed_readiness(core::operator_id::local_specular_legacy));
    CHECK(runtime_feature_requires_detailed_readiness(core::operator_id::faceeye_shadow_legacy));

    CHECK(runtime_feature_is_hard_blocked(core::operator_id::post_bloom));
    CHECK(runtime_feature_is_hard_blocked(core::operator_id::post_hdr));
    CHECK(runtime_feature_is_hard_blocked(core::operator_id::envspec_pmetal_diagnostic));
    CHECK(runtime_feature_is_hard_blocked(core::operator_id::terminal_sat_rgba));
    CHECK(runtime_feature_is_hard_blocked(core::operator_id::dsr_native_sfx));
    CHECK(runtime_feature_is_hard_blocked(core::operator_id::dsr_sfx_inverse_tonemap));

    CHECK(runtime_feature_entry_for(core::operator_id::post_bloom).stage==
          runtime_feature_stage::blocked_preflight);
    CHECK(runtime_feature_entry_for(core::operator_id::post_hdr).stage==
          runtime_feature_stage::blocked_preflight);
    CHECK(runtime_feature_entry_for(core::operator_id::dsr_native_sfx).stage==
          runtime_feature_stage::host_preserve);
    CHECK(runtime_feature_entry_for(core::operator_id::terminal_sat_rgba).stage==
          runtime_feature_stage::rejected);

    std::cout<<"runtime_feature_manifest_tests: PASS\n";
    return 0;
}
