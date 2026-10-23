#!/usr/bin/env python3
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_producer_state.cpp")
s=p.read_text("utf-8")
def sub(old,new):
    global s
    assert s.count(old)==1,(old,s.count(old))
    s=s.replace(old,new,1)
sub("#include <array>","#include <algorithm>\n#include <array>")
sub("bool same_material(\n", """bool certified_source_identity(
    const operators::material_response::material_identity &m) noexcept
{
    if (!m.valid || !m.owner_tuple_exact ||
        !m.material_slot_valid || !m.actual_material_exact ||
        m.semantic_name_hash == 0u) return false;
    const auto populated=[](const auto &a) noexcept {
        return std::any_of(a.begin(),a.end(),
            [](std::uint8_t b) noexcept {return b!=0u;});
    };
    return populated(m.flver_sha256) && populated(m.raw_mtd_sha256);
}

bool same_material(
""")
sub("        a.route_index == b.route_index &&",
"""        (a.route_index == b.route_index ||
         (certified_source_identity(a) && certified_source_identity(b))) &&""")
sub("    hash = fnv_scalar(hash, material.route_index);",
"""    hash = fnv_scalar(hash, certified_source_identity(material)
                            ? 0u : material.route_index);""")
p.write_text(s,"utf-8")
print("V230_CERTIFIED_SOURCE_IDENTITY_PASS")
