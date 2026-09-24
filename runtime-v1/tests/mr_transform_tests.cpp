#include "dsrrl/runtime/mr_dxbc_transform.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>

using namespace dsrrl::runtime::mr;

namespace {

void put_u32(std::vector<std::uint8_t> &v,std::size_t off,std::uint32_t x)
{
    if(v.size()<off+4) v.resize(off+4);
    std::memcpy(v.data()+off,&x,4);
}

std::vector<std::uint8_t> synthetic_ul_dxbc()
{
    const std::vector<std::uint32_t> words = {
        0x00000050u, 19u,
        0x04000059u,0x00208e46u,0u,9u,
        0x01000032u,
        0x07000000u,0x00208006u,0u,7u,0x00208006u,0u,8u,
        0x04000032u,0x00208006u,0u,8u,
        0x0100003eu
    };

    constexpr std::uint32_t chunks=2;
    constexpr std::uint32_t header=32+4*chunks;
    constexpr std::uint32_t rdef_off=header;
    constexpr std::uint32_t rdef_size=4;
    constexpr std::uint32_t shex_off=rdef_off+8+rdef_size;
    const std::uint32_t shex_size=static_cast<std::uint32_t>(words.size()*4u);
    const std::uint32_t total=shex_off+8+shex_size;

    std::vector<std::uint8_t> out(total,0);
    std::memcpy(out.data(),"DXBC",4);
    put_u32(out,20,1u);
    put_u32(out,24,total);
    put_u32(out,28,chunks);
    put_u32(out,32,rdef_off);
    put_u32(out,36,shex_off);

    std::memcpy(out.data()+rdef_off,"RDEF",4);
    put_u32(out,rdef_off+4,rdef_size);
    put_u32(out,rdef_off+8,0x12345678u);

    std::memcpy(out.data()+shex_off,"SHEX",4);
    put_u32(out,shex_off+4,shex_size);
    for(std::size_t i=0;i<words.size();++i)
        put_u32(out,shex_off+8+i*4,words[i]);
    return out;
}

} // namespace

#define CHECK(x) do { if(!(x)){ std::cerr<<"CHECK FAILED: "<<#x<<" line "<<__LINE__<<"\n"; return 1; } } while(false)

int main()
{
    CHECK(k_plans.size()==24u);
    for(std::size_t i=0;i<k_plans.size();++i){
        CHECK(k_plans[i].index==i);
        CHECK(k_plans[i].stock_size>16000u);
        CHECK(k_plans[i].replacement_size==k_plans[i].stock_size+16u);
        CHECK(k_plans[i].cb_sites[0]<k_plans[i].cb_sites[1]);
        CHECK(k_plans[i].original_sha256.size()==64u);
        CHECK(k_plans[i].v29_sha256.size()==64u);
        CHECK(k_plans[i].v210_sha256.size()==64u);
        CHECK(k_plans[i].v211_sha256.size()==64u);
        CHECK(find_plan(k_plans[i].stock_size,k_plans[i].original_sha256)==&k_plans[i]);
    }
    CHECK(find_plan(0,"nope")==nullptr);

    CHECK(k_ul_plans.size()==48u);
    std::size_t stable=0,lerp=0;
    for(std::size_t i=0;i<k_ul_plans.size();++i){
        CHECK(k_ul_plans[i].index==i);
        CHECK(k_ul_plans[i].original_sha256.size()==64u);
        CHECK(find_ul_plan(k_ul_plans[i].original_sha256)==&k_ul_plans[i]);
        if(k_ul_plans[i].lerp){
            ++lerp;
            CHECK(k_ul_plans[i].mr_stable_index==-1);
        }else{
            ++stable;
            CHECK(k_ul_plans[i].mr_stable_index>=0);
            CHECK(k_ul_plans[i].mr_stable_index<24);
        }
    }
    CHECK(stable==24u);
    CHECK(lerp==24u);
    CHECK(find_ul_plan("nope")==nullptr);

    const std::uint8_t junk[32]={};
    const auto r=transform(junk,k_plans[0],variant::full_v211);
    CHECK(!r.ok);
    CHECK(!r.error.empty());

    const auto synthetic=synthetic_ul_dxbc();
    const auto ul=transform_upper_lower(synthetic);
    CHECK(ul.ok);
    CHECK(!ul.code.empty());
    CHECK(ul.code.size()==synthetic.size()+4u);
    const auto twice=transform_upper_lower(ul.code);
    CHECK(!twice.ok);
    CHECK(!twice.error.empty());

    std::cout<<"dsrrl_runtime_v1_tests: PASS\n";
    return 0;
}
