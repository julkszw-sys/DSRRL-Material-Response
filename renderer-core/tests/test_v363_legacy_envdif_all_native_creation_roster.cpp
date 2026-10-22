#include "dsrrl/operators/env_spec/legacy_envdiffuse_verified_identity_v360.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
using namespace dsrrl::operators::env_spec::legacy_diffuse_verified;

constexpr std::array<std::uint8_t,32> decode(const char *sha) {
    std::array<std::uint8_t,32> result{};
    for(std::size_t i=0;i<32u;++i)
        result[i]=static_cast<std::uint8_t>(
          (hex_digit(sha[i*2u])<<4)|hex_digit(sha[i*2u+1u]));
    return result;
}
template<std::size_t N>
constexpr bool check_exact_census(const name_digest (&roster)[N]) {
    for(const auto &entry:roster) {
        wchar_t wide[40]{};
        std::size_t n=0u;
        while(entry.name[n]!='\0' && n<39u) {
            wide[n]=static_cast<wchar_t>(entry.name[n]);++n;
        }
        const auto digest=decode(entry.sha256_hex);
        identity id{};
        if(!resolve(wide,n,digest,id) ||
           id.map!=entry.id.map || id.legacy_row!=entry.id.legacy_row)
            return false;
        // The same logical name must reject a different texture SHA.
        auto wrong=digest;
        wrong[0]^=0x01u;
        if(resolve(wide,n,wrong,id)) return false;
        // Never resolve a truncated or extra character from exact name.
        if(n>0u && resolve(wide,n-1u,digest,id)) return false;
        wide[n]=L'x';
        if(resolve(wide,n+1u,digest,id)) return false;
    }
    return true;
}
static_assert(sizeof(k_exact_owner_rx34)/sizeof(k_exact_owner_rx34[0])==2u);
static_assert(sizeof(k_native_created_m10)/sizeof(k_native_created_m10[0])==14u);
static_assert(sizeof(k_native_created_m18_more)/sizeof(k_native_created_m18_more[0])==6u);
static_assert(check_exact_census(k_exact_owner_rx34));
static_assert(check_exact_census(k_native_created_m10));
static_assert(check_exact_census(k_native_created_m18_more));
int main(){return 0;}
