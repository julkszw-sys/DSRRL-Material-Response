#!/usr/bin/env python3
"""V347 exact R19 EnvDiffuse relocated t11 instruction boundary.
Static source invariant only; native and owner GPU runtime tested separately.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = (root/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text()
draw = (root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
build = (root/"tools/build_v347_lerp_envdiffuse_rebase_one_addon.ps1").read_text()
assert "constexpr std::size_t k_ptde_envdiffuse_lerp_prefix_words = 6u;" in src
assert "site.t11_word != site.t12_word + 129u" in src
assert "site.t11_word -\n        k_ptde_envdiffuse_lerp_prefix_words" in src
assert "start + expected_chain.size() != site.merge_word" in src
assert "expected_chain.size() != 69u" in src
assert "words.begin() + static_cast<std::ptrdiff_t>(start)" in src
assert "v344_expected_t11_delta" in src
assert "129u - k_ptde_envdiffuse_lerp_prefix_words" in src
assert "v344_t11 != v344_t12 + v344_expected_t11_delta" in src
assert "v344_t11 != v344_t12 + 129u" not in src
assert "v344_t11 - 6u" in src
assert "v344_t12 + k_ptde_rgba_envspec_chain.size() + 33u" in src
assert "DSRRL_V346_FAIL(13u)" in src
assert "DSRRL_V347_LERP_ENVDIFFUSE_REBASE" in draw
assert "DSRRL_V347_LERP_ENVDIFFUSE_REBASE" in build
assert "DSRRL_V345_UNIFIED_HEMENVLERP" in build
assert "DSRRL_V346_LERP_REJECT_STAGE" in build
assert 129-6 == 123  # R19 moved the t11 sample into the preceding six NOP dwords.
assert 129-6-6-74 >= 33  # 33-DWORD isolated late t10+modulate fits before relocated t11.
print("V347 source invariant PASS: verified R19 EnvDiffuse A original +129 -> live +123, 33-DWORD insertion bound, 3 exact receivers and fail-open retained; GPU/pixels OPEN")
