# Material Response 1.45 -> Renderer Core 151 reuse matrix

Basis: current main after PR #33. Historical implementation source: `recovery/material-response-1.45-source-chain-2026-09-23`.

## Rule

1.45 is an implementation/provenance bank, not a binary donor to patch into Core wholesale.
Port the narrow operator semantics, exact route tables and verified resource transactions into the single current Core owner.
Do not reinstall historical parallel hooks when current Core already owns the same producer/draw cut.
All unsupported identity/routing remains fail-open to stock DSR.

## Reusable islands

### P_Metal black-safe source-gain / A-B path

Historical authority:
- `pre_v12/mr10_v14/SOURCE_V13_PTDE_DONOR.cpp`
- owner runtime canonical V13 PASS
- clean/public 1.45 lineage preserves the V13-style P_Metal source path

Recovered semantics:
- retail steady producer: RVA 0x563B80
- stable: publish A=A, beta=0
- blend: publish PTDE-authored A/B endpoints plus beta
- historical carrier: b12[2]=A, b12[3]=B with beta in b12[3].w
- P_Metal EnvMap source intentionally excludes DSR g_draw / gFC_EnvSpcMapMulCol and LightProbeParam.x
- native DSR cubemap resources may remain; this visible black-safe fix is not equivalent to EnvSpec resource replacement

Core 151 migration:
- reuse current central producer/hook ownership; do not add a second V13 hook module
- add PTDE EnvSpec A/B semantic payload to the existing draw selection snapshot
- extend the existing MR b12 materialization for exact P_Metal route only
- materialize a dedicated P_Metal receiver variant from the historical V13 semantic DXBC transform
- preserve current SpecRGB t10, Diffuse t0 and Normal t2 transactions
- fail-open for non-P_Metal and on any producer/selector mismatch

### HemEnvLerp coverage

Historical authority:
- V10/V13 owner-accepted blackspot + transition lineage
- V29 provenance contains 24 HemEnvLerp hosts
- current V2.10/V2.11 runtime generator intentionally has 0 HemEnvLerp replacements

Core 151 migration:
- recover the exact Lerp receiver transformations from 1.45 lineage before activation
- no generic "reuse stable HemEnv shader" substitution
- preserve A/B endpoint order and beta semantics
- add independent LERP create/init/bind/replay telemetry

### SpecRGB / Diffuse / Normal

Historical authority:
- `v12/asset_integrated_v12.c`
- `v12/audit_v12_independent.py`
- pre-V12 FULL24 SpecRGB exact chain

Already ported/current:
- SpecRGB exact t10 split with stock t1 preserved
- Diffuse t0 exact V12 pair gate
- Normal t2 exact V12 tuple gate

Action:
- keep current Core implementation; use 1.45 only for regression/provenance checks.

### Subsurface

Historical authority:
- pre-V12 build52 preserved original transformer/builder
- V12 integrated runtime carries Subsurf resource handling

Current status:
- Core construction is exact-route/body-only but build150 observed zero runtime replay.

Action:
- compare current body gate/receiver identity with build52/V12 exact routing before changing shader math.

### EnvSpec exact-slot resource transport

Historical authority:
- V15.1 -> V15.7 exact recovery chain
- final client 1.45 preserves exact 368 material-slot mapping, per-thread A/B semantics, GPU identity two-hit and t12/t14 save-bind-restore
- V15.7 runtime basis PASS

Action:
- do not globally enable as a black-armor fix
- reuse exact slot/resource transaction only for the separate EnvSpec resource island when its receiver/operator is opened
- keep P_Metal black-safe source-gain island logically separate

### no-Spc EnvSpec delete

Current canonical target:
- EnvSpec_term := 0 only on the certified substantive PTDE no-Spc homologs

Current Core gap:
- materialized A1 index covers only a subset and build150 observed no no-Spc bind.

Action:
- recover/materialize the full exact receiver/material census.
- do not infer no-Spc from texture/material name or globally suppress EnvSpec.

## Core 151 order of work

1. Port V13 P_Metal A/B source-gain semantics into current central runtime ownership.
2. Recover exact historical HemEnvLerp transforms and add Lerp telemetry.
3. Complete no-Spc exact census/materialization.
4. Revalidate Subsurface routing against 1.45 build52/V12.
5. Leave current MR/SpecRGB/Diffuse/Normal math unchanged unless exact 1.45 regression evidence falsifies it.
6. Build one reproducible addon and validate runtime/activation/pixel as separate levels.
