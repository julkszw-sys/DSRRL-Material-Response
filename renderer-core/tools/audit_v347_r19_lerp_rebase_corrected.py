#!/usr/bin/env python3
from pathlib import Path
p=Path(__file__).resolve().parents[1]
s=(p/"src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text()
b=(p/"tools/build_v347_lerp_envdiffuse_rebase_one_addon.ps1").read_text()
assert "k_ptde_envdiffuse_lerp_prefix_words = 6u" in s
assert "site->t11_word != site->t12_word + 129u" in s
assert "site.t11_word) -" in s
assert "start + expected_chain.size() != site.merge_word" in s
assert "v344_t11 != v344_t12 + v344_expected_t11_delta" in s
assert "129u - k_ptde_envdiffuse_lerp_prefix_words" in s
assert "v344_t11 - 6u" in s
assert "DSRRL_V346_FAIL(13u)" in s
assert "DSRRL_V347_LERP_ENVDIFFUSE_REBASE" in b
assert 129-6==123 and 129-6-6-74>=33
print("V347 exact source topology PASS; GPU and PTDE pixels OPEN")
