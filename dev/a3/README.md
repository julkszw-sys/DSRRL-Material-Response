# DSRRL Material Response Expanded A3 — Monolithic development branch

This branch is the authoritative development lineage for the post-1.45 Material Response / Renderer Edition runtime.

## Goal

Produce one installable `.addon64` successor containing:

- exact Material Response 1.45 behavior as the legacy basis;
- Expanded Material Response closed operator coverage from A1;
- PntS / PntSS / PntSSSS receiver census from A2;
- PTDE Upper/Lower runtime bridge using the verified draw-local transport;
- future EnvDiffuse bridge only after its resource/assignment routing is verified.

The target remains PTDE-visible behavior on the DSR host. EnvSpec resource replacement is not enabled by this A3 work.

## Source preservation rule

From A3 onward, a runtime build is not considered reproducible unless this branch contains, before release promotion:

1. all handwritten source used by the build;
2. every generator used to create generated headers/payload maps;
3. exact input SHA-256 identities and provenance;
4. generated-plan/audit manifests or enough deterministic data to regenerate them;
5. build commands/toolchain versions;
6. final binary SHA-256 and a construction audit;
7. a clear statement of any opaque legacy binary basis.

Do not make a binary-only production change and document it later.

## Important legacy 1.45 limitation

The public `nexus-review/material-response-1.45-source` branch reproduces the shipping 1.45 file from an exact *integrated binary basis*. The full historical source tree that produced that integrated basis was not preserved as a complete reproducible source build.

This is a known legacy provenance gap. It must not be repeated.

For A3 development, shipping 1.45 is treated as an immutable legacy basis with exact identity:

- SHA-256: `e44183ef10fc7921f7741eb16d54ec30e814f580421ac6c83be18b5308b73342`
- size: `1803264`

Any monolithic integration step applied to that basis must itself be fully represented in source on this branch and independently auditable.

## Current A3 design

- one final addon on disk;
- no separate A1/A2/U-L companion required in the final form;
- A1 closed-operator materialization is retained with persistent per-plan storage;
- A2 receiver identity remains telemetry only;
- shared `FRPG_Phn_DifSpc*` visible patches remain protected by exact-material gating;
- U/L uses the verified PTDE transport `b13[6].xyz / b13[7].xyz`;
- P_Metal U/L payloads are derived from the shipping 1.45 P_Metal alternatives, not from a vanilla/P2.2 rollback;
- EnvDiffuse and PTDE EnvSpec resource replacement are OFF in the first A3 U/L runtime.

## Development inputs

See `dev/a3/SOURCE_MANIFEST.md` and the source folders in this directory.
