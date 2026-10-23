#include "dsrrl/runtime/flver_identity_registry.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <future>
#include <thread>
#include <vector>

using dsrrl::runtime::flver_identity_lookup_direct_parser_model;
using dsrrl::runtime::flver_identity_observe_parse;
using dsrrl::runtime::flver_identity_observe_destroy;
using dsrrl::runtime::flver_identity_reset;
using Sha = std::array<std::uint8_t, 32>;

std::vector<std::uint8_t> model_bytes(std::uint8_t seed)
{
    std::vector<std::uint8_t> b(0x140u);
    const std::uint8_t magic[]{'F','L','V','E','R',0};
    std::memcpy(b.data(),magic,sizeof(magic));
    const std::uint32_t offset=0x40u,length=0x100u;
    std::memcpy(b.data()+0x0cu,&offset,4u);
    std::memcpy(b.data()+0x10u,&length,4u);
    for(std::size_t i=0x40u;i<b.size();++i)
        b[i]=static_cast<std::uint8_t>((i*17u+seed)&0xffu);
    return b;
}
int main()
{
    flver_identity_reset();
    int a=1,b=2;
    Sha one{},two{},now{};
    assert(!flver_identity_lookup_direct_parser_model(&a,now));
    auto pa=model_bytes(5u),pb=model_bytes(17u),changed=model_bytes(77u);
    assert(flver_identity_observe_parse(&a,pa.data(),pa.size()));
    assert(flver_identity_observe_parse(&b,pb.data(),pb.size()));
    assert(flver_identity_lookup_direct_parser_model(&a,one));
    assert(flver_identity_lookup_direct_parser_model(&b,two));
    assert(one!=two);
    for(int i=0;i<10000;++i) {
        assert(flver_identity_lookup_direct_parser_model(&a,now));
        assert(now==one);
        assert(flver_identity_lookup_direct_parser_model(&b,now));
        assert(now==two);
    }
    // Unrelated model replacement must preserve registered A's exact SHA.
    assert(flver_identity_observe_parse(&b,changed.data(),changed.size()));
    assert(flver_identity_lookup_direct_parser_model(&a,now) && now==one);
    assert(flver_identity_lookup_direct_parser_model(&b,now) && now!=two);
    // Same-address identity mutation and destruction invalidate TLS.
    assert(flver_identity_observe_parse(&a,changed.data(),changed.size()));
    assert(flver_identity_lookup_direct_parser_model(&a,now) && now!=one);
    flver_identity_observe_destroy(&a);
    assert(!flver_identity_lookup_direct_parser_model(&a,now));
    assert(flver_identity_observe_parse(&a,pa.data(),pa.size()));
    assert(flver_identity_lookup_direct_parser_model(&a,now) && now==one);
    // Invalidation must cross threads, including an already-hot TLS slot.
    std::promise<void> warmed,continue_worker;
    auto ready=warmed.get_future();
    auto resume=continue_worker.get_future();
    std::thread worker([&] {
        Sha from_worker{};
        assert(flver_identity_lookup_direct_parser_model(&a,from_worker) &&
               from_worker==one);
        warmed.set_value();
        resume.wait();
        assert(!flver_identity_lookup_direct_parser_model(&a,from_worker));
    });
    ready.wait();
    flver_identity_observe_destroy(&a);
    continue_worker.set_value();
    worker.join();
    assert(!flver_identity_lookup_direct_parser_model(&a,now));
    // Global reset must invalidate valid positives across any model.
    assert(flver_identity_observe_parse(&a,pa.data(),pa.size()));
    assert(flver_identity_lookup_direct_parser_model(&a,now));
    flver_identity_reset();
    assert(!flver_identity_lookup_direct_parser_model(&a,now));
    // Unknown candidate pointer is never promoted by MTD or by alias cache.
    int unregistered=3;
    assert(!flver_identity_lookup_direct_parser_model(&unregistered,now));
}
