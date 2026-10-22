#include "dsrrl/operators/env_spec/classic_envdiffuse_rx34_decode_v365.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

namespace v=dsrrl::operators::env_spec::classic_envdiffuse_rx34_v365;
static_assert(v::k_stock_bytes==19556u);
static_assert(v::k_candidate_bytes==19604u);
static_assert(v::k_dsr_t11_sample[8]==11u);
static_assert(v::k_dsr_t11_sample[10]==11u);
static_assert(v::k_ptde_rgb_div_and_mov.size()==12u);
int main() {
    std::vector<std::uint8_t> out{1u};
    assert(v::materialize(nullptr,0u,out)==v::result::not_exact_rx34);
    assert(out.empty());
    std::array<std::uint8_t,v::k_stock_bytes> bad{};
    assert(v::materialize(bad.data(),bad.size(),out)==v::result::not_exact_rx34);
    assert(out.empty());
    assert(v::materialize(bad.data(),bad.size()-1u,out)==v::result::not_exact_rx34);
    assert(out.empty());
    return 0;
}
