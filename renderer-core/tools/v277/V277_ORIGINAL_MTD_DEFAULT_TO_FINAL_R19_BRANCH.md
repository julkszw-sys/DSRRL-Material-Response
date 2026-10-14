# V277 — exact MaterialWorkflow gate of the *actual final* R19

**Original owner inputs:** pinned `DSR/mtd/Mtd.mtdbnd.dcx` SHA256 `7c5a0952c1016a3efa3c6a7180bc60af81ccf7b2a4685c3c89028eda78c2d8b5`, pinned `DSR/mtd/MtdPatch.mtdbnd.dcx` SHA256 `45c1227cb8b8cbb6a55ee2b21c188e9ccff68c59c3764ee5d070755576bf8864`, and the actual final materialized R19 RX33–RX35 exact digests from V276, **not stock-only DXBC**. The origin archive was supplied by the owner; do not commit binder data.

## Key result

All three final R19 shaders have byte-exact `IF_Z cb0[85].x` token sequence `0400001f 0020800a 00000000 00000055`. The second DSR-only `SpecRGB×cb0[10] → gamma2.2` EXP sits **inside the ELSE**, and is reachable only when the actual pixel constant `gFC_MaterialWorkflow.x != 0`. In the zero branch (THEN), this second gamma operator does **not** execute.

| Actual final R19 | IF_Z | ELSE | ENDIF | second EXP |
|---|---:|---:|---:|---:|
| RX33 | 1108 | 1319 | 1421 | 1372 |
| RX34 | 1017 | 1228 | 1330 | 1281 |
| RX35 | 677 | 888 | 990 | 941 |

The original DSR MTD binder contains **17** relevant P_Metal / P_Leather / P_DullLeather records: **9** have authored workflow default `0` (including ordinary P_Metal/Leather/DullLeather), and **8** are workflow `1` (their `_Spec` variants). This is an actual original serialized MTD `int` property, not a filename heuristic. Three nonzero variants include P_Metal`_Spec`; never generalize by broad material class.

The *plain* `P_Metal[DSB].mtd` (original SHA `ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b`) does not have `g_MaterialWorkflow`; DSR original EXE defaults the absent property to zero. Plain `P_Leather[DSB].mtd` is SHA `0f2b9a1012b83c4dfc909c311d29a11147de8c15191bc62398ce0d5e1cc6757e` and explicitly zero. `P_Leather[DSB]_Spec.mtd` SHA `31c245e4cfc0857fdbffac1c2d4f81a97abac230369efec432553d42649656eb` explicitly one.

**Important binding ambiguity:** retail binder contains TWO distinct byte-level `P_DullLeather[DSB].mtd` entries with SHA `7ea7d6ce9bb7148db62e8c27ee45edefb9547018d0b723dd4753d4c37e29f293` and SHA `9376d84c81f0b1e77dc728dc469ce9fa281c41d6a7ccd23ec415471aa28341a3`. Both encode authored default zero, but the logical filename alone does not select the physical MTD source identity. No target family overrides are present in supplied MtdPatch binder.

## Falsification and target scope

Owner-local original MTD binder SHA checks, 17/17 item enumeration, final actual RX33/34/35 shader branch verification **PASS**. Five induced mutation tests (3 modified RX blobs and 2 modified source binders) are rejected.

This corrects an overbroad inference from V276: **the byte-preserved second material gamma has potential pre-Fog liveout but is NOT proven to run for the user's plain nonmetal equipment.** Specifically, a default-zero ordinary MTD excludes it *unless runtime or other engine state overrides cb0[85]*. Therefore it cannot be deployed as a speculative global correction for the owner-observed overgloss on plain P_Leather/P_DullLeather.

**OPEN:** true draw PS cb0[85] & DebugDraw cb0[102], exact FLVER material slot and sidecar/MTD winner, PTDE operator replacement and pixels. The current output is source qualification; there is no active PTDE pixel patch in V277.

## Reproduction

- `python renderer-core/tools/v277/owner_original_mtd_workflow_matrix.py <owner-part02.zip> /tmp/mtd_default_matrix.json`
- `python renderer-core/tools/v277/actual_final_r19_workflow_branch.py <final_RX33.dxbc> <final_RX34.dxbc> <final_RX35.dxbc> /tmp/final_branch.json`

Keep original game bytes local. Preserve stock DSR Fog/LS/Glow, unrelated PBL, P_Metal as control, and exact material/slot fail-open. Do not issue another dormant addon as a cosmetic progress milestone.
