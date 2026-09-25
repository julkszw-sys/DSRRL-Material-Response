# DSRRL Core+Islands

Renderer/material bridge source and reproducibility material for **Dark Souls Remastered – Restored Lighting**.

DSR remains the host renderer. Bridges are scoped to verified renderer/material/resource routes and unsupported or unidentified routes fail open to stock DSR.

## Active development line

**DSRRL Core+Islands 2.0.0-dev**

`main` is the canonical active development/integration line for Renderer Core + operator islands and the eventual single `.addon64`. The machine-readable version authority is `renderer-core/integrated/VERSION.json`.

Core+Islands has its own version namespace. It must not be called Material Response 1.45, and new development must not extend the 1.45 monolith version number.

Construction, compatibility, runtime liveness, bridge activation and PTDE-visible pixel behavior are separate statuses.

## Frozen legacy monolith

**Material Response Monolith 1.45 — FROZEN LEGACY**

`DSRRL_Material_Response_1.45.addon64`

- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`
- size: `1,803,264 bytes`
- PE checksum: `0x001BC666`
- version: `1.45.0.0`

Exact Nexus archive:

- SHA-256: `2439d24644a0dc5bef6e29b1d980bab5b421a08c6bc71fca93d4c88859ad0893`
- size: `244,154 bytes`

1.45 is retained for historical reproduction, compatibility comparison and provenance only. It is **not the active development line** and must not be reported as the current Core+Islands version. The historical release intentionally ships with **PTDE EnvSpec/PackedGI replacement disabled**.

See `RELEASE_HASHES.md` and `BUILD.md`.

## Repository lanes

- `main` — canonical **Core+Islands 2.0.0-dev** active development/integration line.
- `develop` — optional staging lane; it is not authoritative over a newer accepted `main`.
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
