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

    dsrrl::runtime::actual_material_owner_observation owner{};
    owner.material.semantic_name_hash = 0x1234u;
    assert(dsrrl::runtime::flver_identity_enrich_owner(container, 7u, owner));
    assert(owner.flver_sha256 == first);
    assert(owner.material_slot == 7u);
    assert(owner.material_slot_valid);

    dsrrl::runtime::flver_identity_observe_destroy(model_key);
    std::array<std::uint8_t,32> stale{};
    assert(!dsrrl::runtime::flver_identity_lookup(container,stale));
    assert(!dsrrl::runtime::flver_identity_enrich_owner(container, 7u, owner));
    assert(!owner.material_slot_valid);

    raw[0]='X';
    assert(!dsrrl::runtime::flver_identity_observe_parse(model_key,raw.data(),raw.size()));

    std::memcpy(raw.data(),magic,6);
    const std::uint32_t oversized_length=16u*1024u*1024u;
    std::memcpy(raw.data()+0x10,&oversized_length,4);
    assert(!dsrrl::runtime::flver_identity_observe_parse(
        model_key,raw.data(),static_cast<std::size_t>(0x40u)+oversized_length));

    const auto t=dsrrl::runtime::flver_identity_stats();
    assert(t.inserts>=1u);
    assert(t.hits>=2u);
    assert(t.misses>=1u);
    assert(t.erases>=1u);
    assert(t.invalid_raw>=1u);
    return 0;
}
