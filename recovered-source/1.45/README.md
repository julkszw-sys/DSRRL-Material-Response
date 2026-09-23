# Material Response 1.45 — source recovery chain

Status: **PARTIAL SOURCE RECOVERY / CONSTRUCTION**  
Date: 2026-09-23

This directory preserves original development source files recovered from the DSRRL project Library and cross-verified against the source-artifact identities stored in Supabase.

## Target release identity

The current public/Nexus Material Response 1.45 addon is:

- file: `DSRRL_Material_Response_1.45.addon64`
- size: `1,803,264` bytes
- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

The user-supplied 2026-09-23 ZIP was independently unpacked during recovery and its addon payload matches this SHA-256 exactly.

## What is now recovered as original source

### V12 integrated asset bridge

`v12/asset_integrated_v12.c` is the original registered V12 source. Its SHA-256 matches Supabase artifact 1061 exactly:

`879b3ec251c36969cf7eabcc5b63164da7c17b6577e74b013e47f7866b3236e2`

It contains the integrated Normal / Diffuse / SpecRGB / Subsurf asset bridge extension and the corrected local receiver-ordinal routing used by the runtime-PASS V12 lineage.

The accompanying original files are also preserved:

- `v12/asset_stubs_v12.s`
- `v12/build_v12.py`
- `v12/audit_v12_independent.py`

### V15.7 / EnvSpec final activation delta

`v15_7/build_v15_7.py` is the original deterministic V15.7 builder, SHA-256:

`99f9108c2eea2b90632506d46d47ec4180ac657269970e6193f8a2a2305e151e`

The exact route map and static audit used by this lineage are retained next to it.

### Client 1.45 builders

The original final client builders are recovered unchanged:

- `client145/build_client145.py` — `e0fec5dca605f94ea0c532cff5e555d7fdb07824a67e89fecc791b4f87f23db3`
- `client145/build_client145_final.py` — `09c17dfefe536ee2bf10c2ef51ec2652bb2cba672ab98fe921ff2e00d02ccea6`

The repository root/review tooling contains the later exact Nexus-release materializers, including the final shipping cleanup and terminal-SAT delta.

## Important: this is not yet source-complete

Do **not** promote this recovery directory as a source-complete release build yet.

The remaining source-level gaps are:

1. the pre-V12 integrated basis used by `build_v12.py` (`V5.addon64`, `SAFE.addon64`, generated Subsurf payload inputs) is still represented by historical binary/generated inputs rather than one canonical source build;
2. the original registered EnvSpec source `envcube_ext_v15_active_bind.c` and the source-level implementation of the V15.5/V15.6 BSS/thread-state fixes still need to be recovered/merged into a clean V15.7 source unit;
3. the later integrated basis used by the exact Nexus release tooling must ultimately be derivable from the recovered source tree, not supplied as a binary basis;
4. deterministic compile/link/rebuild of the entire final `.addon64` from source must match the declared release behavior before the project-wide source-complete gate can pass.

Until then, this branch is a **recovery and reconstruction branch**, not a release branch.

## Recovery rule

Recovered files are treated as immutable provenance snapshots. New development should not edit these files in place. Source-level consolidation should happen in a separate canonical implementation directory, with explicit links back to these snapshots and their hashes.
