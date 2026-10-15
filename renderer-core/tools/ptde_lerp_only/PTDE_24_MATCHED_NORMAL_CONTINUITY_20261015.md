# Strict Lerp-only: original PTDE 24 matched Stable/Lerp PS3 source census (2026-10-15)

## Original evidence and scope
Owner source archive `DSRRL_FINAL_EVIDENCE_PART02(2).zip` SHA256 `57314b36812dfa2867512052278a08dc0da8a41ea0414c1fbbe4ccdf45838076`, subfolder `PTDE/PTDE_FRPG_PHN_EXTRACTED/dvdbnd1_dcx_0000631D9390`. Only **24 no-PointLight FRPG_Phn_DifSpc/DifSpcBmp PS3.0 shader families** with exact filename-homologous `HemEnv.fpo` ↔ `HemEnvLerp.fpo` are compared. Twelve family variants each. Names include ordinary, Mul, Lit, MulLit and Csd/Sdw. Not all possible DSR shader receiver families. Binaries never stored in repo.

## 24/24 original PTDE source findings
- Every matched original PTDE `HemEnv` and `HemEnvLerp` has **the exact same sequence of NRM instructions with all operand tokens unchanged**: 132 NRM occurrences across 24 Stable and 132 in Lerp.
- `RSQ` identical tokens and order for **108 occurrences** and `DP4` likewise **96 occurrences**; all 24 pairs preserve the entire original instruction-token sequence **before the first TEXLD**, excluding only declarations and constants.
- `DP3` tokens are identical in 16/24, differ in 8/24, so **do not** claim complete normal/reflection operator identity without live-register tracing.
- Sequence matching on exact instruction word tokens, excluding DCL/DEF, preserves **3,396 of 3,788** original Stable instructions as identical ordered matches (min 83.4395%, max 97.6923% by family). This is a structural, not pixel, comparison.
- Lerp adds exactly **two TEXLD opcodes** (24/24), **two DCLs** (24/24), plus a small family-specific set of MUL/MAD/DP3 etc. 20 matched pairs differ by +12 instructions total and 4 by +13; no NRM or RSQ added/removed. These are **opcode counts**, not a proof of exact probe endpoints or semantic isolation.
- In 124/132 NRM occurrences the **two instructions on either side** are also identical, while eight nearby contexts change because Lerp introduces or modifies sampling adjacent to a normalization. The unchanged NRM itself is not sufficient to prove every downstream normal/reflect liveout identical.

## Direct comparison with frozen DSRRL V272 materializers
Exact owner V272 source `ae9ac314d06ad8758e1aa7430cb236eef0eefe27`:
- Stable source `renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp` contains a verified named **R5** `apply_exact_ptde_normal_basis_r5`, explicitly restoring PTDE per-pixel tangent frame versus DSR `SV_IsFrontFace` normal flipping and separately interpolated `TEXCOORD5` bitangent. R5 performs an **upstream** semantic cut feeding both EnvDiffuse and reflection vector EnvSpec.
- Lerp source `renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp` invokes diffuse base + exact per-shader-source site + R19 material workflow and EnvSpec/EnvDiffuse source cuts + R6 terminal encoding, **but no named corresponding R5 normal-basis reconstruction** in this materializer.
- Source topology therefore poses a testable **transition-only operator asymmetry**: in the original PTDE 24-family shader set, normalization operators are preserved across Stable and Lerp; the current DSR host has dedicated stable R5 restoration, but lacks named Lerp R5 construction.
- **IMPORTANT:** no named call DOES NOT imply the shader has no mathematically equivalent normal-basis code. Exact actual Lerp DXBC input/output, other upstream bridge functions and the executed GPU shader must be checked before calling this a confirmed wrong normal. Physical pixel/screenshot proof is absent.

## Decisive next measurement, strict scope
Use exact owner-matched **P_Leather[DSB]** MTD SHA + FLVER/slot and RX33 Lerp vs stable RX34/RX33 under fixed geometry. Trace **live-out tangent-space N, T, B, normal map sample and geometric front-face orientation**, reflection coordinate `R=2*dot(N,V)*N-V`, EnvDiffuse input, EnvSpec t12 sample/LOD, preFog RGB. Compare original PTDE homologous consumer; do not transfer stable R5 by blind 43-DWORD patch. Contrasting same view, same material, good/bad pixels is needed to tie to Lambert/specular laminate. Stock DSR Fog and LightScattering must remain untouched.

## Existing controls / excluded candidates
Owner V276 at source level 35 exact MTD+FLVER+slot pairs: different host/planned PS objects in 35/35, but PTDE `t10` and material authored response unchanged; Lerp has stable object choices across 14 atmosphere buckets and no Lerp fail-open. Leather Alp/Edge missing PTDE t10/t12 in BOTH phases, thus no constant-gap Lerp-only explanation. No certified Cloth draw.

## Status
**CONFIRMED** original PTDE shader *instruction-level* NRM/RSQ/DP4 continuity for the 24 matched pairs. **CONFIRMED SOURCE** V272 Stable R5 call versus no same-named Lerp call. **HYPOTHESIS/OPEN** that Lerp actual N/T/B or reflection differs from PTDE or directly causes laminate. **RUNTIME GPU/PIXEL OPEN**. No renderer/EXE/CB/RT/shader changes authorized by this report.
