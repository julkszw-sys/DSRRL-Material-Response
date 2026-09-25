#include "dsrrl/runtime/spec_rgb_resource_cache.hpp"

#include <iostream>

using namespace dsrrl;

namespace {
bool check(bool c, const char *e, int l) { if (c) return true; std::cerr << "CHECK FAILED line " << l << ": " << e << '\n'; return false; }
#define CHECK(e) do { if (!check(static_cast<bool>(e), #e, __LINE__)) return 1; } while(false)

runtime::spec_rgb_bind_request ready_request()
{
    runtime::spec_rgb_bind_request request;
    request.route.receiver_id = 33;
    request.route.actual_material_verified = true;
    request.route.material_specular_consumer_verified = true;
    request.route.exact_name_ptde_companion_verified = true;
    request.route.ptde_sidecar_ready = true;
    request.route.native_t10_transport_ready = true;
    request.route.stock_t1_preserved = true;
    request.logical_name = u"chr/c0000/tex/body_s";
    request.currently_bound_t1 = 0x101u;
    return request;
}
}

int main()
{
    runtime::spec_rgb_resource_cache cache;
    CHECK(cache.register_exact(u"", 0x101u, 0x201u) == runtime::spec_rgb_resource_result::fail_open_invalid_identity);
    CHECK(cache.register_exact(u"chr/c0000/tex/body_s", 0, 0x201u) == runtime::spec_rgb_resource_result::fail_open_invalid_resource);
    CHECK(cache.register_exact(u"chr/c0000/tex/body_s", 0x101u, 0x201u) == runtime::spec_rgb_resource_result::registered);
    CHECK(cache.size() == 1u);

    auto request = ready_request();
    auto plan = cache.plan_bind(request);
    CHECK(plan.activate); CHECK(plan.preserved_t1_srv == 0x101u); CHECK(plan.ptde_t10_srv == 0x201u);
    CHECK(plan.route.ptde_rgb_srv_slot == 10u); CHECK(plan.route.preserve_stock_t1);

    request.currently_bound_t1 = 0x102u; plan = cache.plan_bind(request); CHECK(!plan.activate); CHECK(plan.ptde_t10_srv == 0u);
    request = ready_request(); request.route.actual_material_verified = false; plan = cache.plan_bind(request);
    CHECK(!plan.activate); CHECK(plan.route.reason == operators::resource_bridges::spec_rgb_reason::material_not_verified);

    CHECK(cache.register_exact(u"chr/c0000/tex/body_s", 0x101u, 0x202u) == runtime::spec_rgb_resource_result::fail_open_conflicting_companion);
    request = ready_request(); plan = cache.plan_bind(request); CHECK(!plan.activate);

    cache.clear(); CHECK(cache.size() == 0u);
    CHECK(cache.register_exact(u"a", 0x301u, 0x401u) == runtime::spec_rgb_resource_result::registered);
    CHECK(cache.register_exact(u"b", 0x301u, 0x402u) == runtime::spec_rgb_resource_result::fail_open_conflicting_stock_binding);

    // The conflict invalidates the old owner too. Retaining "a" as active here
    // would allow a stale exact-name association after ownership became ambiguous.
    runtime::spec_rgb_bind_request conflict = ready_request();
    conflict.logical_name = u"a"; conflict.currently_bound_t1 = 0x301u;
    plan = cache.plan_bind(conflict); CHECK(!plan.activate); CHECK(plan.ptde_t10_srv == 0u);

    cache.erase_stock(0x301u); // reverse owner was already quarantined
    CHECK(cache.size() == 1u); // ambiguous tombstone intentionally survives
    cache.clear(); CHECK(cache.size() == 0u);

    std::cout << "dsrrl_spec_rgb_resource_cache_tests: PASS\n";
    return 0;
}
