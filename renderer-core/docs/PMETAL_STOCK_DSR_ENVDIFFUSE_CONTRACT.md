# P_Metal stock DSR EnvDiffuse candidate

Status: **CONSTRUCTION CANDIDATE — runtime activation and pixel behavior OPEN**

## Intent and narrow semantic cut

This candidate is derived from the pinned V359-RC1 source lineage and preserves the current integrated addon features except the PTDE EnvDiffuse sidecar/consumer rewrite for the exact P_Metal receiver path.

For exact P_Metal routing only (route 345, verified P_Metal MTD identity, receivers 33–35):

- **EnvDiffuse:** retain the stock DSR shader instructions and currently bound native DSR resources/samplers at t11/t13. Do not load or bind PTDE EnvDiffuse sidecar resources, do not transport PTDE EnvDiffuse endpoints through b12[3..5], and do not claim EnvDiffuse resource/CB ownership in the draw transaction.
- **EnvSpec:** retain the existing PTDE EnvSpec sidecar at t12/t14 and its existing materializer path.
- **Material Response / SpecRGB:** retain exact material/owner gates and the existing P_Metal material response path.
- **HemEnvLerp:** retain the R19 MaterialWorkflow and terminal-SAT path while preserving the original stock EnvDiffuse block byte-for-byte; b12 remains 64 bytes rather than being extended solely for EnvDiffuse endpoints.
- **Other operators:** do not globally disable EnvDiffuse or change unrelated shader families, PointLight, Upper/Lower, Subsurface, fog/atmosphere or other independent paths. Existing fail-open behavior remains in force.

## Pipeline effect

At the EnvDiffuse semantic cut, the candidate leaves the DSR t11/t13 resource and consumer path untouched. The draw transaction continues to own only the operators/resources needed by the retained EnvSpec and material-response islands. If exact P_Metal receiver/material identity or the existing transaction preparation fails, the stock DSR draw remains the fallback.

## Important scope limit

“Stock DSR EnvDiffuse” here means **stock DSR EnvDiffuse consumer instructions and t11/t13 resources at the operator cut**, not a claim that the final EnvDiffuse pixel is bit-identical to unmodified vanilla DSR. The existing R5 normal-basis bridge is a shared input used by environment consumers, and existing R3/R11 changes remain downstream. They are intentionally preserved so this candidate changes only the EnvDiffuse carrier/consumer rewrite; separating the shared normal basis would require a separately verified operator split.

## Validation status

The source contract test and native Windows single-addon build establish construction and compile-time invariants only. Runtime liveness, receiver hits, bridge activation, and PTDE-visible pixel behavior are separate states. Pixel equivalence remains OPEN until existing project evidence can prove it; no user runtime capture is assumed or required for this construction step.
