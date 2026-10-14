# HemEnvLerp leather laminate — Fog / LightScattering operator review, 2026-10-14

## Answer to owner question: do Fog and LightScattering run in HemEnvLerp?

**YES** as shader consumers. Exact original owner R19 shader materialization V276 preserves **212-DWORD stock DSR Fog + LightScattering** instruction island, byte for byte, on RX33/34/35. The block resides after receiver material and EnvDiffuse/EnvSpec composition and before the target/color output. It is not a separate fullscreen postprocess prerequisite. Final RX33/34/35 shader Fog offsets are 2579 / 2498 / 2151, with RT1 Glow output original and independent; PTDE-vs-DSR pixel parity unproven.

Original stock DSR PBL family atmospheric operator is, in its matched positive unsaturated branch,

`X(scene linear) -> X^(1/2.2) -> Fog(cb12) -> LightScattering(cb13..20) -> output ^2.2`.

The PTDE matching family instead uses a legacy-domain Fog -> LightScattering operator without those DSR-specific gamma-domain conversions; PTDE Stable/Lerp source scattering is conditionally the same affine transformation at fixed atmosphere input (V298). These are **family-local** observations, not global Dark Souls shader guarantees. The 21-DWORD `LOG/MUL/EXP` stock post-LightScattering gamma unit was isolated exactly in V297 source; that older candidate removed it with 21 NOPs **without** material-specific PTDE pixel proof. Historical V313 integrated visual experiment reported multiple regressions and V315 returned to stock DSR post-LS behavior. Do **not** restore V297 blanket cut as leather laminate fix.

## Owner runtime data constraints

- V274B owner log 2026-10-14: 96/96 scoped RX33/34 CPU b0 writer shadows match physical bound b0 at sampled **PREPARED** Lerp requests; all report cb85 MaterialWorkflow 0, cb100w 1, cb102 0. Ordinary leather route 1073741869 n15 c101=1.5, cb10.rgb=(1,1,1); P_Metal route345 n20 c101=2.5, cb10.rgb=(1,1,1).
- V272 8/8 interior b12 beta routing observations show host beta reproduced for PTDE EnvSpec and EnvDiffuse. **No V274B sample observes b0[12] Fog or b0[13..20] scattering state, nor pixel preFog vs postFog vs postLS, nor actual GPU command-list final CB state.**
- Owner phenotype is positive stable A and stable B, anomalously shiny leather only with 0<beta<1; P_Metal not always defective. The stock atmosphere code **also remains in stable**. Consequently mere presence of Fog/LS is *not sufficient* to explain exclusive Lerp-only, material-selective gloss. Need either (a) distinct Lerp material-scene signal before Fog, (b) Lerp-only physical Fog/scattering producer state, or (c) nonlinear interaction of both. The DSR ^2.2 can magnify apparent absolute separation of a preexisting highlight under nonzero scattering source; a veil/milkiness is a different phenotype from directional plastic-like specularity.

## Narrow falsifier, no speculative production change

1. Bind exact physical owner tuple and same shader receiver variant for plain leather and P_Metal, confirm same actual resource identities, note full FLVER hash is currently zero/unresolved.
2. On **same draw**, trace `preFog scene.rgb` -> `postFog rgb` -> `postLightScattering rgb` -> `postGamma rgb`, including Fog/scattering b0[12..20] on **the matched deferred command-list execution order**; record beta.
3. If laminate residual exists at preFog, primary operator is material/normal/EnvSpec/R11 family, atmosphere can only *amplify* it. If first appears postFog, isolate Fog and its state/weight. If first postLS or only postGamma, isolate LightScattering or post-LS gamma respectively.
4. Compare against same physical A/B endpoints and PTDE target in matched domain; do not global-toggle Fog, LightScattering, postgamma or specular. Stable V272 and independent DSR systems remain frozen, unmatched draws fail open.
5. Before any bridge, verify actual Lerp vs stable shader owner (RX and full DXBC SHA), cb0[12..20] producer and temporal routing; prove downstream color residual and owner pixel improvement.

## Status

**Source Fog/LS existence and stock retention: CONFIRMED.** Conditional DSR atmosphere nonlinear contrast transfer: CONFIRMED for matched family, not site-specific. **Atmosphere causes owner leather laminate: HYPOTHESIS / OPEN.** No addon build or pixel patch in this review.
