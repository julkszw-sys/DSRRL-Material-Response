# Repository map

This file defines which branches are authoritative for which job. It is intentionally non-destructive: historical branches are retained as evidence and rollback points.

## Canonical lanes

**`main`**  
Canonical active development/integration line for **DSRRL Core+Islands 2.0.0-dev**. Renderer Core + operator-island work belongs here once evidence and source completeness support it. Material Response 1.45 is not the version identity of `main`.

**`develop`**  
Optional staging lane. It may hold preparatory integration work, but it is not authoritative over a newer accepted `main`.

**`nexus-review/material-response-1.45-source`**  
Exact frozen Nexus 1.45 monolith reproduction/review lane. It is historical/compatibility provenance and is not an active development lane.

**`recovery/material-response-1.45-source-chain-2026-09-23`**  
Canonical historical source-recovery lane. Recovered originals are immutable provenance objects. This is the only 1.45 recovery branch that should receive new recovery work.

**`dev/material-response-expanded-a3-monolith`**  
Canonical Expanded A3 experiment lane. It is not a substitute for the missing historical 1.45 source chain and is not the shipping release source. Its source-completeness guard now treats shipping 1.45 only as a compatibility oracle and intentionally fails until a committed monolithic current implementation + build audit exist.

## Frozen/legacy duplicates

The following branch classes remain for history but should not receive new canonical work:

- `recovery/material-response-1.45-original-source-2026-09-23`
- `source-recovery/material-response-1.45`
- `dev/material-response-expanded-a3-monolithic`
- dated `pmetal-*` copy/docs/final branches
- dated `resource-routing-*` diagnostic branches
- old `dev/runtime-core-v2-*` experiment snapshots once superseded by a newer explicitly documented branch

Do not delete them to make history cleaner. Supersession is documented, not erased.

## New branch naming

Use:

- `release/<version>` only for source-complete release candidates;
- `recovery/<topic>` for provenance/source recovery;
- `dev/<operator>/<topic>` for renderer/operator development;
- `diag/<operator>/<topic>` for deliberately diagnostic-only work where practical;
- `hotfix/<version>/<topic>` only from an identified release basis.

Existing legacy names are retained, but new work should follow this structure.

## Promotion

Promotion is documentation + evidence + source-completeness, not a branch rename. A branch may only become RC/release when the repository contains the exact source, generators, external input identities, deterministic build path, audit and lineage required by `docs/BRANCH_POLICY.md`.
