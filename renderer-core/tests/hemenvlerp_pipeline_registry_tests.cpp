#include "dsrrl/runtime/hemenvlerp_pipeline_registry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

using namespace dsrrl;

#define CHECK(x) do { if(!(x)){ std::cerr << "CHECK failed: " #x "\n"; return 1; } } while(false)

std::array<std::uint8_t,32> digest(const char *hex)
{
    std::array<std::uint8_t,32> out{};
    auto nibble=[](char ch)->int{
        if(ch>='0'&&ch<='9') return ch-'0';
        if(ch>='a'&&ch<='f') return 10+ch-'a';
        if(ch>='A'&&ch<='F') return 10+ch-'A';
        return -1;
    };
    for(std::size_t i=0;i<out.size();++i){
        const int hi=nibble(hex[i*2]);
        const int lo=nibble(hex[i*2+1]);
        if(hi<0||lo<0) return {};
        out[i]=static_cast<std::uint8_t>((hi<<4)|lo);
    }
    return out;
}

int main()
{
    runtime::hemenvlerp_receiver_pipeline_reset();

    const auto pair9=digest(
        "7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85");
    const auto pair10=digest(
        "857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db");
    const auto pair11=digest(
        "9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8");

    CHECK(runtime::hemenvlerp_receiver_observe_pipeline_digest(
        100u,pair9,20380u));

    const void *cmd=reinterpret_cast<const void*>(0x1000u);
    runtime::hemenvlerp_receiver_observe_bind(cmd,true,100u);

    runtime::hemenvlerp_receiver_identity identity{};
    CHECK(runtime::hemenvlerp_receiver_bound(cmd,identity));
    CHECK(identity.exact);
    CHECK(identity.pair_index==9u);
    CHECK(identity.semantic_receiver_id==33u);

    runtime::hemenvlerp_receiver_forget_pipeline(100u);
    CHECK(runtime::hemenvlerp_receiver_observe_pipeline_digest(
        100u,pair10,20080u));
    runtime::hemenvlerp_receiver_observe_bind(cmd,true,100u);
    CHECK(runtime::hemenvlerp_receiver_bound(cmd,identity));
    CHECK(identity.pair_index==10u);
    CHECK(identity.semantic_receiver_id==34u);

    runtime::hemenvlerp_receiver_forget_pipeline(100u);
    CHECK(runtime::hemenvlerp_receiver_observe_pipeline_digest(
        100u,pair11,18580u));
    runtime::hemenvlerp_receiver_observe_bind(cmd,true,100u);
    CHECK(runtime::hemenvlerp_receiver_bound(cmd,identity));
    CHECK(identity.pair_index==11u);
    CHECK(identity.semantic_receiver_id==35u);

    // Digest and size are a joint exact identity. Cross-pair size reuse must
    // not authorize a semantically adjacent receiver.
    runtime::hemenvlerp_receiver_forget_pipeline(100u);
    CHECK(!runtime::hemenvlerp_receiver_observe_pipeline_digest(
        100u,pair9,20080u));
    runtime::hemenvlerp_receiver_observe_bind(cmd,true,100u);
    CHECK(!runtime::hemenvlerp_receiver_bound(cmd,identity));

    // Reusing a live handle for another exact Lerp receiver is ambiguous until
    // the pipeline destroy boundary clears that handle.
    CHECK(runtime::hemenvlerp_receiver_observe_pipeline_digest(
        200u,pair9,20380u));
    CHECK(!runtime::hemenvlerp_receiver_observe_pipeline_digest(
        200u,pair10,20080u));
    runtime::hemenvlerp_receiver_observe_bind(cmd,true,200u);
    CHECK(!runtime::hemenvlerp_receiver_bound(cmd,identity));

    runtime::hemenvlerp_receiver_forget_pipeline(200u);
    CHECK(runtime::hemenvlerp_receiver_observe_pipeline_digest(
        200u,pair10,20080u));
    runtime::hemenvlerp_receiver_observe_bind(cmd,true,200u);
    CHECK(runtime::hemenvlerp_receiver_bound(cmd,identity));
    CHECK(identity.semantic_receiver_id==34u);

    // Non-pixel binds never discard a valid pixel-family identity.
    runtime::hemenvlerp_receiver_observe_bind(cmd,false,999u);
    CHECK(runtime::hemenvlerp_receiver_bound(cmd,identity));
    CHECK(identity.semantic_receiver_id==34u);

    const auto t=runtime::hemenvlerp_receiver_pipeline_stats();
    CHECK(t.exact_hits>=5u);
    CHECK(t.hash_misses>=1u);
    CHECK(t.handle_conflicts==1u);
    CHECK(t.exact_binds>=4u);
    CHECK(t.lookup_hits>=4u);
    CHECK(t.lookup_misses>=2u);

    runtime::hemenvlerp_receiver_pipeline_reset();
    return 0;
}
