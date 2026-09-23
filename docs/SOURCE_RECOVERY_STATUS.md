# Material Response 1.45 source recovery status

Canonical branch:

`recovery/material-response-1.45-source-chain-2026-09-23`

Current state: **PARTIAL SOURCE RECOVERY / CONSTRUCTION**.

## Byte-exact originals now in GitHub

Recovered originals include the V12 integrated runtime source/stubs/build tooling, the V15.7 builder, the final client-1.45 builders and the V12 EnvSpec slot map. Their historical SHA-256 values are recorded in the recovery manifest and the committed Git blob identities were checked against local `git hash-object` values.

The key recovered V12 implementation is:

`asset_integrated_v12.c`  
SHA-256 `879b3ec251c36969cf7eabcc5b63164da7c17b6577e74b013e47f7866b3236e2`

The exact shipping addon remains:

`e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`

## Remaining historical source gap

Supabase provenance retains file names, roles, sizes and SHA-256 identities for the missing EnvSpec chain, including V13.1, V14, V14.1, V14.2, V15, hardened V15.1 and V15.2 source files.

The historical runtime archives for V13.1, V14, V14.1, V14.2, V14.3, V15.1 and V15.2 were re-inspected. They contain binaries/audits/readmes but not the original C bodies, so those files are not claimed as recovered.

The source-level V15.5/V15.6 BSS/thread-state corrections leading into V15.7 are also still unresolved, and V12 itself still has binary-only predecessor inputs in its historical build chain.

## Consequence

Exact Nexus 1.45 binary reproduction exists, and a substantial part of the original implementation source is now preserved in GitHub, but a full source-to-release rebuild is not yet proven.

The project-wide release gate therefore remains closed for any new release derived from this incomplete historical chain until the missing layers are recovered or reconstructed and independently verified.
