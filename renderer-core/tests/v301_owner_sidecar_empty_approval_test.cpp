#include "dsrrl/runtime/hemenvlerp_v301_owner_sidecar.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    using dsrrl::runtime::hemenvlerp_v301::load;
    std::array<std::uint8_t,256> unapproved_source{};
    unapproved_source[0]='D';unapproved_source[1]='X';
    unapproved_source[2]='B';unapproved_source[3]='C';
    for(std::uint32_t rx: {32u,33u,34u,35u,36u}) {
        std::vector<std::uint8_t> output{1u,2u};
        if(load(rx,unapproved_source.data(),unapproved_source.size(),output)
           || !output.empty()) return 2;
    }
    std::vector<std::uint8_t> output{1u};
    if(load(33u,nullptr,0u,output) || !output.empty()) return 3;
    static_assert(dsrrl::runtime::hemenvlerp_v301::approved.size()==0u,
                  "Unverified shader approval was accidentally enabled");
    std::cout<<"V301_EMPTY_OWNER_SHA_APPROVAL_FAIL_OPEN_PASS\n";
    return 0;
}
