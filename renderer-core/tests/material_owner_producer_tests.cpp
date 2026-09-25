#include "dsrrl/runtime/material_owner_producer.hpp"
#include <iostream>
using namespace dsrrl;

#define CHECK(x) do { if(!(x)){ std::cerr << "CHECK failed: " #x "\n"; return 1; } } while(false)

int main()
{
    runtime::actual_material_owner_observation o{};
    o.material.valid = true;
    o.material.semantic_name_hash = 0x1234u;
    o.material_slot = 7u;
    o.material_slot_valid = true;
    o.flver_identity_hash = 0x55u;

    auto id = runtime::make_actual_material_identity(o);
    CHECK(!id.owner_tuple_exact);

    o.flver_sha256[0] = 0x42u;
    id = runtime::make_actual_material_identity(o);
    CHECK(id.owner_tuple_exact);
    CHECK(id.material_slot_valid);
    CHECK(id.material_slot == 7u);
    CHECK(id.flver_identity_hash == 0x55u);
    CHECK(id.flver_sha256[0] == 0x42u);

    o.material.semantic_name_hash = 0u;
    id = runtime::make_actual_material_identity(o);
    CHECK(!id.owner_tuple_exact);
    return 0;
}
