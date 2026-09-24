#include "dsrrl/runtime/mr_dxbc_transform.hpp"
#include <iostream>
#include <string>

using namespace dsrrl::runtime::mr;

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
    const std::uint8_t junk[32]={};
    const auto r=transform(junk,k_plans[0],variant::full_v211);
    CHECK(!r.ok);
    CHECK(!r.error.empty());
    std::cout<<"dsrrl_runtime_v1_tests: PASS\n";
    return 0;
}
