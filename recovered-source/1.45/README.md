# Material Response 1.45 — source recovery chain

Status: **PARTIAL SOURCE RECOVERY / CONSTRUCTION**  
Date: 2026-09-23

This branch starts from the exact Nexus 1.45 review branch and is the canonical recovery workspace for rebuilding the release from source instead of treating a previous `.addon64` as canonical input.

## Exact shipping release identity

- file: `DSRRL_Material_Response_1.45.addon64`
- size: `1,803,264` bytes
- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

The 2026-09-23 owner upload was unpacked independently and its addon payload matches this shipping SHA-256 exactly.

## Exact original development files recovered

The immutable recovery snapshot contains original files whose hashes match the historical Supabase provenance:

- `asset_integrated_v12.c` → `879b3ec251c36969cf7eabcc5b63164da7c17b6577e74b013e47f7866b3236e2`
- `asset_stubs_v12.s` → `c7e8634e3fd37730f8ee052c2cdc763448dfd408cd023492a6d472ab285c89a9`
- `build_v12.py` → `c330b710ed4cede81dcb873f6ad56f7ef798c31f667e3d61bf90edcc412061a3`
- `audit_v12_independent.py` → `f6e10a914e5313e1cc28501bcde6ec1783a8f6c5ab03ef64c9024330430f9f2c`
- `build_v15_7.py` → `99f9108c2eea2b90632506d46d47ec4180ac657269970e6193f8a2a2305e151e`
- `build_client145.py` → `e0fec5dca605f94ea0c532cff5e555d7fdb07824a67e89fecc791b4f87f23db3`
- `build_client145_final.py` → `09c17dfefe536ee2bf10c2ef51ec2652bb2cba672ab98fe921ff2e00d02ccea6`

The exact recovery archive is `DSRRL_Material_Response_1.45_SOURCE_RECOVERY_2026-09-23.zip`, SHA-256 `56c2bc30b4ecc791495a82a9dd214d9d619411a40c204ed26222741778c54546`.

## GitHub text-copy warning

The currently browsable `v15_7/build_v15_7.py` and `client145/*.py` files on this branch were transferred through a text-only connector and are **normalized convenience copies**, not the immutable provenance objects. Their historical SHA-256 values above refer to the exact files in the recovery snapshot.

Do not use a mismatch between those convenience copies and the provenance hashes as evidence of changed historical behavior. Exact source ingestion is being materialized separately and must be hash-verified before the source-complete gate can pass.

## Source-complete gate is intentionally NOT passed

Remaining gaps:

1. materialize the exact V12 source/stubs/build tools as hash-verifiable repository objects;
2. consolidate the pre-V12 integrated basis so V12 is built from source rather than from `V5.addon64` / `SAFE.addon64`;
3. recover or reconstruct the original `envcube_ext_v15_active_bind.c` and fold the V15.5/V15.6 BSS/thread-state fixes into a clean source-level V15.7 implementation;
4. make the final Nexus 1.45 materializer derive its integrated basis from source instead of a previous binary;
5. deterministic rebuild + binary/semantic audit before any source-complete promotion.

Recovered provenance files are immutable. New development should happen in a clean canonical implementation layered above this recovery tree, not by silently editing provenance snapshots.
