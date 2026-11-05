#!/usr/bin/env python3
"""V441 chained original static semantic mutation-negative suite: no disk writes."""
import sys
from pathlib import Path
from verify_v440_native_channel import PE,verify
from verify_v441_entity_lifecycle import check as lifecycle
from verify_v441_three_channel_transport import check as channels
from verify_v441_two_actor_source_census_r2 import check as census
blob=Path(sys.argv[1]).read_bytes();verify(blob);p=PE(blob)
TESTS={
 "lifecycle":[0x215992,0x2159a0,0x215a29,0x215ded,0x21aa49,
   0x21f934,0x21f942,0x21f9c5,0x21fc00,0x2237f9],
 "three_channels":[0x2167cc,0x2167de,0x220647,0x220655,
   0x1c0c40,0x1c0c60,0x1c0c4b,0x1c0c6b,
   0x1c86c8,0x1caccc,0x1c86a4,0x1caca7,
   0x1cad1f,0x1cad27,0x1cad8f,0x1cae76,0x1cad9f,0x1caeee],
 "direct_owner_census":[0x2167ba,0x220639,0x220632,
  0x1c69fe,0x1c6f4a,0x219d79,0x222d49,0x2d39ab,
  0x55d046,0x221412,0x12ebfe8+0x50,0x12eca10+0x50,
  0x216232,0x220054]}
CHECKS={"lifecycle":lifecycle,"three_channels":channels,
 "direct_owner_census":census}
for name,sites in TESTS.items():
 assert CHECKS[name](blob,strict_sha=True)
 for site in sites:
  tampered=bytearray(blob);tampered[p.offset(site)]^=1
  try:CHECKS[name](bytes(tampered),strict_sha=False)
  except AssertionError:continue
  raise AssertionError(f"unrejected {name} mutation {site:#x}")
 print(name,"SEMANTIC_NEGATIVES_PASS",len(sites))
print("V441_TOTAL_SEMANTIC_NEGATIVES_PASS",sum(map(len,TESTS.values())))
