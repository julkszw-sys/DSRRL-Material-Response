# P_Metal EnvSpec with stock DSR EnvDiffuse

This is a dedicated source variant derived from the canonical V359-RC1 active
development base. It retains the independently gated P_Metal EnvSpec bridge
and keeps the DSR EnvDiffuse consumer path native.

## Contract

- The source-owned variant header enables `DSRRL_PMETAL_STOCK_DSR_ENVDIFFUSE`.
- The PTDE EnvDiffuse LightBank donor is not loaded, validated, or required.
  Its absence must not make `pmetal_env_source_runtime::install()` fail or
  gate publication of an otherwise valid EnvSpec source.
- Single/blend EnvSpec source publication does not depend on EnvDiffuse capture
  or validation.
- The EnvDiffuse linear carrier is marked invalid; default zero values are
  not advertised as recovered PTDE data.
- The shader materializer skips both the exact PTDE EnvDiffuse replacement
  and the legacy linear-EnvDiffuse translation.
- The integrated addon does not report the full PTDE EnvDiffuse island active.

## Remaining fail-open conditions

This removes only the **EnvDiffuse-donor prerequisite**. Independent EnvSpec
fail-open gates remain intact: missing/invalid PTDE EnvSpec sidecar, unknown
shader/material identity, invalid source selectors, failed shader
postconditions, and failed replacement registration must still preserve stock
DSR for that route. No global fail-open bypass is introduced.

Other independent operator islands retain their own routing and postconditions.
This variant is not a claim of runtime activation or PTDE pixel equivalence.

## Validation

Run `python renderer-core/tools/audit_stock_dsr_envdiffuse_variant.py` from
the repository root. It checks source contracts only. A native integrated
`.addon64` build, runtime liveness/activation checks, and PTDE pixel comparison
remain required before release.
