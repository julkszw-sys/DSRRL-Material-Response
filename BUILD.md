# Building the exact Material Response 1.45 Nexus release

## Current exact release

`DSRRL_Material_Response_1.45.addon64`

- size: `1,803,264 bytes`
- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`
- PE checksum: `0x001BC666`

PTDE EnvSpec/PackedGI replacement is intentionally disabled in this shipping release.

## Exact historical reproduction path

The exact release can currently be materialized from the integrated 1.45 basis:

`db2e6547b5fb5516d4ad6559173e66421315d1635ca0c08e8141b0de2f57f966`

Stage 1 produces the telemetry-clean, EnvSpec-disabled intermediate:

`3dcb50bee7d4a1ffcb47c2e9d116cbad5da322f6719e3cf2ecdbe6846db63000`

Stage 2 changes only the terminal RGB write in embedded DXBC 33/34/35 from `MOV` to `MOV_SAT`, recomputes the affected DXBC checksums and PE checksum, and produces the exact Nexus release `e44183...`.

Use the exact reproduction tools maintained on:

`nexus-review/material-response-1.45-source`

The canonical historical-source recovery is maintained on:

`recovery/material-response-1.45-source-chain-2026-09-23`

## Source-completeness status

This exact historical reproduction path is **not** equivalent to a source-complete rebuild because the integrated basis is itself a prior addon binary.

Therefore:

- exact binary reproduction: available;
- recovered original V12/client build source: partially available and hash-verified;
- full source-to-release rebuild: still open;
- new RC/release promotion from this lineage: blocked until the source-complete gate passes.

See `docs/SOURCE_RECOVERY_STATUS.md`.

## Validation scope

Exact output hash proves construction/reproduction only. Runtime liveness, bridge activation, renderer routing and PTDE-visible pixel behavior require separate evidence.
