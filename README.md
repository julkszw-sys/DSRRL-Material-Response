# DSRRL Material Response

Renderer/material bridge source and reproducibility material for **Dark Souls Remastered – Restored Lighting**.

DSR remains the host renderer. Bridges are scoped to verified renderer/material/resource routes and unsupported or unidentified routes fail open to stock DSR.

## Current public release

**Material Response 1.45**

`DSRRL_Material_Response_1.45.addon64`

- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`
- size: `1,803,264 bytes`
- PE checksum: `0x001BC666`
- version: `1.45.0.0`

Exact Nexus archive:

- SHA-256: `2439d24644a0dc5bef6e29b1d980bab5b421a08c6bc71fca93d4c88859ad0893`
- size: `244,154 bytes`

The release intentionally ships with **PTDE EnvSpec/PackedGI replacement disabled**. The final release delta adds terminal RGB saturation to embedded DXBC 33/34/35; alpha is unchanged.

See `RELEASE_HASHES.md` and `BUILD.md`.

## Repository lanes

- `main` — canonical release-facing metadata, reproducibility docs and stable review material.
- `develop` — integration lane for work that is not yet a release.
- `nexus-review/material-response-1.45-source` — exact Nexus 1.45 binary-review/reproduction lane.
- `recovery/material-response-1.45-source-chain-2026-09-23` — canonical historical source-recovery lane for 1.45.
- `dev/material-response-expanded-a3-monolith` — source-completeness experimental lane; not the shipping 1.45 source.
- other `dev/*`, `pmetal-*`, `resource-routing-*` and legacy recovery branches — diagnostics/history unless explicitly promoted.

See `docs/REPOSITORY_MAP.md` and `docs/BRANCH_POLICY.md`.

## Source recovery status

Original V12 runtime source/stubs/build tooling and the final V15.7 / client-1.45 builders have been recovered byte-for-byte and committed on the canonical recovery branch.

The historical source chain is **not yet source-complete**. V15.4→V15.6 binary deltas have now been independently reconstructed and reproduce the preserved binaries byte-for-byte, but they are explicitly `RECONSTRUCTED`, not recovered originals. The principal remaining handwritten-source gap is the EnvSpec implementation around V15/V15.1 (`envcube_ext_v15_active_bind.c`) and earlier source-to-binary lineage, plus binary-only predecessor inputs before V12.

See `docs/SOURCE_RECOVERY_STATUS.md`.

## Mandatory release rule

No new Renderer Edition / Material Response runtime may be promoted to RC or RELEASE unless the exact handwritten source, generators, immutable external-input hashes, deterministic build path, build audit, source commit SHA and parent/rollback lineage are committed.

A previous addon binary may be retained as historical evidence or compatibility input, but it is not accepted as the canonical implementation of a new release.

## Validation levels

Construction, compatibility, runtime liveness, bridge activation and PTDE-visible pixel behavior are separate statuses. A successful build does not by itself prove renderer equivalence.
