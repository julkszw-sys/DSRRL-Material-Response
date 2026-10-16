#include "dsrrl/runtime/v316_strict_744_sidecar_gate.hpp"
#include <cassert>
#include <cstdint>

int main() {
    using namespace dsrrl::runtime;
    using v316::literal_spec_hash;
    static_assert(v316::strict_source_count()==744u);
    assert(sidecar_20261010::k_dsr_direct_spec_hashes.size()==749u);
    assert(v316::strict_direct_source(literal_spec_hash("am_a_1000_s")));
    assert(v316::strict_direct_source(literal_spec_hash("wp_a_0100_s")));
    assert(!v316::strict_direct_source(literal_spec_hash("am_f_9450_l_s")));
    assert(!v316::strict_direct_source(literal_spec_hash("wp_a_0652_l_s")));
    assert(!v316::strict_direct_source(literal_spec_hash("wp_a_0911_s")));
    assert(!v316::strict_direct_source(literal_spec_hash("wp_a_1004_s")));
    for(const auto low : v316::k_dsr_direct_low_suffix) {
        assert(sidecar_20261010::dsr_direct_spec_allowed(low));
        assert(!v316::strict_direct_source(low));
        assert(!v316::allow_sidecar_equipment_name(low));
    }
    assert(v316::low_suffix_policy_filter_events.load()==5u);
    assert(v316::allow_sidecar_equipment_name(literal_spec_hash("am_a_1000_s")));
    assert(v316::low_suffix_policy_filter_events.load()==5u);
}
