#include "dsrrl/runtime/pointlight_ptde_source.hpp"
#include <cstdlib>
#include <limits>
using namespace dsrrl::runtime;
void check(bool x) { if(!x) std::abort(); }
int main() {
    // Numeric-payload identity remains available as a static corpus helper,
    // but runtime donor routing uses exact logical bank structure identity.
    for(const auto &b:pointlight_donors::banks) {
        check(pointlight_ptde_source::identify(b.dsr.data())!=nullptr);
        auto bad=b.dsr; bad[13].intensity^=1;
        check(pointlight_ptde_source::identify(bad.data())==nullptr);
    }
    for(std::size_t i=0;i<pointlight_donors::banks.size();++i) {
        const auto sig=
            pointlight_bank_structure_authority_v1::k_signatures[i];
        check(pointlight_ptde_source::identify_structure(sig)==
              &pointlight_donors::banks[i]);
    }
    for(const auto sig:
        pointlight_bank_structure_authority_v1::k_known_non_donor_signatures)
        check(pointlight_ptde_source::identify_structure(sig)==nullptr);
    check(pointlight_ptde_source::identify_structure(0u)==nullptr);
    pointlight_ptde_source::signal a{1,5,{1,2,3}},b{3,9,{3,6,9}},out;
    check(pointlight_ptde_source::mix(a,b,0.25f,out));
    check(out.begin==1.5f && out.end==6 && out.q[0]==1.5f && out.q[2]==4.5f);
    check(pointlight_ptde_source::mix(a,b,-1,out)&&out.end==5);
    check(pointlight_ptde_source::mix(a,b,2,out)&&out.end==9);
    check(!pointlight_ptde_source::mix(a,b,std::numeric_limits<float>::quiet_NaN(),out));
    a.end=a.begin; check(!pointlight_ptde_source::mix(a,a,0,out));
    pointlight_donors::row row{0x3f000000u,0x40a00000u,255,128,0,120};
    const auto s=pointlight_ptde_source::decode(row);
    check(s.begin==0.5f&&s.end==5.0f&&std::abs(s.q[0]-1.2f)<0.000001f&&s.q[2]==0);
}
