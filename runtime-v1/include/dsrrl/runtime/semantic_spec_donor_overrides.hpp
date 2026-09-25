#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace dsrrl::runtime {

// Exact semantic recovery for the four DSR raw-MTD hashes that collapse two
// distinct PTDE specular donors each. Raw SHA remains sufficient for c100, but
// c101/c102 must not be recovered from these hashes without the semantic BND
// basename.
//
// Provenance:
//   DSR Mtd.mtdbnd.dcx SHA256
//     7c5a0952c1016a3efa3c6a7180bc60af81ccf7b2a4685c3c89028eda78c2d8b5
//   decoded DSR BND3 SHA256
//     621f787733e8fc9cd08eaac39ef433394a57d9328342c41fec77766e793344c9
//   decoded PTDE BND3 SHA256
//     857d58c36e47536086104059c5146500fd8afe5dbef90c5d29b8216d32928e56
//
// The eight tuples below are direct PTDE MTD values:
//   c101 = g_SpecularMapColor.rgb * g_SpecularMapColorPower
//   c101_f0q = max(c101,0)^(1/2.2)
//   c102 = g_SpecularPower
//   slot = g_EnvSpcSlotNo
struct semantic_spec_donor_override {
    std::wstring_view mtd_basename;
    std::string_view raw_sha256;
    std::array<float,3> c101{};
    std::array<float,3> c101_f0q{};
    float c102=0.0f;
    int slot=0;
};

inline constexpr std::array<semantic_spec_donor_override,8>
k_semantic_spec_donor_overrides = {{
    {
        L"A03_7Metal[DSB][L].mtd",
        "a557affd0cddeea208bdb3f1208a5a9bafd7a1dc20f7e5d194b827046c35da59",
        {2.0f,2.0f,2.0f},
        {1.37035098f,1.37035098f,1.37035098f},
        7.0f,2
    },
    {
        L"M_7Metal[DSB][L].mtd",
        "a557affd0cddeea208bdb3f1208a5a9bafd7a1dc20f7e5d194b827046c35da59",
        {2.5f,2.5f,2.5f},
        {1.51663761f,1.51663761f,1.51663761f},
        8.5f,2
    },
    {
        L"P_Damage_L_DullLeather[DSB].mtd",
        "0f8ca2b6896c9fbe81bfdb30eb0d53a48670cb4c1719ab35533a05f03c46cc00",
        {0.25f,0.25f,0.25f},
        {0.532520545f,0.532520545f,0.532520545f},
        0.5f,0
    },
    {
        L"P_Damage_L_Leather[DSB].mtd",
        "0f8ca2b6896c9fbe81bfdb30eb0d53a48670cb4c1719ab35533a05f03c46cc00",
        {0.5f,0.5f,0.5f},
        {0.729740053f,0.729740053f,0.729740053f},
        1.25f,0
    },
    {
        L"P_Damage_L_Wet[DSB].mtd",
        "28610066d4831bc7a3b4b44c3dff33bbd7cfc18b87ebaabfa5c7c0c95846e513",
        {1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f},
        25.0f,0
    },
    {
        L"P_Damage_S_Wet[DSB].mtd",
        "28610066d4831bc7a3b4b44c3dff33bbd7cfc18b87ebaabfa5c7c0c95846e513",
        {2.0f,2.0f,2.0f},
        {1.37035098f,1.37035098f,1.37035098f},
        50.0f,0
    },
    {
        L"P_Damage_S_DullLeather[DSB].mtd",
        "c28d101192023595b0d41b556b04c42f30f3b65c6a0a7662a314e2c5c9b672f1",
        {0.5f,0.5f,0.5f},
        {0.729740053f,0.729740053f,0.729740053f},
        1.0f,0
    },
    {
        L"P_Damage_S_Leather[DSB].mtd",
        "c28d101192023595b0d41b556b04c42f30f3b65c6a0a7662a314e2c5c9b672f1",
        {1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f},
        2.5f,0
    }
}};

inline int find_semantic_spec_donor_override(
    std::wstring_view semantic_name,
    std::string_view raw_sha256) noexcept
{
    const auto slash=semantic_name.find_last_of(L"\\/");
    const auto basename=
        slash==std::wstring_view::npos ?
        semantic_name :
        semantic_name.substr(slash+1u);

    for(std::size_t i=0;i<k_semantic_spec_donor_overrides.size();++i){
        const auto &entry=k_semantic_spec_donor_overrides[i];
        if(entry.raw_sha256==raw_sha256 &&
           entry.mtd_basename==basename)
            return static_cast<int>(i);
    }
    return -1;
}

} // namespace dsrrl::runtime
