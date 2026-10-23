#!/usr/bin/env python3
"""Activate bounded exact source1 material provenance on pinned CI build only."""
from pathlib import Path
p = Path(__file__).with_name("build_v230_production.ps1")
text = p.read_text(encoding="utf-8")
old_flag = "/DDSRRL_V230_SOURCE1_REPAIR "
new_flag = "/DDSRRL_V230_SOURCE1_REPAIR /DDSRRL_V230_SOURCE1_EXACT_PROVENANCE "
assert text.count(old_flag) == 1, "Source1 build flag drift"
text = text.replace(old_flag, new_flag, 1)
anchor = "python renderer-core/tools/materialize_v230_source1_repair.py"
assert text.count(anchor) == 1, "Source1 repair materializer drift"
next_step = "python renderer-core/tools/test_v230_release_contract.py"
assert text.count(next_step) == 1, "Source1 release contract anchor drift"
text = text.replace(next_step,
    "python renderer-core/tools/materialize_v230_source1_exact_hit_provenance.py\n"
    "if ($LASTEXITCODE -ne 0) { throw 'source1 exact-hit provenance materializer failed' }\n"
    + next_step, 1)
assert text.index(anchor) < text.index("materialize_v230_source1_exact_hit_provenance.py") < text.index(next_step)
p.write_text(text, encoding="utf-8")
print("V230_SOURCE1_PROVENANCE_BUILD_ROUTING_PASS")
