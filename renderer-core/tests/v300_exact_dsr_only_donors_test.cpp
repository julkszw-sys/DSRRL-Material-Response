#include "dsrrl/runtime/v300_dsr_only_lightbank_donors.hpp"
#include <cassert>
using namespace dsrrl::runtime::v300;

static_assert(k_dsr_only.size()==3u);
static_assert(k_dsr_only[0].row_id==64u);
static_assert(k_dsr_only[1].row_id==64u);
static_assert(k_dsr_only[2].row_id==64u);
static_assert(k_dsr_only[0].dsr_envdiffuse[3]==1500u);
static_assert(k_dsr_only[0].dsr_envspec[3]==200u);
static_assert(k_dsr_only[1].dsr_envdiffuse[3]==500u);
static_assert(k_dsr_only[1].dsr_envspec[3]==200u);
static_assert(k_dsr_only[2].dsr_envdiffuse[3]==150u);
static_assert(k_dsr_only[2].dsr_envspec[3]==150u);
static_assert(classify_dsr_only(0xc9b879a8c5e624d0ULL,
    0xac99bb1bdf5a164aULL,65u,64u,64u).provenance ==
    donor_provenance::dsr_only);
static_assert(classify_dsr_only(0xc9b879a8c5e624d0ULL,
    0xac99bb1bdf5a164aULL,65u,64u,63u).provenance ==
    donor_provenance::unknown);
static_assert(classify_dsr_only(0xc9b879a8c5e624d0ULL,
    0xac99bb1bdf5a164aULL,65u,64u,65u).provenance ==
    donor_provenance::unknown);
static_assert(classify_dsr_only(0xc9b879a8c5e624d0ULL,
    0xac99bb1bdf5a164bULL,65u,64u,64u).provenance ==
    donor_provenance::unknown);
static_assert(classify_dsr_only(0xc9b879a8c5e624d0ULL,
    0xac99bb1bdf5a164aULL,64u,64u,64u).provenance ==
    donor_provenance::unknown);
static_assert(decode_linear(k_dsr_only[0].dsr_envdiffuse)[0]==15.0f);
static_assert(decode_linear(k_dsr_only[1].dsr_envdiffuse)[0]==5.0f);
static_assert(decode_linear(k_dsr_only[2].dsr_envdiffuse)[0]==1.5f);
static_assert(decode_linear(k_dsr_only[0].dsr_envspec)[0]==2.0f);
static_assert(requires_stock_dsr(classify_dsr_only(
    0xc9b879a8c5e624d0ULL,0xac99bb1bdf5a164aULL,65u,64u,64u)));
int main(){
    auto x=classify_dsr_only(
      0x1ecfd1e617c59071ULL,0x4233f14d61b764beULL,65u,64u,64u);
    assert(x.dsr_values && x.dsr_values->dsr_envspec[3]==150u);
}
