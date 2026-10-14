#include "dsrrl/runtime/pmetal_donor_provenance_policy.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using dsrrl::runtime::pmetal_donor_provenance::row_origin;
using dsrrl::runtime::pmetal_donor_provenance::classify;

struct bank_test {
    std::uint32_t live;
    std::uint32_t donor;
};
constexpr std::array<bank_test,20> banks{{
    {64,64},{64,64},{64,64},{30,30},{64,64},
    {64,64},{64,64},{64,64},{65,64},{65,64},
    {64,64},{64,64},{64,64},{64,64},{64,64},
    {64,64},{64,64},{64,64},{65,64},{64,64}
}};
constexpr std::uint32_t count_dsr_only() {
    std::uint32_t total = 0;
    for(const auto &b : banks) {
        for(std::uint32_t id=0;id<b.live;++id) {
            if(classify(id,b.live,b.donor)==row_origin::dsr_only)
                ++total;
        }
    }
    return total;
}
static_assert(count_dsr_only()==3u);
static_assert(classify(64u,65u,64u)==row_origin::dsr_only);
static_assert(classify(63u,65u,64u)==row_origin::ptde_exact);
static_assert(classify(65u,65u,64u)==row_origin::invalid);
static_assert(classify(3u,2u,4u)==row_origin::invalid);
static_assert(classify(0u,0u,0u)==row_origin::invalid);
int main() {
    std::uint32_t live=0u,ptde=0u,dsr=0u;
    for(const auto &bank:banks) {
        live+=bank.live;ptde+=bank.donor;
        for(std::uint32_t row=0;row<bank.live;++row) {
            const auto kind=classify(row,bank.live,bank.donor);
            if(kind==row_origin::invalid) return 2;
            if(kind==row_origin::dsr_only) ++dsr;
        }
    }
    if(live!=1249u||ptde!=1246u||dsr!=3u)return 3;
    std::cout<<"V299_DSR_ONLY_LIGHTBANK_DONOR_GUARD_PASS 1249/1246/3\n";
}
