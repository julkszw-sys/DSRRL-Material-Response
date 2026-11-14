#!/usr/bin/env python3
"""V469: prune *only* V230 nonmetal FIRST-CUT diagnostics from RX34-focused addon.

The stage latch is 512 routes * 24 receiver buckets * 16 material slots
= 196608 global atomic<uint32_t> = 786432 logical bytes. It is telemetry
only (no admission), and explicitly excludes both 345 RX34 and 229 routes.
This is a diagnostic memory/file-size and hot-atomic-cost isolation,
NOT a change to production Material Response or any PTDE shader.
"""
from pathlib import Path

p=Path("renderer-core/integrated/integrated_addon.cpp")
s=p.read_text(encoding="utf-8")

table='''std::array<std::atomic<std::uint32_t>, 512u * 24u * 16u>
    g_v230_nonmetal_route_logged{};'''
assert s.count(table)==1, ("exact table identity drift",s.count(table))

a=s.index("void v230_nonmetal_route_trace(")
b=s.index("\n#endif\n",a)
body=s[a:b]
required=(
    "material.route_index == 345u",
    "material.route_index == 229u",
    "g_v230_nonmetal_route_logged[key].fetch_or(",
    "reshade::log::message(reshade::log::level::warning, msg);",
)
for marker in required:
    assert marker in body, ("not certified diagnostic-only section",marker)
assert "void v230_nonmetal_route_trace(" in body and len(body)<4000
# Same exact signature; compiler can fold calls to a no-op while preserving
# all render-side material/preparation and other instrumentation.
replacement='''inline void v230_nonmetal_route_trace(
    std::uint32_t stage,
    std::uint32_t receiver,
    const dsrrl::operators::material_response::material_identity &material,
    const dsrrl::operators::material_response::decision &decision) noexcept
{
    // V469 targeted RX34 diagnostic: irrelevant nonmetal first-hit telemetry
    // disabled; render state and all upstream validation remain unchanged.
    (void)stage; (void)receiver; (void)material; (void)decision;
}'''
s=s[:a]+replacement+s[b:]
assert s.count(table)==1
s=s.replace(table,
"""// V469 RX34 scope: removed 196608 atomics from nonmetal-only
// first-hit telemetry. They were never consumed by rendering.""",1)
assert "g_v230_nonmetal_route_logged" not in s
assert s.count("inline void v230_nonmetal_route_trace(")==1
p.write_text(s,encoding="utf-8")
print("V469 source PASS: eliminated diagnostic-only 196608 atomic slot table; route 345/229 rendering untouched")
