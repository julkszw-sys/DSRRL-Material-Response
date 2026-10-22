#pragma once
// V359 descendant: exactly observed retail legacy DSR EnvDiffuse resources.
// Native identity ONLY. This file does NOT authorize a PTDE donor, a shader
// island, t12 EnvSpec, or changing a pixel.
//
// The two tuples below are confirmed independently in multiple owner runtime
// creation-SHA logs and on the exact RX34 stock t11 backing D3D11 resources.
// Source name comes from the retail loader TLS at the SAME init_resource call;
// name alone or SHA alone is never sufficient.
#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::env_spec::legacy_diffuse_verified {

struct identity {
    std::uint16_t map = 0u;
    std::uint16_t legacy_row = 0u;
    // This is NOT the packed-GI ordinal and must never index the 342 atlas.
};
struct name_digest {
    const char *name;
    const char *sha256_hex;
    identity id;
};
constexpr name_digest k_exact_owner_rx34[] = {
    { "EnvDif_m18_500",
      "19241a18f938ea8fa00dcaede57e9116c60cf818d5c8faaa1cc93e7eb98ce81b",
      {18u,500u} },
    { "EnvDif_m18_510",
      "cace4d9bdbe2bd54bb9b4e7c91002b88c6a68c367638febde9c76144ee5a7eb9",
      {18u,510u} },
};

#include "dsrrl/operators/env_spec/legacy_dsr_m10_create_roster_v363.inc"
#include "dsrrl/operators/env_spec/legacy_dsr_m18_create_roster_v363.inc"
constexpr int hex_digit(char c) noexcept
{
    return c >= '0' && c <= '9' ? c-'0' :
        c >= 'a' && c <= 'f' ? c-'a'+10 : -1;
}
constexpr bool exact_name(const wchar_t *name,std::size_t count,
                          const char *expected) noexcept
{
    if(name==nullptr || expected==nullptr || count==0u || count>32u)
        return false;
    std::size_t n=0u;
    for(; expected[n]!='\0';++n) {
        if(n>=count || name[n]!=static_cast<wchar_t>(expected[n]))
            return false;
    }
    return n==count;
}
constexpr bool exact_digest(const std::array<std::uint8_t,32> &digest,
                            const char *hex) noexcept
{
    if(hex==nullptr) return false;
    for(std::size_t i=0u;i<32u;++i) {
        const int high=hex_digit(hex[2u*i]);
        const int low=hex_digit(hex[2u*i+1u]);
        if(high<0 || low<0 ||
           digest[i]!=static_cast<std::uint8_t>((high<<4)|low))
            return false;
    }
    return hex[64u]=='\0';
}
// Output remains empty on uncertainty. No fuzzy match, no cross-family alias.
constexpr bool resolve(const wchar_t *name,std::size_t count,
                       const std::array<std::uint8_t,32> &digest,
                       identity &out) noexcept
{
    out={};
    bool found=false;
    for(const auto &entry:k_exact_owner_rx34) {
        if(exact_name(name,count,entry.name) &&
           exact_digest(digest,entry.sha256_hex)) {
            if(found) {out={};return false;}
            out=entry.id;
            found=true;
        }
    }
    for(const auto &entry:k_native_created_m10) {
        if(exact_name(name,count,entry.name) &&
           exact_digest(digest,entry.sha256_hex)) {
            if(found) {out={};return false;}
            out=entry.id;found=true;
        }
    }
    for(const auto &entry:k_native_created_m18_more) {
        if(exact_name(name,count,entry.name) &&
           exact_digest(digest,entry.sha256_hex)) {
            if(found) {out={};return false;}
            out=entry.id;found=true;
        }
    }
    return found;
}
} // namespace dsrrl::operators::env_spec::legacy_diffuse_verified
