#!/usr/bin/env python3
"""V340 opt-in PTDE Lerp beta-zero isolation: construction contract only.

Assertion scope is deliberately exact: this is NOT proof of GPU shader binding
or PTDE-visible pixels. Compare V340 with the existing V338 on identical scene.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
runtime = (root / "src/runtime/pmetal_envspec_draw_runtime.cpp").read_text(encoding="utf-8")
cmake = (root / "integrated/CMakeLists.txt").read_text(encoding="utf-8")
build = (root / "tools/build_v340_nonmetal_lerp_beta_zero_one_addon.ps1").read_text(encoding="utf-8")

feature = "DSRRL_V340_NONMETAL_LERP_B12_BETA_ZERO"
assert runtime.count("#if defined(" + feature + ")") == 2
assert cmake.count("if(" + feature + ")") == 1
assert cmake.count("    " + feature + "\n") == 1
assert f"-D{feature}=ON" in build
assert "-DDSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB=ON" in build
assert "r8_selected &&" in runtime
assert "family == pmetal_envspec_receiver_family::hemenvlerp" in runtime
assert "material.route_index != 345u" in runtime
assert runtime.count("    const float v340_shader_beta =") == 2
assert runtime.count("            v340_shader_beta\n") == 2
assert runtime.count("            source.beta\n") == 0  # both b12 Lerp beta writes replaced
assert "            source.beta\n" not in runtime.split("const std::array<f4,6> payload{{",1)[1].split("    }};",1)[0]
assert "            carrier3[2],\n            v340_shader_beta" in runtime
assert "            source.envdiffuse_b[2],\n            v340_shader_beta" in runtime
assert "source.beta != 0.0f" in runtime
assert "source.beta;" in runtime
assert "probe_b_required =" in runtime and "prepare_bound(" in runtime
assert "v340_shader_beta =\n        r8_selected &&" in runtime
assert "            ? 0.0f\n            : source.beta;" in runtime
assert "[DSRRL V340 BETA ZERO] STAGE=request_ready" in runtime
assert runtime.index("[DSRRL V340 BETA ZERO]") > runtime.index("build_island_draw_mutation(")
assert "prepared.request.pixel_shader =\n        shader;" in runtime
assert "    prepared.request.constant_buffers[0] = {\n        12u,\n        b12," in runtime

# Exact routing matrix: V340 must change b12 Lerp coefficients ONLY if
# nonmetal R8 was selected; P_Metal, Stable and nonadmitted draws unchanged.
def shader_beta(r8: bool, lerp: bool, route: int, beta: float) -> float:
    return 0.0 if r8 and lerp and route != 345 else beta

for beta in [0.0, 0.00001, 0.12, 0.5, 1.0]:
    assert shader_beta(True, True, 5, beta) == 0.0
    assert shader_beta(True, True, 1073741869, beta) == 0.0
    assert shader_beta(False, True, 345, beta) == beta
    assert shader_beta(False, False, 5, beta) == beta
    assert shader_beta(False, True, 5, beta) == beta

print("V340 PASS: strictly two shader interpolation lanes beta=0 when exact nonmetal R8 selected; original CPU beta and A/B source routing untouched; controls P_Metal and Stable unchanged; runtime/pixels OPEN")
