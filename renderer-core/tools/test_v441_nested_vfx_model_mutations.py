#!/usr/bin/env python3
"""Ten original semantics negative controls for nested VFX ModelDrawEntity provenance."""
import sys
from pathlib import Path
from verify_v440_native_channel import PE,verify
from verify_v441_nested_vfx_model_actor import check
blob=Path(sys.argv[1]).read_bytes();verify(blob);check(blob);p=PE(blob)
sites={
 "cluster_COL":0x136c0c8,"particle_COL":0x136c200,
 "cluster_ctor_offset":0x4fd3cf,"cluster_ctor_call":0x4fd3de,
 "cluster_dtor_offset":0x4fc09d,"cluster_dtor_call":0x4fc09e,
 "particle_ctor_offset":0x4fe7ef,"particle_ctor_call":0x4fe7fe,
 "particle_dtor_offset":0x4fdc7d,"particle_dtor_call":0x4fdc7e}
for label,site in sites.items():
 bad=bytearray(blob);bad[p.offset(site)]^=1
 try:check(bytes(bad),strict_sha=False)
 except AssertionError:continue
 raise AssertionError("unrejected VFX parent/inner mutation "+label)
print("V441_NESTED_VFX_NEGATIVES_PASS",len(sites))
