#!/usr/bin/env python3
"""Expose exact bound-D3D11-view -> GI probe ordinal lookup for map source join."""
from pathlib import Path
h=Path("renderer-core/include/dsrrl/runtime/envspec_resource_runtime.hpp")
c=Path("renderer-core/src/runtime/envspec_resource_runtime.cpp")
hs=h.read_text();cs=c.read_text()
a="""    bool prepare_bound(
        ID3D11ShaderResourceView *stock_a,"""
assert hs.count(a)==1
hs=hs.replace(a,"""    // Read-only native view identity; no cubemap replacement or capture.
    bool identify_bound_probe(
        ID3D11ShaderResourceView *stock_a,
        ID3D11ShaderResourceView *stock_b,
        bool require_b,
        std::uint16_t &probe_a,
        std::uint16_t &probe_b) const noexcept;

"""+a,1)
a="bool envspec_resource_runtime::prepare_bound("
assert cs.count(a)==1
cs=cs.replace(a,"""bool envspec_resource_runtime::identify_bound_probe(
    ID3D11ShaderResourceView *a,
    ID3D11ShaderResourceView *b,
    bool require_b,
    std::uint16_t &probe_a,
    std::uint16_t &probe_b) const noexcept
{
    probe_a=0u;probe_b=0u;
    if (!probe_for_native_view(a,probe_a))return false;
    if (require_b) {
        if (!probe_for_native_view(b,probe_b))return false;
    } else probe_b=probe_a;
    return true;
}

"""+a,1)
h.write_text(hs);c.write_text(cs)
print("PASS exact bound GI probe identity interface")
