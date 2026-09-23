# Material Response 1.45 — source recovery chain

Status: **PARTIAL SOURCE RECOVERY / CONSTRUCTION**  
Date: 2026-09-23

This branch starts from the exact Nexus 1.45 review branch and preserves the original development source chain recovered from the DSRRL Library/Supabase provenance.

## Exact shipping release identity

- file: `DSRRL_Material_Response_1.45.addon64`
- size: `1,803,264` bytes
- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

The user-supplied 2026-09-23 ZIP was independently unpacked during recovery and its addon payload matches this SHA-256 exactly.

## Original source recovered

The recovered source snapshot contains:

- V12 integrated Normal / Diffuse / SpecRGB / Subsurf bridge source: `asset_integrated_v12.c`
- V12 assembler stubs and deterministic builder/audit
- original V15.7 deterministic EnvSpec activation builder and route map
- original final client 1.45 builders
- handoff and static audit evidence

Key provenance matches:

- `asset_integrated_v12.c` → `879b3ec251c36969cf7eabcc5b63164da7c17b6577e74b013e47f7866b3236e2`
- `build_v15_7.py` → `99f9108c2eea2b90632506d46d47ec4180ac657269970e6193f8a2a2305e151e`
- `build_client145.py` → `e0fec5dca605f94ea0c532cff5e555d7fdb07824a67e89fecc791b4f87f23db3`
- `build_client145_final.py` → `09c17dfefe536ee2bf10c2ef51ec2652bb2cba672ab98fe921ff2e00d02ccea6`

## Source-complete gate is intentionally NOT passed yet

Remaining gaps:

1. the pre-V12 integrated basis referenced by the historical V12 builder still needs source-level consolidation;
2. original `envcube_ext_v15_active_bind.c` plus the V15.5/V15.6 BSS/thread-state fixes still need to be recovered/merged into one clean V15.7 source unit;
3. the later integrated basis used by the exact Nexus materializer must become derivable from source rather than supplied as a binary basis;
4. only after deterministic compile/link/rebuild can the project-wide source-complete release gate pass.

Recovered files are immutable provenance snapshots. Future development should consolidate them into a clean canonical implementation rather than editing provenance files in place.

The exact recovered snapshot is stored by the recovery manifest/archive on this branch and can be reconstructed deterministically.
