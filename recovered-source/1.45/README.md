# Material Response 1.45 — source recovery chain

Status: **PARTIAL SOURCE RECOVERY / CONSTRUCTION**  
Date: 2026-09-23

This branch starts from the exact Nexus 1.45 review branch and is the canonical recovery workspace for rebuilding the release from source instead of treating a previous `.addon64` as canonical implementation.

## Exact shipping release identity

- file: `DSRRL_Material_Response_1.45.addon64`
- size: `1,803,264` bytes
- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

The 2026-09-23 owner upload was unpacked independently and its addon payload matches this shipping SHA-256 exactly.

## Exact original development files recovered and committed

The following historical originals were recovered from owner-side archival packages and their SHA-256 values match the historical Supabase provenance:

- `v12/asset_integrated_v12.c` → `879b3ec251c36969cf7eabcc5b63164da7c17b6577e74b013e47f7866b3236e2`
- `v12/asset_stubs_v12.s` → `c7e8634e3fd37730f8ee052c2cdc763448dfd408cd023492a6d472ab285c89a9`
- `v12/build_v12.py` → `c330b710ed4cede81dcb873f6ad56f7ef798c31f667e3d61bf90edcc412061a3`
- `v12/audit_v12_independent.py` → `f6e10a914e5313e1cc28501bcde6ec1783a8f6c5ab03ef64c9024330430f9f2c`
- `v15_7/build_v15_7.py` → `99f9108c2eea2b90632506d46d47ec4180ac657269970e6193f8a2a2305e151e`
- `client145/build_client145.py` → `e0fec5dca605f94ea0c532cff5e555d7fdb07824a67e89fecc791b4f87f23db3`
- `client145/build_client145_final.py` → `09c17dfefe536ee2bf10c2ef51ec2652bb2cba672ab98fe921ff2e00d02ccea6`
- `v15_7/V12_ROUTE_ENVSPC_SLOT_MAP_COMPLETE.json` → `046fcbc32781f37475fb9a101b0483bb2fb4b969bf14438201a128c01b45440f`

These repository objects were additionally checked against local `git hash-object` identities, so the committed text files preserve the recovered bytes rather than being normalized convenience copies.

The immutable recovery archive is `DSRRL_Material_Response_1.45_SOURCE_RECOVERY_2026-09-23.zip`, SHA-256 `56c2bc30b4ecc791495a82a9dd214d9d619411a40c204ed26222741778c54546`.

## Historical EnvSpec source gap

Supabase provenance records original source identities for the missing EnvSpec development chain, including:

- V13.1 `envprov_ext_v13_1.c` — `ee341afc580f623eefb7f21138d17c4659b6dc36b931ce794dfc9e671f8e4c1d`
- V14 `envcube_ext_v14.c` — `dc1db91a5df4b459ac7883e761068ce18ae1dfcbdcb292f71f74bdf23841ed2c`
- V14.1 `envdev_ext_v14_1.c` — `4483d69c8554766d9ad7d0b56c6c96673372ca27cd5d2d81fbc94f86adef641d`
- V14.2 `envcube_ext_v14_2.c` — `89074e10eb0aa3eab4adecd4221ad8a0c8e322176b7a7fdd5845ae59b810efd7`
- V15 `envcube_ext_v15_active_bind.c` — `5d45ed33c6e1a0c062ce28375212d17d34d218e14aedcc0dfc364caa3444c4af`
- V15.1 hardened `envcube_ext_v15_active_bind.c` — `627a6f7c5a977f80f1b109e6cc03218bc9db97ca54ad045028bbc41e2b412a49`
- V15.2 `envcube_ext_v15_2_callsite.c` — `bb6a522b8e333a811bf7bcd414e15d583e8b12f899a69afc8c81b09c2032f3ec`

Historical runtime ZIPs V13.1, V14, V14.1, V14.2, V14.3, V15.1 and V15.2 were re-inspected. They contain binaries/audits/readmes but not these original C files. Their provenance is therefore retained, but the C bodies are not falsely claimed as recovered. Separately, exact preserved V15.3/V15.4/V15.5/V15.6 binaries were differentially audited; reconstructed builders now reproduce V15.4, V15.5 and V15.6 byte-for-byte from their exact parents.

## Source-complete gate is intentionally NOT passed

Remaining blockers:

1. recover or reconstruct and independently verify the missing handwritten EnvSpec source-level chain, especially the V15/V15.1 `envcube_ext_v15_active_bind.c` lineage and earlier producers; V15.4→V15.6 binary deltas are now independently reconstructed byte-exact under `reconstructed/1.45/v15_4_to_v15_6/` but are not claimed as recovered original source;
2. consolidate the pre-V12 integrated basis so V12 no longer depends on historical binary-only predecessor inputs;
3. make the final Nexus 1.45 materializer derive its integrated basis from committed source rather than a previous addon binary;
4. deterministic rebuild + binary/semantic audit before any source-complete promotion.

Recovered provenance files are immutable. New development belongs above this recovery tree; do not silently rewrite historical originals.
