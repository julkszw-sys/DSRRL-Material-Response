#include "dsrrl/runtime/semantic_spec_donor_overrides.hpp"

#include <cstdio>
#include <string_view>

#define CHECK(expr) do { if(!(expr)){ std::fprintf(stderr,"CHECK failed: %s\n",#expr); return 1; } } while(false)

using namespace dsrrl::runtime;

int main()
{
    static_CHECK(k_semantic_spec_donor_overrides.size()==8u);

    const auto a=find_semantic_spec_donor_override(
        L"A03_7Metal[DSB][L].mtd",
        "a557affd0cddeea208bdb3f1208a5a9bafd7a1dc20f7e5d194b827046c35da59");
    const auto m=find_semantic_spec_donor_override(
        L"N:\\FRPG\\data\\Material\\mtd\\M_7Metal[DSB][L].mtd",
        "a557affd0cddeea208bdb3f1208a5a9bafd7a1dc20f7e5d194b827046c35da59");
    CHECK(a>=0 && m>=0 && a!=m);
    CHECK(k_semantic_spec_donor_overrides[static_cast<std::size_t>(a)].c102==7.0f);
    CHECK(k_semantic_spec_donor_overrides[static_cast<std::size_t>(m)].c102==8.5f);

    const auto ld=find_semantic_spec_donor_override(
        L"P_Damage_L_DullLeather[DSB].mtd",
        "0f8ca2b6896c9fbe81bfdb30eb0d53a48670cb4c1719ab35533a05f03c46cc00");
    const auto ll=find_semantic_spec_donor_override(
        L"P_Damage_L_Leather[DSB].mtd",
        "0f8ca2b6896c9fbe81bfdb30eb0d53a48670cb4c1719ab35533a05f03c46cc00");
    CHECK(ld>=0 && ll>=0 && ld!=ll);
    CHECK(k_semantic_spec_donor_overrides[static_cast<std::size_t>(ld)].c101[0]==0.25f);
    CHECK(k_semantic_spec_donor_overrides[static_cast<std::size_t>(ll)].c101[0]==0.5f);

    const auto lw=find_semantic_spec_donor_override(
        L"P_Damage_L_Wet[DSB].mtd",
        "28610066d4831bc7a3b4b44c3dff33bbd7cfc18b87ebaabfa5c7c0c95846e513");
    const auto sw=find_semantic_spec_donor_override(
        L"P_Damage_S_Wet[DSB].mtd",
        "28610066d4831bc7a3b4b44c3dff33bbd7cfc18b87ebaabfa5c7c0c95846e513");
    CHECK(lw>=0 && sw>=0 && lw!=sw);
    CHECK(k_semantic_spec_donor_overrides[static_cast<std::size_t>(lw)].c102==25.0f);
    CHECK(k_semantic_spec_donor_overrides[static_cast<std::size_t>(sw)].c102==50.0f);

    CHECK(find_semantic_spec_donor_override(
        L"P_Damage_L_Wet[DSB].mtd",
        "c28d101192023595b0d41b556b04c42f30f3b65c6a0a7662a314e2c5c9b672f1")==-1);
    CHECK(find_semantic_spec_donor_override(
        L"Unknown.mtd",
        "28610066d4831bc7a3b4b44c3dff33bbd7cfc18b87ebaabfa5c7c0c95846e513")==-1);
    return 0;
}
