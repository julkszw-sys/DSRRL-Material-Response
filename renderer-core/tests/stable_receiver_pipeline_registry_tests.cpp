#include "dsrrl/runtime/stable_receiver_pipeline_registry.hpp"
#include "dsrrl/runtime/generated_stable_hemenv_receivers_v1.hpp"

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
    runtime::stable_receiver_pipeline_reset();

    const auto rx33=digest(
        "35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24");
    const auto rx34=digest(
        "d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837");

    CHECK(runtime::generated::stable_hemenv_candidate_size(19856u));
    CHECK(!runtime::generated::stable_hemenv_candidate_size(19857u));
    CHECK(runtime::generated::stable_hemenv_receiver_id(rx33,19856u)==33u);
    CHECK(runtime::generated::stable_hemenv_receiver_id(rx34,19556u)==34u);
    CHECK(runtime::generated::stable_hemenv_receiver_id(rx33,19556u)==0u);

    CHECK(runtime::stable_receiver_observe_pipeline_digest(100u,rx33,19856u));

    const void *cmd=reinterpret_cast<const void*>(0x1000u);
    runtime::stable_receiver_observe_bind(cmd,true,100u);
    std::uint32_t receiver=0u;
    CHECK(runtime::stable_receiver_bound(cmd,receiver));
    CHECK(receiver==33u);

    // Unknown pixel bind must clear the previous exact receiver.
    runtime::stable_receiver_observe_bind(cmd,true,999u);
    CHECK(!runtime::stable_receiver_bound(cmd,receiver));
    CHECK(receiver==0u);

    // Reusing one still-live pipeline handle for a different exact receiver is
    // ambiguous and quarantines that handle until its destroy boundary.
    CHECK(runtime::stable_receiver_observe_pipeline_digest(200u,rx33,19856u));
    CHECK(!runtime::stable_receiver_observe_pipeline_digest(200u,rx34,19556u));
    runtime::stable_receiver_observe_bind(cmd,true,200u);
    CHECK(!runtime::stable_receiver_bound(cmd,receiver));

    runtime::stable_receiver_forget_pipeline(200u);
    CHECK(runtime::stable_receiver_observe_pipeline_digest(200u,rx34,19556u));
    runtime::stable_receiver_observe_bind(cmd,true,200u);
    CHECK(runtime::stable_receiver_bound(cmd,receiver));
    CHECK(receiver==34u);

    // Non-pixel binds do not disturb the currently bound pixel receiver.
    runtime::stable_receiver_observe_bind(cmd,false,999u);
    CHECK(runtime::stable_receiver_bound(cmd,receiver));
    CHECK(receiver==34u);

    const auto t=runtime::stable_receiver_pipeline_stats();
    CHECK(t.exact_receiver_hits>=3u);
    CHECK(t.handle_conflicts==1u);
    CHECK(t.lookup_hits>=3u);
    CHECK(t.lookup_misses>=2u);

    runtime::stable_receiver_pipeline_reset();
    return 0;
}
