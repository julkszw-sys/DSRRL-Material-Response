#!/usr/bin/env python3
"""Hard release veto for registered active material islands.
Inputs: actual game ReShade logs from hotspot AND control location.
No log-only claim can substitute PTDE pixel tests or full coverage.
"""
import argparse,re
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument("--hotspot",type=Path,required=True)
p.add_argument("--control",type=Path,required=True)
p.add_argument("--verified-pixel-and-coverage",action="store_true",
               help="Human/CI evidence must independently attest this; not a substitute for evidence")
a=p.parse_args()
s=a.hotspot.read_text(encoding="utf-8",errors="replace")
t=a.control.read_text(encoding="utf-8",errors="replace")
errors=[]
for label,log in (("hotspot",s),("control",t)):
    if "v351_typed_dsr_only_modified_renderer" not in log:
        errors.append(f"{label}: V351 build provenance missing")
    forbidden={
        "old V304 forced DSR-only fallback":r"stage=v304_typed_dsr_only_native_draw",
        "V296 stock material fallback":r"stage=non_atomic_spec_failopen|\[DSRRL V296 ROOTCAUSE\]",
        "shader materialization failure":r"create_materialize_fail",
        "EnvSpec probe reject":r"stage=probe_reject",
        "invalid typed LightBank data":r"stage=v351_typed_source_invalid_RELEASE_BLOCK",
        "resource transaction bind/restore failure":r"restore_fail=(?!0(?:\D|$))\d+|stage=(?:bind_fail|draw_failopen|tx_failopen|quarantine)",
    }
    for name,pattern in forbidden.items():
        matches=re.findall(pattern,log)
        if matches: errors.append(f"{label}: {name}, observations={len(matches)}")
if "stage=typed_source_into_modified_renderer" not in s:
    errors.append("hotspot: DSR-only row64 active modified renderer consumer not observed")
if not a.verified_pixel_and_coverage:
    errors.append("PTDE pixel/receiver/asset coverage not independently attested")
print("V351 RELEASE VETO", "BLOCKED" if errors else "PRECONDITIONS_PASS")
for e in errors: print(" - "+e)
if errors:raise SystemExit(1)
