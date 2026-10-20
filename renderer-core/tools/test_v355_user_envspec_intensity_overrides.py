#!/usr/bin/env python3
"""V230: verify scoped 170 Ceaseless EnvSpec experiment changes ONLY one of 3 overridden rows.
No addon build. Original PTDE source and v300 provenance remain unmodified.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[2]
source=root/"runtime-v1/generated/v13_pmetal_donors.hpp"
generator=root/"renderer-core/tools/generate_pmetal_env_source_authority.py"
layout=root/"renderer-core/data/provenance/pmetal_env_source_layout_v1.tsv"
experiment=root/"renderer-core/data/provenance/user_envspec_intensity_overrides_v355.tsv"
original_provenance=root/"renderer-core/data/provenance/v300_lightbank_donors_ptde_dsr_only.tsv"
assert all(x.is_file() for x in (source,generator,layout,experiment,original_provenance))
regex=re.compile(r"donor_row\{(\d+)u,(\d+)u,(\d+)u,(\d+)u,(\d+)u\}")
original=[tuple(int(v) for v in m.groups()) for m in regex.finditer(source.read_text("utf-8"))]
assert len(original)==1246
with tempfile.TemporaryDirectory() as tmp:
    out=Path(tmp)/"generated_pmetal_env_source_authority.hpp"
    run=subprocess.run(
       [sys.executable,str(generator),"--input",str(source),
        "--layout-input",str(layout),"--output",str(out)],
       capture_output=True,text=True,check=True)
    print(run.stdout)
    generated=out.read_text("utf-8")
    effective=[tuple(int(v) for v in m.groups()) for m in regex.finditer(generated)]
    assert len(effective)==1246
    assert "User V355 test overrides applied" in generated
    changed=[(i,a,b) for i,(a,b) in enumerate(zip(original,effective)) if a!=b]
    assert len(changed)==3,changed
    # v13 bank slices, independently enumerated from immutable input source.
    banks=[tuple(int(v,16 if j==0 else 10) for j,v in enumerate(m.groups()))
        for m in re.finditer(r"\{(0x[0-9a-fA-F]+)ULL,(\d+)u,(\d+)u\}",source.read_text("utf-8"))]
    expected={(8,6):((255,235,155),170),(8,12):((255,255,200),130),(16,18):((255,255,255),130)}
    seen=set()
    for _i,a,b in changed:
        for (bank,row),(rgb,new_m) in expected.items():
            signature,first,count=banks[bank]
            if _i != first+row: continue
            assert row<count and a==(row,*rgb,100) and b==(row,*rgb,new_m),(bank,row,a,b)
            seen.add((bank,row))
    assert seen==set(expected),(seen,expected)
    assert "USER_TEST_OVERRIDE" in experiment.read_text("utf-8")
    lines=[s for s in experiment.read_text("utf-8").splitlines() if s and not s.startswith("#")]
    assert len(lines)==4,lines
    assert lines[1].split("\\t")[5:7]==["100","170"],lines[1]
    unchanged=1246-len(changed)
    print(f"V230_CEASELESS_ENVSPEC_DONOR_PASS changed=3 unchanged={unchanged}"
          " a14_m14_row6=170 a14_m14_row12=130 a17_m17_row18=130"
          " raw_PTDE_immutable=PASS addon_rebuild=NOT_PERFORMED")
