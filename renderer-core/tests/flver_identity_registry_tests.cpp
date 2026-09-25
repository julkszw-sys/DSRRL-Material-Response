#include "dsrrl/runtime/flver_identity_registry.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

int main()
{
    std::array<std::uint8_t,0x80> raw{};
    const char magic[6]={'F','L','V','E','R','\0'};
    std::memcpy(raw.data(),magic,6);
    const std::uint32_t data_offset=0x40u;
    const std::uint32_t data_length=0x40u;
    std::memcpy(raw.data()+0x0c,&data_offset,4);
    std::memcpy(raw.data()+0x10,&data_length,4);
    for(std::size_t i=0x18;i<raw.size();++i)raw[i]=static_cast<std::uint8_t>(i);

    alignas(16) std::array<std::uint8_t,0x100> model{};
    const void *model_key=model.data();
    const void *container=model.data()+0x88u;

    assert(dsrrl::runtime::flver_identity_observe_parse(model_key,raw.data(),raw.size()));

    std::array<std::uint8_t,32> first{},second{};
    assert(dsrrl::runtime::flver_identity_lookup(container,first));
    assert(dsrrl::runtime::flver_identity_lookup(container,second));
    assert(first==second);

    dsrrl::runtime::flver_identity_observe_destroy(model_key);
    std::array<std::uint8_t,32> stale{};
    assert(!dsrrl::runtime::flver_identity_lookup(container,stale));

    raw[0]='X';
    assert(!dsrrl::runtime::flver_identity_observe_parse(model_key,raw.data(),raw.size()));

    const auto t=dsrrl::runtime::flver_identity_stats();
    assert(t.inserts>=1u);
    assert(t.hits>=2u);
    assert(t.misses>=1u);
    assert(t.erases>=1u);
    assert(t.invalid_raw>=1u);
    return 0;
}
