#!/usr/bin/env python3
"""V376: nonmetal dispatch-fork observability, without gate mutation."""
from pathlib import Path
src=(Path(__file__).resolve().parents[1] / "integrated/integrated_addon.cpp").read_text("utf-8")
start=src.index("void v376_nonmetal_fork_witness(")
end=src.index("void v230_nonmetal_route_trace(",start)
cut=src[start:end]
for token in ("case 232u:", "case 160u:", "case 88u:",
              "case 12u:", "case 41u:", "rx < 33u || rx > 34u",
              "std::array<std::atomic_bool,10u>",
              "[DSRRL V376 NONMETAL FORK]", "fixed=%u clustered=%u",
              "sss=%u hemdir3=%u ul=%u"):
    assert token in cut, token
for forbidden in ("PSSet", "PSGet", "DrawIndexed(", "CreatePixelShader",
                  "append_island_draw_request", "prepare_draw_request"):
    assert forbidden not in cut, forbidden
prepare=src[src.index("bool prepare_island_batch("):src.index("void observe_bloom_fx_draw_authority()")]
assert prepare.count("v376_nonmetal_fork_witness(")==1
assert prepare.index("v376_nonmetal_fork_witness(")<prepare.index("if (clustered_pointlight_bound)")
print("V376_NONMETAL_PASSIVE_FORK_WITNESS_SOURCE_PASS pixel=OPEN")
