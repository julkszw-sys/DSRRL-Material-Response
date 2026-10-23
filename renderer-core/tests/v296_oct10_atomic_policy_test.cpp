#include "dsrrl/runtime/v296_oct10_material_atomic_gate.hpp"
#include <cassert>

using dsrrl::runtime::v296::decide_material_policy;
using dsrrl::runtime::v296::material_atomic_policy;
using dsrrl::runtime::v296::suppress_partial_material_response;

static_assert(decide_material_policy(
    false, true, true, true) ==
    material_atomic_policy::stock_material_fail_open);
static_assert(decide_material_policy(
    true, true, true, true) ==
    material_atomic_policy::exact_ptde_island);
static_assert(decide_material_policy(
    false, true, false, true) ==
    material_atomic_policy::independent_operator);
static_assert(decide_material_policy(
    false, false, true, true) ==
    material_atomic_policy::independent_operator);
static_assert(decide_material_policy(
    false, true, true, false) ==
    material_atomic_policy::independent_operator);
static_assert(suppress_partial_material_response(
    decide_material_policy(false, true, true, true)));
static_assert(!suppress_partial_material_response(
    decide_material_policy(true, true, true, true)));

int main() {
    // A DDS load failure does not change October manifest membership.
    // The classifier does not admit a 'companion_ready' parameter.
    assert(suppress_partial_material_response(
        decide_material_policy(false, true, true, true)));
}
