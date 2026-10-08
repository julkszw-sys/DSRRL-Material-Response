#!/usr/bin/env python3
from pathlib import Path
import re
s=Path("renderer-core/src/runtime/pmetal_env_source_runtime.cpp").read_text()
h=Path("renderer-core/include/dsrrl/runtime/pmetal_dsr_only_lightbank_fallback.hpp").read_text()
c=Path("renderer-core/integrated/CMakeLists.txt").read_text()
assert "DSRRL_PMETAL_EXACT_DSR_ROW64_HYBRID" in s and "DSRRL_PMETAL_EXACT_DSR_ROW64_HYBRID" in c
assert h.count("64u,{255u") == 3
assert "if (row_id != 64u)" in h
assert "bank->live_count == 65u && bank->count == 64u && count == 65u" in s
assert "host->dsr_envspec_rgbm" in s and "host->dsr_envdiffuse_rgbm" in s
assert "pmetal_env_source_authority::find_row(*bank,row_id)" in s
assert "DSRRL PMETAL ROW64 HYBRID" in s
print("PMETAL_EXACT_DSR_ROW64_HYBRID_AUDIT_PASS")
