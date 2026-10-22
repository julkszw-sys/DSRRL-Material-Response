#include "dsrrl/operators/env_spec/legacy_envdiffuse_verified_identity_v360.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cwchar>
using namespace dsrrl::operators::env_spec::legacy_diffuse_verified;

constexpr std::array<std::uint8_t,32> decode(const char *hex) {
    std::array<std::uint8_t,32> d{};
    for(std::size_t i=0;i<32;i++) {
        const auto hi=hex_digit(hex[i*2]);
        const auto lo=hex_digit(hex[i*2+1]);
        d[i]=static_cast<std::uint8_t>((hi<<4)|lo);
    }
    return d;
}
constexpr auto sha500=decode(k_exact_owner_rx34[0].sha256_hex);
constexpr auto sha510=decode(k_exact_owner_rx34[1].sha256_hex);
static_assert(exact_digest(sha500,k_exact_owner_rx34[0].sha256_hex));
static_assert(exact_digest(sha510,k_exact_owner_rx34[1].sha256_hex));
static_assert(exact_name(L"EnvDif_m18_500",14u,"EnvDif_m18_500"));
static_assert(exact_name(L"EnvDif_m18_510",14u,"EnvDif_m18_510"));
static_assert([] {
    identity id{};
    return resolve(L"EnvDif_m18_500",14u,sha500,id) &&
           id.map==18u && id.legacy_row==500u;
}());
static_assert([] {
    identity id{};
    return resolve(L"EnvDif_m18_510",14u,sha510,id) &&
           id.map==18u && id.legacy_row==510u;
}());
static_assert([] {
    identity id{9u,999u};
    return !resolve(L"EnvDif_m18_500",14u,sha510,id) &&
           id.map==0u && id.legacy_row==0u;
}());
static_assert([] {
    identity id{};
    return !resolve(L"EnvDif_m10_500",14u,sha500,id) &&
           !resolve(L"EnvDif_m18_500",13u,sha500,id) &&
           !resolve(L"GI_EnvDif_m18_500",17u,sha500,id);
}());
int main() {
    identity out{};
    assert(resolve(L"EnvDif_m18_500",std::wcslen(L"EnvDif_m18_500"),sha500,out));
    assert(out.map==18u && out.legacy_row==500u);
    assert(resolve(L"EnvDif_m18_510",std::wcslen(L"EnvDif_m18_510"),sha510,out));
    assert(out.map==18u && out.legacy_row==510u);
    return 0;
}
