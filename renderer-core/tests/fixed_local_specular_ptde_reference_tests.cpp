#include "dsrrl/operators/point_light/fixed_local_specular_ptde_reference.hpp"

#include <array>
#include <cstdint>
#include <iostream>

using namespace dsrrl::operators::point_light;

namespace {
std::uint8_t nibble(char c)
{
    if(c>='0'&&c<='9') return static_cast<std::uint8_t>(c-'0');
    if(c>='a'&&c<='f') return static_cast<std::uint8_t>(10+c-'a');
    if(c>='A'&&c<='F') return static_cast<std::uint8_t>(10+c-'A');
    return 0xffu;
}

std::array<std::uint8_t,32> digest(const char *hex)
{
    std::array<std::uint8_t,32> out{};
    for(std::size_t i=0;i<out.size();++i)
        out[i]=static_cast<std::uint8_t>((nibble(hex[i*2])<<4u)|nibble(hex[i*2+1]));
    return out;
}

bool check(bool c,const char *e,int l)
{
    if(c) return true;
    std::cerr<<"CHECK FAILED line "<<l<<": "<<e<<'\n';
    return false;
}
#define CHECK(e) do { if(!check(static_cast<bool>(e),#e,__LINE__)) return 1; } while(false)
}

int main()
{
    CHECK(fixed_local_specular_ptde_reference_count()==48u);

    const auto dsr=digest(
        "021cbb62d1cca4bad3789c50c3aa551b893904910dbf6596aedb369814d9001f");

    fixed_local_specular_ptde_reference ref{};
    CHECK(fixed_local_specular_ptde_reference_for_digest(
        dsr,24564u,fixed_local_specular_ptde_profile::hemenv,ref));
    CHECK(ref.light_count==2u);
    CHECK(ref.ptde_size==6512u);
    CHECK(ref.ptde_sha256==digest(
        "1da7f99af8f25422d32a2f310804cfb53f03d08cb1897a75a630f686e4836610"));

    CHECK(fixed_local_specular_ptde_reference_for_digest(
        dsr,24564u,fixed_local_specular_ptde_profile::hemenvlerp,ref));
    CHECK(ref.light_count==2u);
    CHECK(ref.ptde_size==6872u);
    CHECK(ref.ptde_sha256==digest(
        "76933abd5b419b39715dd3f7b0c92a7b9e04a3f6b8aad9377a9d17113aae87e8"));

    CHECK(!fixed_local_specular_ptde_reference_for_digest(
        dsr,24563u,fixed_local_specular_ptde_profile::hemenv,ref));

    auto bad=dsr;
    bad[0]^=0xffu;
    CHECK(!fixed_local_specular_ptde_reference_for_digest(
        bad,24564u,fixed_local_specular_ptde_profile::hemenv,ref));

    std::cout<<"fixed_local_specular_ptde_reference_tests: PASS\n";
    return 0;
}
