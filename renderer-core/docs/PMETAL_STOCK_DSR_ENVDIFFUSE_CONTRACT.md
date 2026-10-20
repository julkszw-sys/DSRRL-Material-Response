# P_Metal stock DSR EnvDiffuse candidate

Status: **CONSTRUCTION CANDIDATE — runtime activation and pixel behavior OPEN**

## Intent and receiver selection

This candidate is derived from the pinned V359-RC1 source lineage. It builds two create-time shader payloads from the same verified receiver shader:

- **Exact P_Metal** — route 345, authenticated P_Metal MTD identity, receivers 33–35: stock DSR EnvDiffuse consumer instructions and native t11/t13 resources/samplers remain untouched.
- **Verified nonmetal aliases** — the existing V285/V289 exact FLVER/slot/MTD routes: retain the default PTDE EnvDiffuse sidecar path, including the current R19 Lerp A/B+beta operator and the nonmetal R8 SpecRGB branch.

The runtime selects the stock payload only after the exact P_Metal material and route decision both pass. If the stock payload is absent or any identity/receiver/resource/transaction check fails, that draw fails open to stock DSR. Nonmetal aliases do not inherit the stock variant by shader-family resemblance alone.

## Operator changes

- **EnvDiffuse on exact P_Metal:** no PTDE EnvDiffuse consumer rewrite; no PTDE EnvDiffuse SRV/sampler binding at t11/t13; no EnvDiffuse resource or transaction ownership. Stable b12 stays 64 bytes. HemEnvLerp retains the original t11/t13 A/B+beta block byte-for-byte and keeps b12 at 64 bytes rather than extending it solely for EnvDiffuse endpoints.
- **EnvSpec:** keep the existing PTDE EnvSpec sidecar at t12/t14 and the existing source A/B+beta bridge.
- **Material Response / SpecRGB / MaterialWorkflow / terminal SAT:** preserve the existing exact material/owner gates and cumulative operator islands.
- **Other operators:** keep R3/R5/R6/R7/R11 and the existing independent PointLight, Upper/Lower, Subsurface and atmosphere/fog behavior unchanged. No global renderer rollback is introduced.

The exact PTDE EnvDiffuse donor is still attempted so the verified nonmetal aliases can keep their current sidecar path. If that donor is missing or invalid, the stock P_Metal path may still consume the valid EnvSpec source, while any draw that requires PTDE EnvDiffuse is rejected/fails open through its own source-validity gate.

## Pipeline and remaining semantic limit

At the P_Metal EnvDiffuse semantic cut, the candidate leaves DSR's t11/t13 consumer instructions, resources and samplers unchanged. It does **not** claim that the final EnvDiffuse pixel is bit-identical to vanilla DSR: the shared R5 normal-basis bridge remains an input to the environment consumers, and existing R3/R11 changes remain downstream. Those operators are intentionally retained to preserve the rest of the mod; splitting the shared normal basis so stock EnvDiffuse also receives the unmodified DSR normal would be a separate shader-operator change requiring its own exact census and proof.

## Validation status

The source-contract test and native Windows single-addon build establish construction and compile-time invariants only. Runtime liveness, receiver hits, bridge activation, and PTDE-visible pixel behavior are separate states. Pixel equivalence remains OPEN until existing project evidence can prove it; no new user runtime capture is assumed or required for this construction step.
