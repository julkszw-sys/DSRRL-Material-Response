from pathlib import Path
root=Path(__file__).resolve().parents[1]
h=(root/"include/dsrrl/runtime/v381_rx35_material_census.hpp").read_text()
s=(root/"integrated/integrated_addon.cpp").read_text()
assert '[DSRRL V381 RX35 ALL MATERIAL CUT]' in h
assert "d.receiver_id!=35u" in h
assert "m.owner_tuple_exact" in h
assert "m.material_slot_valid" in h
assert "m.route_index==345u" not in h
assert 'v381_rx35_material_census.hpp' in s
assert s.count('v381_trace_rx35(2u,"prepared"') == 2
assert s.count('v381_trace_rx35(1u,"prepare_reject"') == 2
assert s.count('v381_trace_rx35(3u,"native_armed"') == 2
assert s.count('v381_issued?"issued":"failopen"') == 2
assert s.count('v381_trace_rx35(v381_issued?4u:5u') == 2
assert 'p->envspec.envdiffuse_resources.ready' in s
assert 'p->resources.spec_rgb' in s and 'p->resources.diffuse' in s
assert 'draw_tx_issued(dispatch.transaction)' in s
assert 'source_.latest(' not in h
assert 'PSSet' not in h and 'CreatePixelShader' not in h
print("V381_RX35_ALL_MATERIAL_DISPATCH_CENSUS_PASS pixel=OPEN")
