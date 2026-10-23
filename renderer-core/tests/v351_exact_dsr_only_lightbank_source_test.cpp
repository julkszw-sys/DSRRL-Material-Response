#include "dsrrl/runtime/v300_dsr_only_lightbank_donors.hpp"
#include <cassert>
using namespace dsrrl::runtime::v300;
int main() {
    static_assert(k_dsr_only.size()==3u,"must retain exact three typed rows");
    const float diffuse_expected[3]={15.f,5.f,1.5f};
    const float spec_expected[3]={2.f,2.f,1.5f};
    for(unsigned i=0;i<3;++i) {
        const auto &d=k_dsr_only[i];
        assert(d.row_id==64u && d.live_count==65u && d.ptde_donor_count==64u);
        auto selected=classify_dsr_only(d.signature,d.layout_signature,d.live_count,
                                        d.ptde_donor_count,64u);
        assert(selected.provenance==donor_provenance::dsr_only);
        assert(selected.dsr_values==&d);
        const auto ed=decode_linear(d.dsr_envdiffuse);
        const auto es=decode_linear(d.dsr_envspec);
        for(unsigned c=0;c<3;++c) {
            assert(ed[c]==diffuse_expected[i]);
            assert(es[c]==spec_expected[i]);
        }
        assert(classify_dsr_only(d.signature,d.layout_signature,d.live_count,
                                 d.ptde_donor_count,63u).provenance==donor_provenance::unknown);
        assert(classify_dsr_only(d.signature,d.layout_signature^1ull,d.live_count,
                                 d.ptde_donor_count,64u).provenance==donor_provenance::unknown);
        assert(classify_dsr_only(d.signature,d.layout_signature,64u,
                                 d.ptde_donor_count,64u).provenance==donor_provenance::unknown);
    }
}
