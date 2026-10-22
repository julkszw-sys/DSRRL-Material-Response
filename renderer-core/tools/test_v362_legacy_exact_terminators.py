#!/usr/bin/env python3
"""Regression for V360 impossible identity comparisons (C++ multi-char literal)."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'include/dsrrl/operators/env_spec/legacy_envdiffuse_verified_identity_v360.hpp'
s=p.read_text(encoding='utf-8')
assert s.count(r"'\0'")==2, "C++ NUL terminators (one actual slash) must occur twice"
assert r"'\\0'" not in s, "Multi-character backslash-zero literal silently makes all SHA/name lookups false"
assert "expected[n]!='\\0'" in s
assert "hex[64u]=='\\0'" in s
assert "exact_name(name,count,entry.name) &&" in s
assert "exact_digest(digest,entry.sha256_hex)" in s
print("V362_LEGACY_NUL_TERMINATOR_SOURCE_PASS")
