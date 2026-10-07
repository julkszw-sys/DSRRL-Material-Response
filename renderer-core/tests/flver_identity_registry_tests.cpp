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

    // Regression: a selector may run before its FLVER parser/support hook.
    // A cached negative verdict must become visible as a positive identity as
    // soon as the parser publishes the model, without waiting for a later
    // destruction/replacement epoch change.
    alignas(16) std::array<std::uint8_t,0x100> late_model{};
    const void *late_model_key=late_model.data();
    const void *late_container=late_model.data()+0x88u;
    std::array<std::uint8_t,32> late_sha{};
    assert(!dsrrl::runtime::flver_identity_lookup(late_container,late_sha));
    assert(dsrrl::runtime::flver_identity_observe_parse(
        late_model_key,raw.data(),raw.size()));
    assert(dsrrl::runtime::flver_identity_lookup(late_container,late_sha));
    assert(late_sha==first);

    // Destruction of another live FLVER must not change the authoritative
    // identity of this model. The registry implementation may keep the hot
    // positive verdict lock-free when the invalidation journal proves that
    // the mutation belongs to a different model.
    alignas(16) std::array<std::uint8_t,0x100> other_model{};
    const void *other_model_key=other_model.data();
    const void *other_container=other_model.data()+0x88u;
    std::array<std::uint8_t,32> other_sha{};
    assert(dsrrl::runtime::flver_identity_observe_parse(
        other_model_key,raw.data(),raw.size()));
    assert(dsrrl::runtime::flver_identity_lookup(other_container,other_sha));
    const auto cached_epoch_before_other_destroy =
        dsrrl::runtime::flver_identity_epoch();
    dsrrl::runtime::flver_identity_observe_destroy(other_model_key);
    std::uint64_t epoch_after_other_destroy = 0u;
    assert(dsrrl::runtime::flver_identity_cache_epoch_survives(
        container,
        cached_epoch_before_other_destroy,
        epoch_after_other_destroy));
    assert(epoch_after_other_destroy > cached_epoch_before_other_destroy);
    std::array<std::uint8_t,32> after_other_destroy{};
    assert(dsrrl::runtime::flver_identity_lookup(
        container,after_other_destroy));
    assert(after_other_destroy==first);

    const auto cached_epoch_before_self_destroy =
        epoch_after_other_destroy;
    dsrrl::runtime::flver_identity_observe_destroy(model_key);
    std::uint64_t epoch_after_self_destroy = 0u;
    assert(!dsrrl::runtime::flver_identity_cache_epoch_survives(
        container,
        cached_epoch_before_self_destroy,
        epoch_after_self_destroy));
    std::array<std::uint8_t,32> stale{};
    assert(!dsrrl::runtime::flver_identity_lookup(container,stale));
    assert(!dsrrl::runtime::flver_identity_enrich_owner(container, 7u, owner));
    assert(!owner.material_slot_valid);

    dsrrl::runtime::flver_identity_observe_destroy(late_model_key);

    raw[0]='X';
    assert(!dsrrl::runtime::flver_identity_observe_parse(model_key,raw.data(),raw.size()));

    std::memcpy(raw.data(),magic,6);
    const std::uint32_t oversized_length=16u*1024u*1024u;
    std::memcpy(raw.data()+0x10,&oversized_length,4);
    assert(!dsrrl::runtime::flver_identity_observe_parse(
        model_key,raw.data(),static_cast<std::size_t>(0x40u)+oversized_length));

    const auto t=dsrrl::runtime::flver_identity_stats();
    assert(t.inserts>=3u);
    assert(t.hits>=5u);
    assert(t.misses>=2u);
    assert(t.erases>=3u);
    assert(t.invalid_raw>=1u);
    return 0;
}