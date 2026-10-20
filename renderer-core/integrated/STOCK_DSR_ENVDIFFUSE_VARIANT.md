# P_Metal EnvSpec with stock DSR EnvDiffuse

This branch is a dedicated source variant. It deliberately keeps the stock DSR
EnvDiffuse consumer while retaining the independent P_Metal EnvSpec bridge.

## Contract

- The source-owned header
  `include/dsrrl/runtime/pmetal_stock_dsr_envdiffuse_variant.hpp` enables
  `DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE` for this variant.
- The PTDE EnvDiffuse LightBank donor is not loaded, validated, or required.
  Absence of that donor must not make `pmetal_env_source_runtime::install()`
  fail and must not gate publication of an otherwise valid EnvSpec source.
- EnvDiffuse capture does not participate in EnvSpec source validity.
- The shader materializer skips both the exact PTDE EnvDiffuse replacement and
  the legacy linear-EnvDiffuse translation. The DSR EnvDiffuse consumer/resource
  path is not rewritten by those routines.
- The EnvDiffuse linear carrier is marked invalid so another consumer cannot
  mistake a zero/default payload for recovered PTDE EnvDiffuse.
- The integrated addon does not emit the misleading full-PTDE-EnvDiffuse
  create-time log for this variant.

## Remaining fail-open conditions

This change removes only the **EnvDiffuse-donor prerequisite**. It does not
disable the independent fail-open guards for EnvSpec: missing/invalid PTDE
EnvSpec sidecar, unknown shader/material identity, invalid source selectors,
failed shader postconditions, or failed replacement registration must still
leave that route stock DSR. Those are separate gates and must not be bypassed.

Other independently compiled operator islands remain governed by their existing
identity, routing, and postcondition guards. This source variant is not a claim
that every other P_Metal operator is pixel-equivalent to PTDE.

## Audit and status

Run `python renderer-core/tools/audit_stock_dsr_envdiffuse_variant.py` from
the repository root. The audit checks source contracts only. It is not a
compiler/build test, runtime-liveness test, bridge-activation test, or pixel
equivalence test.

**Current status:** source variant prepared; native .addon64 build and runtime
validation are still required before this can be called a deployable variant.
The existing v2.3.0 addon is not modified in place.
